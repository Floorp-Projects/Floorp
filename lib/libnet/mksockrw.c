/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "mkutils.h"
#include "mktcp.h"
#include "mkparse.h"
#include "mkgeturl.h"  /* for error codes */
#include "fe_proto.h" /* for externs */
#include "merrors.h"
#include "ssl.h"

#include "xp_error.h"

extern int MK_HTTP_TYPE_CONFLICT;
extern int XP_ERRNO_EWOULDBLOCK;

#ifdef XP_UNIX
/* #### WARNING, this is duplicated in mkconect.c
 */
# include <sys/ioctl.h>
/*
 * mcom_db.h is only included here to set BYTE_ORDER !!!
 * MAXDNAME is pilfered right out of arpa/nameser.h.
 */
# include "mcom_db.h"

# if defined(__hpux) || defined(_HPUX_SOURCE)
#  define BYTE_ORDER BIG_ENDIAN
#  define MAXDNAME        256             /* maximum domain name */
# else
#  include <arpa/nameser.h>
# endif

#include <resolv.h>

#if !defined(__osf__) && !defined(AIXV3) && !defined(_HPUX_SOURCE) && !defined(__386BSD__) && !defined(__linux) && !defined(SCO_SV)
#include <sys/filio.h>
#endif

#endif /* XP_UNIX */

#ifdef PROFILE
#pragma profile on
#endif

/* Global TCP Read/Write variables
 */
PUBLIC char * NET_Socket_Buffer=0;
PUBLIC int    NET_Socket_Buffer_Size=0;

#if defined(XP_UNIX) && defined(XP_BSD_UNIX)

PRIVATE char * net_real_socket_buffer_ptr=0;

#include <unistd.h>
#include <malloc.h>

typedef unsigned int intptr_t;

char *GetPageAlignedBuffer(int size)
{
    char *rv;
    static int pageSize = 0;

    if (pageSize == 0) 
	  {
	    /* Cache this value to avoid syscall next time */
	    pageSize = getpagesize();
      }

	net_real_socket_buffer_ptr = 0;

    /* Allocate too much memory */
    rv = (char *) XP_ALLOC(size + pageSize - 1);
    if (rv) 
	  {
	    intptr_t r = (intptr_t) rv;
	    intptr_t offset = r & (pageSize - 1);

		net_real_socket_buffer_ptr = rv;

	    if (offset) 
		  {
	        /* Have to round up address */
	        r = r + pageSize - offset;
	      } 
		else 
		  {
	        /*
	        ** Could be generous here and realloc to shrink since we
	        ** don't need the extra space...
	        */
	      }
	    rv = (char*) r;
      }
    return rv;
}
#endif /* XP_UNIX */

/* allocate memory for the TCP socket
 * buffer
 */
PUBLIC int
NET_ChangeSocketBufferSize (int size)
{
    NET_Socket_Buffer_Size = 0;

	if(size < 1)
		size = 10*1024; /* default */

	if(size > 31*1024)
		size = 31*1024;

#if defined(XP_UNIX) && defined(XP_BSD_UNIX)
    FREEIF(net_real_socket_buffer_ptr);
    NET_Socket_Buffer = GetPageAlignedBuffer(size);
#else
    FREEIF(NET_Socket_Buffer);
    NET_Socket_Buffer = (char *) XP_ALLOC(size);
#endif /* XP_UNIX */

   if(!NET_Socket_Buffer)
	  return(0); 

   /* else */
   NET_Socket_Buffer_Size = size;
   return(1);
}

/* this is a very standard network write routine.
 * 
 * the only thing special about it is that
 * it returns MK_HTTP_TYPE_CONFLICT on special error codes
 *
 */
PUBLIC int NET_HTTPNetWrite (PRFileDesc *fildes, CONST void * buf, unsigned int nbyte)
{
    static int status;
  
    status = (int) NET_BlockingWrite(fildes, buf, nbyte);

#ifdef XP_UNIX
	/* these status codes only work on UNIX
	 */
    if ((status < 0) 
		 && (status == PR_NOT_CONNECTED_ERROR || status == PR_CONNECT_RESET_ERROR || status == PR_PIPE_ERROR)) 
        return MK_HTTP_TYPE_CONFLICT;
#endif /* XP_UNIX */

    /* else */
    return(status);
}

