/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the X11 print system utilities library.
 * 
 * The Initial Developer of the Original Code is Roland Mainz 
 * <roland.mainz@informatik.med.uni-giessen.de>.
 * All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "xprintutil.h"
 
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#ifdef XPU_USE_THREADS
#include <time.h>
#ifdef XPU_USE_NSPR
#include <prthread.h>
#else
#include <pthread.h>
#endif
#else
#ifdef __sun
#include <wait.h>
#endif /* __sun */
#endif /* XPU_USE_THREADS */
#include <unistd.h>
#include <sys/time.h>

/* local prototypes */
#ifdef DEBUG
static void PrintXPGetDocStatus( XPGetDocStatus status );
#endif
static Bool  XNextEventTimeout( Display *display, XEvent *event_return, struct timeval *timeout );
static void  MyPrintToFileProc( Display *pdpy, XPContext pcontext, unsigned char *data, unsigned int data_len, XPointer client_data );
static void  MyFinishProc( Display *pdpy, XPContext pcontext, XPGetDocStatus  status, XPointer client_data );
#ifdef XPU_USE_NSPR
static void PrintToFile_Consumer( void *handle );
#else
static void *PrintToFile_Consumer( void *handle );
#endif

#ifdef DEBUG
/* DEBUG: Print XPGetDocStatus */
static
void PrintXPGetDocStatus( XPGetDocStatus status )
{
  switch(status)
  {
    case XPGetDocFinished:        puts("PrintXPGetDocStatus: XPGetDocFinished");       break;
    case XPGetDocSecondConsumer:  puts("PrintXPGetDocStatus: XPGetDocSecondConsumer"); break;
    case XPGetDocError:           puts("PrintXPGetDocStatus: XPGetDocError");          break;
    default:                      puts("PrintXPGetDocStatus: <unknown value");         break;
  }
}
#endif /* DEBUG */


/* XNextEvent() with timeout */
static
Bool XNextEventTimeout( Display *display, XEvent *event_return, struct timeval *timeout ) 
{
  int      res;
  fd_set   readfds;
  int      display_fd = XConnectionNumber(display);

  /* small shortcut... */
  if( timeout == NULL )
  {
    XNextEvent(display, event_return);
    return(True);
  }
  
  FD_ZERO(&readfds);
  FD_SET(display_fd, &readfds);

  /* Note/bug: In the case of internal X events (like used to trigger callbacks 
   * registered by XpGetDocumentData()&co.) select() will return with "new info" 
   * - but XNextEvent() below processes these _internal_ events silently - and 
   * will block if there are no other non-internal events.
   * The workaround here is to check with XEventsQueued() if there are non-internal 
   * events queued - if not select() will be called again - unfortunately we use 
   * the old timeout here instead of the "remaining" time... (this only would hurt 
   * if the timeout would be really long - but for current use with values below
   * 1/2 secs it does not hurt... =:-)
   */
  while( XEventsQueued(display, QueuedAfterFlush) == 0 )
  {
    res = select(display_fd+1, &readfds, NULL, NULL, timeout);
  
    switch(res)
    {
      case -1: /* select() error - should not happen */ 
          perror("XNextEventTimeout: select() failure"); 
          return(False);
      case  0: /* timeout */
        return(False);
    }
  }
  
  XNextEvent(display, event_return); 
  return(True);
}


#ifdef XPU_USE_THREADS
/**
 ** XpuPrintToFile() - threaded version
 ** Create consumer thread which creates it's own display connection to print server 
 ** (a 2nd display connection/thread is required to avoid deadlocks within Xlib),
 ** registers (Xlib-internal) consumer callback (via XpGetDocumentData(3Xp)) and 
 ** processes/eats all incoming events via MyPrintToFileProc(). A final call to 
 ** MyPrintToFileProc() cleans-up all stuff and sets the "done" flag.
 ** Note that these callbacks are called directly by Xlib while waiting for events in 
 ** XNextEvent() because XpGetDocumentData() registeres them as "internal" callbacks, 
 ** e.g. XNextEvent() does _not_ return before/after processing these events !!
 **
 ** Usage:
 ** XpStartJob(pdpy, XPGetData); 
 ** handle = XpuPrintToFile(pdpy, pcontext, "myfile");
 ** // render something
 ** XpEndJob(); // or XpCancelJob()
 ** status = XpuWaitForPrintFileChild(handle);
 **
 */
 
typedef struct 
{
#ifdef XPU_USE_NSPR
  PRThread      *prthread;
#else
  pthread_t      tid;
#endif    
  char          *displayname;
  Display       *pdpy;
  XPContext      pcontext;
  const char    *file_name;
  FILE          *file;
  XPGetDocStatus status;
  Bool           done;
} MyPrintFileData;


