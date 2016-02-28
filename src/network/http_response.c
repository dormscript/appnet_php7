
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "aeserver.h"
#include "http_response.h"
#include <fcntl.h>
#include "ae.h"

void http_response_proc( httpHeader* reqHeader , char* mime_type );
void onRespWritable(  aeEventLoop *el, int connfd, void *privdata, int mask )
{
	httpHeader* reqHeader = ( httpHeader*)privdata;
	//printf( "onRespWritable fd=%d,hfd=%d,mime_type=%s \n" , connfd , reqHeader->connfd,reqHeader->mime_type  );

	http_response_proc( reqHeader , reqHeader->mime_type );
	aeDeleteFileEvent( el, connfd , AE_WRITABLE );
}

void http_response_static( httpHeader* reqHeader  )
{
        int thid = reqHeader->connfd % servG->reactorNum;
        aeEventLoop *el = servG->reactorThreads[thid].reactor.eventLoop;
        if ( aeCreateFileEvent( el, reqHeader->connfd , AE_WRITABLE , onRespWritable, reqHeader ) == -1)
        {
            printf( "aeApiAddEvent Error fd=[%d] \n" , reqHeader->connfd );
            return;
        }
}

//response static resource
void http_response_proc( httpHeader* reqHeader , char* mime_type )
{
	int cllen , ctlen ;
	char path[1024];
	int len;
        get_file_path( reqHeader->uri , path );
	struct stat stat_file;
        int ret =  stat( path , &stat_file );
	if( ret < 0 )
	{
	   	printf( "File Not Exist Or Error:%s,errno=%d,error=%s \n" , path , errno,strerror( errno ) );
	   	//send 404.
	   	return;
	}

	char response[1024] = {0};
	snprintf( response , sizeof( response ) , 
		"%s 200 OK \r\nServer: appnet/1.0.0\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n" , 
		reqHeader->version,
		stat_file.st_size,mime_type
	);

	
	int nwritten = write( reqHeader->connfd , response , strlen( response ));
    	if (nwritten <= 0)
    	{
        	printf( "I/O error writing to client: %s \n", strerror(errno));
        	return;
    	}

        int fd = open( path , O_RDONLY );
	if( fd < 0 )
	{
	 	printf( "Open file Error:%s,errno=%d \n" , strerror(errno) , errno );
		return;
	}

	off_t offset = 0;
	while( offset < stat_file.st_size  )
	{	
		int sendn = sendfile( reqHeader->connfd , fd , &offset , stat_file.st_size - offset );	
		if( sendn < 0 )
		{
		   if(errno == EAGAIN)continue;
		   if(errno == EINTR)break;
		}
	}
}


void get_file_path( char* uri , char* path )
{
	char* pos  = strstr( uri , "?" );
	memcpy( path ,  servG->httpDocsRoot , strlen( servG->httpDocsRoot ) );
	if( pos == NULL )
	{
	   memcpy( path + strlen( servG->httpDocsRoot )  ,  uri , strlen( uri)  );
	}
	else
	{
	   memcpy( path + strlen( servG->httpDocsRoot ) ,  uri , pos-uri  );	
	}
}