PUBLIC int32 NET_BlockingWrite (PRFileDesc *filedes, CONST void * buf, unsigned int nbyte)
{
    int32 length_written = 1;
	unsigned int tot_len_written = 0; 

	while(nbyte > 0 && length_written > -1)
	  {
		length_written = PR_Write(filedes, (const char*) buf+tot_len_written, nbyte);
		
		if(length_written > -1)
		  {
			nbyte -= (unsigned int) length_written;
			tot_len_written += (unsigned int) length_written;
		  }
		else
		  {
			int rv = PR_GetError();
			if(rv == PR_WOULD_BLOCK_ERROR)
			 {
#if defined(XP_WIN) && defined(MOZILLA_CLIENT)
			   FEU_StayingAlive();
#endif
		       length_written = 1; /* this will let it continue looping */
			 }
			else
			  {
			    return (rv < 0) ? rv : (-rv);
			  }
		  }
	  }

	return(length_written); /* postive or negative */
}

#ifdef DEBUG
/* only a debugging routine!
 * Prints the output to stderr as well as the socket
 */
MODULE_PRIVATE int 
NET_DebugNetWrite (PRFileDesc *fildes, CONST void *buf, unsigned nbyte)
{
  if(MKLib_trace_flag && nbyte > 0)
    {
#ifdef XP_UNIX
       write(2, "Tx: ", 4);
       write(2, buf, nbyte);
       write(2, "\n", 1);
#endif
    }

  return(PR_Write(fildes, buf, nbyte));
}

/* This is a pretty standard read routine for debugging
 *
 * It prints whatever it reads to stderr for debugging purposes
 */
MODULE_PRIVATE int
NET_DebugNetRead (PRFileDesc *fildes, void * buf, unsigned nbyte)
{
  static int status;  /* read return code */

  status = PR_Read (fildes, buf, nbyte);

  if(MKLib_trace_flag && status != PR_SUCCESS)
    {
#ifdef XP_UNIX
      write(2,"Rx: ", 4);
      write(2, (const char *)buf, status);
      write(2, "\n", 1);
#endif
    }

  return(status);
}
#endif /* DEBUG */


/* This is a pretty standard read routine
 *
 * The only special thing that is does is
 * that it checks for the special errors encountered
 * in a HTTP 1/.9 conflict and returns MK_HTTP_TYPE_CONFLICT
 * when encountered
 */
/* fix Mac warning of missing prototype */
MODULE_PRIVATE int 
NET_HTTPNetRead (PRFileDesc *fildes, void *buf, unsigned nbyte);

MODULE_PRIVATE int 
NET_HTTPNetRead (PRFileDesc *fildes, void *buf, unsigned nbyte)
{
  static int status;  /* read return code */

  status = PR_Read (fildes, buf, nbyte);

#ifdef XP_UNIX
    /* check for HTTP server type conflict 
     */
  if (status == ENOTCONN || status == ECONNRESET || status == EPIPE) 
      return MK_HTTP_TYPE_CONFLICT;
#endif /* XP_UNIX */

  /* else */
  return(status);
}

/* net_BufferedReadLine
 *
 * will do a single read on the passed in socket
 * and try and grok a single line from it.
 *
 * a '\n' is the demarkation of the end of a line
 *
 * if a '\n' exists the line will be returned into 'line' as
 * a separately malloc'd line.  Any extra data
 * will be malloc'd into the passed in 'buffer' pointer.
 *
 * if a '\n' is not found the data read from the
 * socket will be appended (realloc'd) to the end of
 * the passed in buffer.
 *
 * the status of the socket read will be passed back
 * as a return value
 *
 * if 'line' is non-zero then a line was available and
 * was malloc'd
 */
#define LINE_BUFFER_SIZE 1024