void *XpuPrintToFile( Display *pdpy, XPContext pcontext, const char *filename )
{
  MyPrintFileData *mpfd; /* warning: shared between threads !! */
         
  if( (mpfd = malloc(sizeof(MyPrintFileData))) == NULL )
    return(NULL);
  
  mpfd->displayname = DisplayString(pdpy);
  mpfd->pdpy        = NULL;
  mpfd->pcontext    = pcontext;
  mpfd->file_name   = filename;
  mpfd->file        = NULL;      
  mpfd->status      = XPGetDocError;
  
  /* make sure we can open the file for writing */
  if( (mpfd->file = fopen(mpfd->file_name, "w")) == NULL ) 
  {
    /* fopen() error */
    free(mpfd);
    return(NULL);
  }
  
  /* its important to flush before we start the consumer thread, 
   * to make sure that the XpStartJob gets through first in the parent
   */
  XFlush(pdpy);
#ifdef XPU_USE_NSPR
  if( (mpfd->prthread = PR_CreateThread(PR_SYSTEM_THREAD, PrintToFile_Consumer, mpfd, PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0)) == NULL )
#else    
  if( pthread_create(&(mpfd->tid), NULL, PrintToFile_Consumer, mpfd) != 0 ) 
#endif
  {
    /* pthread_create() error */
    fclose(mpfd->file);
    free(mpfd);
    return(NULL);
  }
  
  /* we're still in the parent */
  XPU_DEBUG_ONLY(printf("### parent started consumer thread.\n" ));
  return(mpfd);
}


XPGetDocStatus XpuWaitForPrintFileChild( void *handle )
{
  MyPrintFileData *mpfd   = (MyPrintFileData *)handle;
  void            *res;
  XPGetDocStatus   status;

#ifdef XPU_USE_NSPR
  if( PR_JoinThread(mpfd->prthread) != PR_SUCCESS  )
    perror("XpuWaitForPrintFileChild: PR_JoinThread() failure"); /* fixme(later): use NSPR error handling calls... */
#else   
  if( XPU_TRACE(pthread_join(mpfd->tid, &res)) != 0 )
    perror("XpuWaitForPrintFileChild: pthread_join() failure");
#endif
      
  status = mpfd->status;      
  free(handle);  
              
  XPU_DEBUG_ONLY(PrintXPGetDocStatus(status));
  return(status);
}

#else /* XPU_USE_THREADS */
/**
 ** XpuPrintToFile() - fork() version
 ** Create consumer thread which creates it's own display connection to print server 
 ** (a 2nd display connection/process is required to avoid deadlocks within Xlib),
 ** registers (Xlib-internal) consumer callback (via XpGetDocumentData(3Xp)) and 
 ** processes/eats all incoming events via MyPrintToFileProc(). A final call to 
 ** MyPrintToFileProc() cleans-up all stuff and sets the "done" flag.
 ** Note that these callbacks are called directly by Xlib while waiting for events in 
 ** XNextEvent() because XpGetDocumentData() registeres them as "internal" callbacks, 
 ** e.g. XNextEvent() does _not_ return before/after processing these events !!
 **
 ** Usage:
 ** XpStartJob(pdpy, XPGetData); 
 ** handle = XpuPrintToFile(pdpy, pcontext, "myfile");
 ** // render something
 ** XpEndJob(); // or XpCancelJob()
 ** status = XpuWaitForPrintFileChild(handle);
 **
 */
typedef struct 
{
  pid_t           pid;
  int             pipe[2];  /* child-->parent communication pipe */
  char           *displayname;
  Display        *pdpy;
  XPContext       pcontext;
  char           *file_name;
  FILE           *file;
  XPGetDocStatus  status;
  Bool            done;
} MyPrintFileData;


void *XpuPrintToFile( Display *pdpy, XPContext pcontext, const char *filename )
{
  MyPrintFileData *mpfd;

  if( (mpfd = malloc(sizeof(MyPrintFileData))) == NULL )
    return(NULL);

  /* create pipe */
  if( pipe(mpfd->pipe) == -1 ) 
  {
    /* this should never happen, but... */
    perror("XpuPrintToFile: cannot create pipe");
    free(mpfd);
    return(NULL);
  }
        
  mpfd->displayname = DisplayString(pdpy);
  mpfd->pcontext    = pcontext;
  mpfd->file_name   = filename;
  mpfd->file        = NULL;
  mpfd->status      = XPGetDocError;
  
  /* make sure we can open the file for writing */
  if( (mpfd->file = fopen(mpfd->file_name, "w")) == NULL ) 
  {
    /* fopen() error */
    close(mpfd->pipe[1]);
    close(mpfd->pipe[0]);      
    free(mpfd);
    return(NULL);
  }
  
  /* its important to flush before we fork, to make sure that the
   * XpStartJob gets through first in the parent
   */
  XFlush(pdpy);     
  
  mpfd->pid = fork();

  if( mpfd->pid == 0 ) 
  {
    /* we're now in the fork()'ed child */
    PrintToFile_Consumer(mpfd);
  }
  else if( mpfd->pid < 0 ) 
  {
    /* fork() error */
    close(mpfd->pipe[1]);
    close(mpfd->pipe[0]);
    fclose(mpfd->file);
    free(mpfd);
    return(NULL);
  }
  
  /* we're still in the parent */
  XPU_DEBUG_ONLY(printf("### parent fork()'ed consumer child.\n"));
  
  /* child will write into file - we don't need it anymore here... :-) */
  fclose(mpfd->file);
  close(mpfd->pipe[1]);
  return(mpfd);      
}


