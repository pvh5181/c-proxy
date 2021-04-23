/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 */
#include "csapp.h"
#define MAX_OBJECT_SIZE 7204056
/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
typedef struct rangeNode{
  int type;
  int first;
  int second
} rangeNode;

struct cache{
  int cachelen;
  char *cacheuri;
  char *cachebuf;
  char *cacheheaders;
};
struct cache c1 = {0,"", "", ""};

void doit(int fd);
void read_requesthdrs(rio_t *rp, rangeNode *nodePtr);
int parse_uri(char *uri, char host[], char port[], char ext[]);
void serve_static(int fd, char *filename, int filesize, int size_flag);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
    char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, SIG_IGN);
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,
        port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);                                             //line:netp:tiny:doit
    Close(connfd);                                            //line:netp:tiny:close
  }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/* $begin doit */
void doit(int fd)
{

  rangeNode range = {0, 0, 0};
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  rio_t rio;
  char requestbuf[MAXLINE] = "";
  /* Read request line and headers */
  rio_readinitb(&rio, fd);
  if (!rio_readlineb(&rio, buf, MAXLINE))  //line:netp:doit:readrequest
    return;

  printf("%s", buf);


  sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
  if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
    clienterror(fd, method, "501", "Not Implemented",
        "Tiny does not implement this method");
    return;
  }

  read_requesthdrs(&rio, &range);


  char host[50] = "";
  char ext[50] = "";
  char port[50] = "";
  int serverfd = parse_uri(uri, host, port, ext);  //line:netp:doit:staticcheck

  if ((strcmp(c1.cacheuri, uri)==0)) {
    rio_writen(fd, c1.cachebuf, c1.cachelen);
    return;
  }

  serverfd = open_clientfd(host, port);