MODULE_PRIVATE int
NET_BufferedReadLine   (PRFileDesc *  sock, 
             	        char   ** line, 
             	        char   ** buffer, 
             	        int32   * buffer_size, 
             	        Bool    * pause_for_next_read)
{
    char *strptr, *linefeed=0;
    int status=1;
    static char line_buffer[LINE_BUFFER_SIZE];  /* maybe this should be static? */
    int line_length;
    char *far_end;

    *line = 0;  /* init */

    *pause_for_next_read = TRUE;  /* This is the default it may change */


    /* scan for line in existing buffer */
    if(*buffer_size > 0)
      {
        for(strptr = *buffer; strptr < *buffer+*buffer_size; strptr++)
        if(*strptr == LF)
          {
            linefeed = strptr;
            break;
          }
      }

    if(!linefeed)
      {
        /* get some more data from the socket */
		int32 read_size = MIN(LINE_BUFFER_SIZE, NET_Socket_Buffer_Size);
        status = PR_Read(sock, NET_Socket_Buffer, read_size);

		TRACEMSG(("Read %d bytes from socket %d", status, sock));

        if(status < 0)
	  	  {
			int rv = PR_GetError();
			if (rv == PR_WOULD_BLOCK_ERROR)
			  {
	    	    /* defaults to  *pause_for_next_read = TRUE; */
				return(1);
			  }
	    	*pause_for_next_read = FALSE;
            return (rv < 0) ? rv : (-rv);
	  	  }
	
		TRACEMSG(("Read %d bytes from socket\n",status));

        if(status > 0)
          {
            BlockAllocCat(*buffer, *buffer_size, NET_Socket_Buffer, status);
            *buffer_size += status;
          }

        if(*buffer_size > 0)
          {
            for(strptr = *buffer; strptr < *buffer+*buffer_size; strptr++)
            if(*strptr == LF)
              {
                linefeed = strptr;
                break;
              }
          }
      }
    
    if(linefeed)
      {
		int32 tot_buf_size;

        *linefeed = '\0';

        /* kill the '\r' if it exits */
        if(linefeed > *buffer && *(linefeed-1) == '\r')
           *(linefeed-1) = '\0';


		/* the number of bytes that the line tekes up */
		line_length = (linefeed+1) - *buffer;  

		/* the farthest end of the memory buffer */
        far_end = *buffer+*buffer_size;  

		if(line_length == *buffer_size)
	  	  {
	         /* the line is the whole buffer
	          * no copying is required and we know that
	          * there can't be any new \n's in the buffer
	          * so lets return now
	          */
	         *buffer_size = 0;
	         *line = *buffer;
	         return(status);
	      }

	    /* set the line pointer now since we know where it
	     * will end up
	     */
	    *line = far_end-line_length;

        /* I'm doing some optimization here to try and reduce malloc's
	     * I want to copy the line that the calling function needs
	     * to the end of the buffer and move the part of the buffer
	     * that needs saveing to the beginning of the buffer
         * that way no mallocs are required.  Space will be compacted
         * or enlarged by the AllocCat above.
	     *
	     * If the line_buffer isn't large enough to hold the whole
	     * line then we need to do the copy in segments and shift
	     * the contents of the remaining line segment and buffer
	     * to the left each time.  This is very inefficient
	     * but the line_buffer is large enough to handle every
	     * expected line size (since lines should always be less
	     * than 512).  The segmenting should only come into
	     * play in degenerate cases.
         */
		tot_buf_size = *buffer_size;

	    while(line_length)
	      {

	        if(line_length > LINE_BUFFER_SIZE)
	          {
	            XP_MEMMOVE(line_buffer, *buffer, LINE_BUFFER_SIZE);
        	    *buffer_size -= LINE_BUFFER_SIZE;
			    line_length -= LINE_BUFFER_SIZE;
				/* move everything over includeing the parts 
				 * of the buffer already moved 
			 	 */
                XP_MEMMOVE(*buffer, 
						(*buffer)+LINE_BUFFER_SIZE, 
						tot_buf_size-LINE_BUFFER_SIZE);
			    XP_MEMMOVE(far_end-LINE_BUFFER_SIZE, line_buffer, LINE_BUFFER_SIZE);
	          }
	        else
	          {
			    XP_MEMMOVE(line_buffer, *buffer, line_length);
        	    *buffer_size -= line_length;
				/* move everything over includeing the parts 
				 * of the buffer already moved 
			 	 */
                XP_MEMMOVE(*buffer, 
						(*buffer)+line_length, 
						tot_buf_size-line_length);
			    XP_MEMMOVE(far_end-line_length, line_buffer, line_length);
			    line_length = 0;
	          }
    	  }

        /* check for another linefeed in the buffered data
         * if there is one then we don't want to pause for
         * read yet.
         */
        linefeed = 0;
        for(strptr = *buffer; strptr <= *buffer+*buffer_size; strptr++)
            if(*strptr == LF)
              {
                linefeed = strptr;
                break;
              }

        if(linefeed)
           *pause_for_next_read = FALSE;
      }

    return(status);
}

#ifdef PROFILE
#pragma profile off
#endif