XPGetDocStatus XpuWaitForPrintFileChild( void *handle )
{
  MyPrintFileData *mpfd   = (MyPrintFileData *)handle;
  siginfo_t        res;
  XPGetDocStatus   status = XPGetDocError; /* used when read() from pipe fails */
  
  if( XPU_TRACE(waitid(P_PID, mpfd->pid, &res, WEXITED)) == -1 )
    perror("XpuWaitForPrintFileChild: waitid failure");

  /* read the status from the child */ 
  if( read(mpfd->pipe[0], &status, sizeof(XPGetDocStatus)) != sizeof(XPGetDocStatus) )
  {
    perror("XpuWaitForPrintFileChild: can't read XPGetDocStatus");
  }    
  close(mpfd->pipe[0]);
    
  free(handle);
  
  XPU_DEBUG_ONLY(PrintXPGetDocStatus(status));
  return(status);
}
#endif /* XPU_USE_THREADS */


static 
void MyPrintToFileProc( Display       *pdpy,
                        XPContext      pcontext,
                        unsigned char *data, 
                        unsigned int   data_len,
                        XPointer       client_data )
{
  MyPrintFileData *mpfd = (MyPrintFileData *)client_data;
  
  /* write to the file */
  XPU_TRACE_CHILD((void)fwrite(data, data_len, 1, mpfd->file)); /* what about error handling ? */
}   


static 
void MyFinishProc( Display        *pdpy,
                   XPContext       pcontext,
                   XPGetDocStatus  status,
                   XPointer        client_data )
{
  MyPrintFileData *mpfd = (MyPrintFileData *)client_data;

  /* remove the file if not successfull */
  if( status != XPGetDocFinished )
  {
    XPU_DEBUG_ONLY(printf("MyFinishProc: error %d\n", (int)status));
    XPU_TRACE_CHILD(remove(mpfd->file_name));
  }  
  
  XPU_TRACE_CHILD((void)fclose(mpfd->file)); /* what about error handling ? */
  
  mpfd->status = status; 
  mpfd->done   = True;    
}


static
#ifdef XPU_USE_NSPR
void PrintToFile_Consumer( void *handle )
#else
void *PrintToFile_Consumer( void *handle )
#endif
{
  MyPrintFileData *mpfd = (MyPrintFileData *)handle;
  XEvent           dummy;
  struct timeval   timeout;
  
  timeout.tv_sec  = 0;
  timeout.tv_usec = 100000; /* 1/10 s */
        
  XPU_DEBUG_ONLY(printf("### child running, getting data from '%s'.\n", mpfd->displayname));
  
  /* we cannot reuse fork()'ed display handles - our child needs his own one */
  if( (mpfd->pdpy = XPU_TRACE_CHILD(XOpenDisplay(mpfd->displayname))) == NULL )
  {
    perror("child cannot open display");
#ifdef XPU_USE_NSPR
    return;
#else
    return(NULL);
#endif
  }
    
  mpfd->done = False;
    
  /* register "consumer" callbacks */
  if( XPU_TRACE_CHILD(XpGetDocumentData(mpfd->pdpy, mpfd->pcontext,
                                        MyPrintToFileProc, MyFinishProc, 
                                        (XPointer)mpfd)) == 0 )
  {
    XPU_DEBUG_ONLY(printf("XpGetDocumentData cannot register callbacks\n"));
#ifdef XPU_USE_NSPR
      return;
#else
      return(NULL);
#endif
  }      
  
  /* loop forever - libXp has registered hidden event callbacks for the consumer 
   * callbacks - the finishCB will call set the "done" boolean after all...
   */
  while( mpfd->done != True )
  {
    XNextEventTimeout(mpfd->pdpy, &dummy, &timeout);
  }
  
  XCloseDisplay(mpfd->pdpy);

#ifdef XPU_USE_THREADS
#ifdef XPU_USE_NSPR
  return;
#else
  return(NULL);
#endif
#else   
  /* write the status to the parent */
  if( XPU_TRACE_CHILD(write(mpfd->pipe[1], &mpfd->status, sizeof(XPGetDocStatus))) != sizeof(XPGetDocStatus) )
  {
    perror("PrintToFile_Consumer: can't write XPGetDocStatus");
  }
  
  /* we don't do any free's or close's, as we are just
   * going to exit, in fact, get out without calling any C++
   * destructors, etc., as we don't want anything funny to happen
   * to the parent 
   */
   XPU_TRACE_CHILD(_exit(EXIT_SUCCESS));
#endif /* XPU_USE_THREADS */           
}

/* EOF. */
