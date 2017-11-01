#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

#define TRUE 1
#define FALSE 0
pthread_mutex_t mutex;//mutex to protect the received message
int *comm_fd;//communication file descriptor 'pointer'(to identify which machine speak)


void *accept_client_co(void *communication_fd);
int stream_socket(int port);
void sig_handler(int sig);



void sig_handler(int sig) {
	switch(sig){
		case SIGINT:
      free(comm_fd);
      printf("Server was stopped by user action.\n" );
			exit(0);
			break;
		case SIGTERM:
      free(comm_fd);
      printf("Server was stopped by terminal signal.\n" );
			exit(0);
			break;
		default:
			perror("Unknown signal");
			break;
	}
}


// Datagram Sockets: SOCK_DGRAM
// Stream Sockets: SOCK_STREAM

int stream_socket(int port){

    struct sigaction sa;
    pthread_t thread;

    int socket_fd;//socket file descriptor returned by the socket function
    int optval = 1, thread_err;
    struct sockaddr_in servaddr;//IPv4
    /*
     * struct sockaddr_in { //for ipv6, we have to use sockaddr_in6
     *     sa_family_t    sin_family; // address family: AF_INET
     *     in_port_t      sin_port;   // port in network byte order
     *     struct in_addr sin_addr;   // internet address
     * };
     */
    comm_fd = (int *)malloc(sizeof(int));
    if(comm_fd == NULL)
      fprintf(stderr, "Couldn't allocate memory for thread argument\n");

   	sigemptyset(&sa.sa_mask);
   	sa.sa_handler = &sig_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, 0) == -1) {
    	perror("9");
  	}
  	if (sigaction(SIGTERM, &sa, 0) == -1) {
    	perror("9");
  	}

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);//create a socket and store his descriptor
    if(socket_fd == -1)
      return 1;
    //this function sets an option for the socket to allow use of the same port
    if(setsockopt(socket_fd , SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) == -1)
      return 6;

    bzero( &servaddr, sizeof(servaddr));//write sizeof(servaddr) time "0" to servaddr

    servaddr.sin_family = AF_INET;//IPv4
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);//allow connexions from any ip address
    servaddr.sin_port = htons(port);//specify the port to listen on. (argv[1])

    //assign the address of the server to the socket referred by the file descriptor (socket_fd)
    if(bind(socket_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1)
      return 2;

    //this socket will be used to accept incoming connections with 4
    //maximum pending connexions
    if(listen(socket_fd, 4) == -1)
      return 3;

    pthread_mutex_init(&mutex, NULL);
    while(TRUE)
    {
      /*
       *  extract the first connection request on
       *  the queue of pending connections, creates a new socket
       *  with the same properties of socket,
       *  and allocates a new file descriptor for the socket.
      */
        *comm_fd = accept(socket_fd, (struct sockaddr*) NULL, NULL);
        //for each client create a thread to handle the communication
        thread_err = pthread_create(&thread, NULL, accept_client_co, (void **)comm_fd);
        if(thread_err != 0)
          return 7;
    }
    free(comm_fd);
    return 0;

}

/* notice that we don't store all communication file descriptors which
 * were stored into memory pointed by "comm_fd" , and this region of memory change
 * with the number of connection requests.
 * It's because in our case we don't have to do so, thanks to threads.
 */
void *accept_client_co(void *communication_fd){

  int comm_fd;
  int msg_size;
  char str[100];

  comm_fd = *((int*)communication_fd);
  pthread_mutex_lock (&mutex);


  while( (msg_size = recv(comm_fd, str, 100, 0)) != 0){//Read from comm_fd and store into str
    printf("Server received %d bytes: %s\n", msg_size, str);
    printf("Echoing back: %s\n", str);
    write(comm_fd, str, strlen(str)+1);//write str to the new socket, created by "accept"
    bzero( str, 100);//write 100 "0" to str address
    pthread_mutex_unlock (&mutex);
  }
  pthread_exit(NULL);
}

int main(int argc, char const *argv[]) {

  int port;
  int err;

//Check parameters
  if(argc != 2){
    fprintf(stderr, "Usage: %s [port] \n", argv[0]);
    exit(1);
  }else if( (port = atoi(argv[1])) == 0){
    fprintf(stderr, "Please enter a valid port number \n");
    exit(1);
  }

  err = stream_socket(port);
  if(err == 1)
    fprintf(stderr, "Server failed to create a socket \n");
  else if(err == 2)
    fprintf(stderr, "Bind failed \n");
  else if(err == 3)
    fprintf(stderr, "Server failed to set up the queue \n");
  else if(err == 4)
    fprintf(stderr, "Server failed to read from socket \n");
  else if(err == 5)
    fprintf(stderr, "Server failed to write to socket \n");
  else if(err != 0)
    fprintf(stderr, "Server accidentally quit \n");


  return 0;
}