//memcpy(tempbuf+body size)
    Rio_writen(serverfd, "GET ", strlen("GET "));
    Rio_writen(serverfd, ext, strlen(ext));
    Rio_writen(serverfd, " HTTP/1.0\r\n\r\n", strlen(" HTTP/1.0\r\n\r\n"));

    int i;





    c1.cacheuri = calloc(1, strlen(uri));
    strcpy(c1.cacheuri, uri);


    char headers[MAXBUF] = "";
    rio_t rio_fd;
    rio_readinitb(&rio_fd, serverfd);
    printf("\n");

    while((strcmp(requestbuf, "\r\n"))!=0) {
        rio_readlineb(&rio_fd, requestbuf, MAXLINE);
        strcat(headers, requestbuf);
        printf("%s", requestbuf);
        //rio_writen(fd, requestbuf, MAXLINE);
    }

    rio_writen(fd, headers, strlen(headers));
    c1.cacheheaders = malloc(strlen(headers));
    memcpy(c1.cacheheaders, headers, strlen(headers));


    int counter = 0;
    char localbuf[MAX_OBJECT_SIZE];
    while((i=rio_readn(serverfd, requestbuf, MAXLINE))>0){
      if ((counter) <MAX_OBJECT_SIZE){
        memcpy(localbuf+counter, requestbuf, i);
      }
      else{
        memcpy(localbuf+MAX_OBJECT_SIZE, requestbuf, i);
      }
      //segfaults here because cant memcpy past max object size
      rio_writen(fd, requestbuf, i);
      counter +=i;
      //c1.cachelen = 0;
      }


    c1.cachelen = counter;
    if ((c1.cachelen)>MAX_OBJECT_SIZE){
      c1.cachebuf = calloc(1, MAX_OBJECT_SIZE);
      memcpy(c1.cachebuf, localbuf, MAX_OBJECT_SIZE);
    }
    else{
      c1.cachebuf = calloc(1, c1.cachelen);
      memcpy(c1.cachebuf, localbuf, c1.cachelen);
    }

  close(serverfd);


}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp, rangeNode *nodePtr)
{
  char buf[MAXLINE];

  if (!rio_readlineb(rp, buf, MAXLINE))
    return;
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {          //line:netp:readhdrs:checkterm
    if (!strncasecmp(buf, "Range:", 6)) {
      process_range(buf, nodePtr);
    }
    rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 *             2 if static but no content-length .nosize
 */
/* $begin parse_uri */
int parse_uri(char *uri, char host[], char port[], char ext[]){
  char *hostptr = 0;
  char *portptr = 0;
  char *extptr = 0;
  int defaultport = 0;
  int extdefault = 0;

  if (!((strstr(uri, "http://")))){
    printf("error");
    return -1;
  }
  else{
    hostptr = (strstr(uri, "http://"));
    hostptr = hostptr+7;
    //strncpy(host, uri, hostptr-portptr);
    if (!((strstr(hostptr, ":")))){
        defaultport = 1;
    }
    else{
      portptr = (strstr(hostptr, ":"));
      portptr = portptr + 1;
    }


    if (!((strstr(hostptr, "/")))){
      extdefault = 1;
    }
    else{
      extptr = (strstr(hostptr, "/"));
    }
  }


  int hostptrlen = strlen(hostptr);
  int portptrlen = 0;
  int extptrlen = 0;
  if (extdefault==0){
    extptrlen = strlen(extptr);
    strcpy(ext, extptr);
  }
  else{
    strcpy(ext, "/");
  }
  if (defaultport==0){
    portptrlen = strlen(portptr);
    strncpy(port, portptr, portptrlen-extptrlen);
  }
  else{
    strcpy(port, "80");
  }
  strncpy(host, hostptr, hostptrlen-portptrlen-(!(extdefault)));
  return 0;

}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client
 * size_flag is 1, provide content-length,
 * size_flag is 2, do not provide content-length
 */
/* $begin serve_static */

void process_range(char *buf, rangeNode *nodePtr) {
  char *next_tok;
  int r1, r2;
  if ((next_tok = strstr(buf, "bytes=")) != NULL) {
    next_tok = next_tok + 6;
    if (sscanf(next_tok, "-%u", &r1) == 1) {
      nodePtr->type = 3;
      nodePtr->first = -r1;
    }
    else if (sscanf(next_tok, "%u-%u",&r1, &r2) == 2) {
      nodePtr->type = 1;
      nodePtr->first = r1;
      nodePtr->second = r2;
    }
    else if (sscanf(next_tok, "%u-",&r1) == 1) {
      nodePtr->type = 2;
      nodePtr->first = r1;
    }
    else {
      nodePtr->type = 0;
      printf("get range: error\n");
    }
  }
  printf("range type: %d, first: %d, second: %d\n", nodePtr->type,
   nodePtr->first, nodePtr->second);
}



void serve_static(int fd, char *filename, int filesize, int size_flag)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];
  size_t writesize;

  /* Send response headers to client */
  get_filetype(filename, filetype);       //line:netp:servestatic:getfiletype
  sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  if (size_flag == 1) {
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  }
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  writesize = strlen(buf);
  if (rio_writen(fd, buf, strlen(buf)) < writesize) {
    printf("errors writing to client.\n");       //line:netp:servestatic:endserve
  }
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//line:netp:servestatic:mmap
  Close(srcfd);                           //line:netp:servestatic:close
  if (rio_writen(fd, srcp, filesize) < filesize) {
    printf("errors writing to client.\n");         //line:netp:servestatic:write
  }
  Munmap(srcp, filesize);                 //line:netp:servestatic:munmap
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
    strcpy(filetype, "video/mp4");
  else if (strstr(filename, ".mp3"))
    strcpy(filetype, "audio/mp3");
  else
    strcpy(filetype, "text/plain");
}
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */ //line:netp:servedynamic:fork
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
    Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
    Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
  }
  Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}

/* $end serve_dynamic */
/*
 * clienterror - returns an error message to the client
 */
/* $begin clienterror */

void clienterror(int fd, char *cause, char *errnum,
    char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  sprintf(buf, "%sContent-type: text/html\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, (int)strlen(body));
  rio_writen(fd, buf, strlen(buf));
  rio_writen(fd, body, strlen(body));
}
/* $end clienterror */
