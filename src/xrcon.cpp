#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define BUFFER_SIZE 4096

#define GREEN "\e[0;92m"
#define YELLOW "\e[0;93m"
#define RED "\e[0;91m"
#define RST "\e[0m"

#define SERVERDATA_AUTH 3
#define SERVERDATA_AUTH_RESPONSE 2
#define SERVERDATA_EXECCOMMAND 2
#define SERVERDATA_RESPONSE_VALUE 0

void int2LE(int offset, unsigned char* packet, int value)
{
	for(unsigned short i=0;i<sizeof(value);++i)
	{
		packet[offset + i] = value;
		value >>= 8;
	}
}

void buildPacket(unsigned char* packet, int& realPacketSz, int id, int type, std::string text)
{
	const int szOffset = 0, idOffset = 4, typeOffset = 8, textOffset = 12;
	int packetSize = text.size() + 8 + 1;
	realPacketSz = packetSize + 4;

	int2LE(szOffset,packet,packetSize);
	int2LE(idOffset,packet,id);
	int2LE(typeOffset,packet,type);

	for(unsigned short i=0;i<text.size();++i) packet[textOffset + i] = text[i];

	packet[realPacketSz - 1] = 0x00;
}

int idxOf(char value)
{
	if(value <= '9') return value - '0';
	else if(value > 'a' && value < 'k') return value - 'a' + 10;
	else return value - 'k' + 16;
}

static const char* colors[] = {
	"\e[0;30m", // BLACK
	"\e[0;34m", // DARK_BLUE
	"\e[0;32m", // DARK_GREEN
	"\e[0;36m", // DARK_AQUA
	"\e[0;31m", // DARK_RED
	"\e[0;35m", // DARK_PURPLE
	"\e[0;33m", // GOLD
	"\e[0;37m", // GRAY
	"\e[0;90m", // DARK_GRAY
	"\e[0;94m", // BLUE
	"\e[0;92m", // GREEN
	"\e[0;96m", // AQUA
	"\e[0;91m", // RED
	"\e[0;95m", // LIGHT_PURPLE
	"\e[0;93m", // YELLOW
	"\e[0;97m", // WHITE
	"\e[8m", // OBFUSCATED
	"\e[1m", // BOLD
	"\e[9m", // STRIKETHROUGH
	"\e[4m", // UNDERLINE
	"\e[3m", // ITALIC
	"\e[0m", // RESET
};

void printPacket(int& realPacketSize,unsigned char* buff, bool& hasClr)
{
	for(int i=12;buff[i];++i)
	{
		if(buff[i] == 194 && buff[i+1] == 167) // IF SPECIAL CHARACTER CATCHED (Minecraft only for color codes)
		{
			printf("%s", colors[idxOf(buff[i+2])]);
			i+=2;
			hasClr = 1;
		}
		else printf("%c", buff[i]);
	}

	if(hasClr) printf("%s", "\n" RST); // New Line & Console Color Sequense Reset
}

int main()
{
	unsigned char packet[BUFFER_SIZE], buffer[BUFFER_SIZE];
	std::string IPAddr, pass;
	struct sockaddr_in serv_addr;
	int port=0, realPackSz=0, client_fd=0, carry=0;
	bool isSource=0, hasColor=0;
	
	do
	{
		printf("Enter IP: "); std::cin >> IPAddr;

		printf("Enter Port: "); std::cin >> port;

		if((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) // Create Socket
		{
			printf(RED "Socket Creation Error \n" RST);
			return -1;
		}
		else printf(GREEN "Socket Created!\n" RST);

		serv_addr.sin_family = AF_INET; // Set Address Family
		serv_addr.sin_port = htons(port); // Set Dest Port

		if(inet_pton(AF_INET, IPAddr.c_str(), &serv_addr.sin_addr) <= 0) // Convert All Data To Byte Format
		{
			printf(RED "Invalid Address / Address Not Supported \n" RST);
		}
	}
	while(inet_pton(AF_INET, IPAddr.c_str(), &serv_addr.sin_addr) <= 0);

	while(connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) // Connecting To Server
	{
		printf(RED "Connection Failed \n" RST);
		printf(YELLOW "Reconnecting...\n" RST);
		sleep(5);
	}

	printf(GREEN "Connected!\n" RST);

	do
	{
		printf("Enter PASS: "); std::cin >> pass;

		std::cin.ignore(256,'\n'); // Fix error when getline go throw without waiting any input

		buildPacket(packet, realPackSz, 0, SERVERDATA_AUTH, pass.c_str()); // Build Request Auth Packet

		send(client_fd, packet, realPackSz, 0); // Send Auth Packet
		recv(client_fd, buffer, BUFFER_SIZE,0); // Get Reponse From Server

		if(buffer[8] != 2)
		{
			isSource = 1;
			recv(client_fd, buffer, BUFFER_SIZE,0); // Get Reponse From Server
		}

		if(buffer[4]) printf(RED "Authentication Denied!\n" RST);
	}
	while(buffer[4]);

	printf(GREEN "Authentication Successful!\n" RST);

	if(isSource) printf(YELLOW "Sorce RCON Server Detected!\n" RST);
	else printf(YELLOW "Minecraft RCON Server Detected!\n" RST);

	for(;;)
	{
		printf(">>> ");

		std::string command;
		std::getline(std::cin, command); // Read command

		if(command == "") continue;

		buildPacket(packet, realPackSz, 0, SERVERDATA_EXECCOMMAND, command.c_str());
		send(client_fd, packet, realPackSz, 0);

		if(command == "stop" || command == "quit") break;

		if(!isSource)
		{
			carry = recv(client_fd, buffer, BUFFER_SIZE, 0);
			printPacket(realPackSz, buffer, hasColor);
		}
		else
		{
			carry = recv(client_fd, buffer, BUFFER_SIZE, 0);
			send(client_fd, packet, realPackSz, 0);
			for(;;)
			{
				carry = recv(client_fd, buffer, BUFFER_SIZE, 0);
				printPacket(realPackSz, buffer, hasColor);
				buildPacket(packet, realPackSz, 0, SERVERDATA_EXECCOMMAND, "");
				send(client_fd, packet, realPackSz, 0);
				if(carry == 14) break;
			}
		}
	}
	close(client_fd); // Close connection
	printf(RED "Connection Closed.\n\n" RST);
	return 0;
}
