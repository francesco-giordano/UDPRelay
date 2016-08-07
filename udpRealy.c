/*
    Raw UDP sockets
*/
#include<stdio.h> //for printf
#include<string.h> //memset
#include<sys/socket.h>    //for socket ofcourse
#include<stdlib.h> //for exit(0);
#include<errno.h> //For errno - the error number
#include<netinet/udp.h>   //Provides declarations for udp header
#include<netinet/ip.h>    //Provides declarations for ip header
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define BUFSIZE 4096
#define MAX_SERVER 5
#define MAX_STR 1024
 
/* 
    96 bit (12 bytes) pseudo header needed for udp header checksum calculation 
*/
struct pseudo_header
{
	u_int32_t source_address;
	u_int32_t dest_address;
	u_int8_t placeholder;
	u_int8_t protocol;
	u_int16_t udp_length;
};


typedef struct server_info
{
	char name[MAX_STR];
	int port;
	int aviable;
} t_server_info;


int
udptest(int s)
{
	int i, ret;

	for (i = 0; i <= 3; i++) {
		if (write(s, "X", 1) == 1)
			ret = 1;
		else
			ret = -1;
	}
	return (ret);
}
 
/*
    Generic checksum calculation function
*/
unsigned short csum(unsigned short *ptr,int nbytes) 
{
    register long sum;
    unsigned short oddbyte;
    register short answer;
 
    sum=0;
    while(nbytes>1) {
        sum+=*ptr++;
        nbytes-=2;
    }
    if(nbytes==1) {
        oddbyte=0;
        *((u_char*)&oddbyte)=*(u_char*)ptr;
        sum+=oddbyte;
    }
 
    sum = (sum>>16)+(sum & 0xffff);
    sum = sum + (sum>>16);
    answer=(short)~sum;
     
    return(answer);
}

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}


 
int main (int argc, char **argv)
{
	int sockfd; /* socket */
	int portno; /* port to listen on */
	int clientlen; /* byte size of client's address */
	struct sockaddr_in serveraddr; /* server's addr */
	struct sockaddr_in clientaddr; /* client addr */
	struct hostent *hostp; /* client host info */
	char buf[BUFSIZE]; /* message buf */
	char *hostaddrp; /* dotted decimal host addr string */
	int optval; /* flag value for setsockopt */
	int n; /* message byte size */
	t_server_info server_pool[MAX_SERVER];
	int serversno;

	/*
	 * check command line arguments
	 */
	if (argc != 2) {
		fprintf(stderr, "usage: %s config_file\n", argv[0]);
		exit(1);
	}



	/*
	 * Read Config File
	 */
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	fp = fopen(argv[1], "r");
	if (fp == NULL)
	        exit(1);

	portno = 0;
	serversno = 0;
	while ((read = getline(&line, &len, fp)) != -1) {
		if (portno == 0) {
			portno = atoi(line);
		} else {
			if (serversno > MAX_SERVER)
				break;

			/*
			 * To Be review, there is better ways to do this
			 */
			char str1[MAX_STR], str2[MAX_STR];
			int j,target,k;
			target = 1;
			k=0;
			for(j; line[j]!='\n'; j++) {
				if (line[j] == ' ' && target == 1) {
					target = 2;
					k = 0;
				}
				if (target == 1)
					str1[k++] = line [j];
				else
					str2[k++] = line [j];
			}

			strcpy(server_pool[serversno].name,str1);
			server_pool[serversno].port = atoi(str2);
			server_pool[serversno].aviable = 0;
			serversno++;
			//DEBUG
			printf("%s %s\n",str1, str2);
		}
			

	}

	fclose(fp);
	if (line)
	free(line);

	//DEBUG
	printf("Port Number: %d\n",portno);
	//exit (0);

	/*
	 * socket: create the parent socket
	 */
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	/* setsockopt: Handy debugging trick that lets
	 * us rerun the server immediately after we kill it;
	 * otherwise we have to wait about 20 secs.
	 * Eliminates "ERROR on binding: Address already in use" error.
	 */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

	/*
	 * build the server's Internet address
	 */
	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)portno);

	/*
	 * bind: associate the parent socket with a port
	 */
	if (bind(sockfd, (struct sockaddr *) &serveraddr,
        	sizeof(serveraddr)) < 0)
	error("ERROR on binding");

	/*
	 * main loop: wait for a datagram, then echo it
	 */
	clientlen = sizeof(clientaddr);
	while (1) {

		/*
		* recvfrom: receive a UDP datagram from a client
		*/
		bzero(buf, BUFSIZE);
		n = recvfrom(sockfd, buf, BUFSIZE, 0, (struct sockaddr *) &clientaddr, &clientlen);
		if (n < 0)
			error("ERROR in recvfrom");

			//Create a raw socket of type IPPROTO
			int s = socket (AF_INET, SOCK_RAW, IPPROTO_RAW);
     
		if(s == -1)
			{
	        	//socket creation failed, may be because of non-root privileges
			perror("Failed to create raw socket");
			exit(1);
			}
   
		//Datagram to represent the packet
		char datagram[4096], source_ip[32], *data, *pseudogram;
	
		 //zero out the packet buffer
		 memset (datagram, 0, 4096);
     
		 //IP header
		 struct iphdr *iph = (struct iphdr *) datagram;
     
		 //UDP header
		 struct udphdr *udph = (struct udphdr *) (datagram + sizeof (struct ip));
     
		 struct sockaddr_in sin;
		 struct pseudo_header psh;
     
		//Data part
		data = datagram + sizeof(struct iphdr) + sizeof(struct udphdr);
		strcpy(data , buf);
	
		//some address resolution

		hostaddrp = inet_ntoa(clientaddr.sin_addr);
			if (hostaddrp == NULL)
				error("ERROR on inet_ntoa\n");

		strcpy(source_ip, hostaddrp);
		//DEBUG
		//strcpy(source_ip, "127.0.0.5");
     
	    	sin.sin_family = AF_INET;
		sin.sin_port = htons(6666);

		// Pick a server in the pool randomly
		int server_index = (rand() % serversno);
		printf("index: %d\n",server_index);

		sin.sin_addr.s_addr = inet_addr (server_pool[server_index].name);
		//DEBUG
		//printf("Chosen server: %s\n", server_pool[server_index].name);
		//sin.sin_addr.s_addr = inet_addr ("127.0.0.1");
     
		//Fill in the IP Header
		iph->ihl = 5;
		iph->version = 4;
		iph->tos = 0;
		iph->tot_len = sizeof (struct iphdr) + sizeof (struct udphdr) + strlen(data);
		iph->id = htonl (54321); //Id of this packet
		iph->frag_off = 0;
		iph->ttl = 255;
		iph->protocol = IPPROTO_UDP;
		iph->check = 0;      //Set to 0 before calculating checksum
		iph->saddr = inet_addr ( source_ip );    //Spoof the source ip address
		iph->daddr = sin.sin_addr.s_addr;

		//Ip checksum
		iph->check = csum ((unsigned short *) datagram, iph->tot_len);
     
		//UDP header
		udph->source = htons (clientaddr.sin_port);

		udph->dest = htons (server_pool[server_index].port);
		//DEBUG
		//printf("on port: %d\n", server_pool[server_index].port);
		//udph->dest = htons (12345);
		udph->len = htons(8 + strlen(data)); //tcp header size
		udph->check = 0; //leave checksum 0 now, filled later by pseudo header
 
		//Now the UDP checksum using the pseudo header
		psh.source_address = inet_addr( source_ip );
		psh.dest_address = sin.sin_addr.s_addr;
		psh.placeholder = 0;
		psh.protocol = IPPROTO_UDP;
		psh.udp_length = htons(sizeof(struct udphdr) + strlen(data) );

		int psize = sizeof(struct pseudo_header) + sizeof(struct udphdr) + strlen(data);
		pseudogram = malloc(psize);
     
		memcpy(pseudogram , (char*) &psh , sizeof (struct pseudo_header));
		memcpy(pseudogram + sizeof(struct pseudo_header) , udph , sizeof(struct udphdr) + strlen(data));
     
	    	udph->check = csum( (unsigned short*) pseudogram , psize);

        	//Send the packet
	        if (sendto (s, datagram, iph->tot_len ,  0, (struct sockaddr *) &sin, sizeof (sin)) < 0)
        	{
			perror("sendto failed");
	        }
		close(s);
	    }     
    return 0;
}
 
//Complete 
