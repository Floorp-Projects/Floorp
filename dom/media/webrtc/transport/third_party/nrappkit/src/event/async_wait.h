/**
   async_wait.h


   Copyright (C) 2001-2003, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


   ekr@rtfm.com  Thu Dec 20 20:14:49 2001
 */


#ifndef _async_wait_h
#define _async_wait_h

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <csi_platform.h>

typedef void (*NR_async_cb)(NR_SOCKET resource,int how,void *arg);

#define NR_ASYNC_WAIT_READ 0
#define NR_ASYNC_WAIT_WRITE 1


int NR_async_wait_init(void);
int NR_async_wait(NR_SOCKET sock, int how, NR_async_cb cb,void *cb_arg,
                            char *function,int line);
int NR_async_cancel(NR_SOCKET sock,int how);
int NR_async_schedule(NR_async_cb cb,void *arg,char *function,int line);
int NR_async_event_wait(int *eventsp);
int NR_async_event_wait2(int *eventsp,struct timeval *tv);


#ifdef NR_DEBUG_ASYNC
#define NR_ASYNC_WAIT(s,h,cb,arg) do { \
fprintf(stderr,"NR_async_wait(%d,%s,%s) at %s(%d)\n",s,#h,#cb,__FUNCTION__,__LINE__); \
       NR_async_wait(s,h,cb,arg,(char *)__FUNCTION__,__LINE__);            \
} while (0)
#define NR_ASYNC_SCHEDULE(cb,arg) do { \
fprintf(stderr,"NR_async_schedule(%s) at %s(%d)\n",#cb,__FUNCTION__,__LINE__);\
       NR_async_schedule(cb,arg,(char *)__FUNCTION__,__LINE__);            \
} while (0)
#define NR_ASYNC_CANCEL(s,h) do { \
       fprintf(stderr,"NR_async_cancel(%d,%s) at %s(%d)\n",s,#h,(char *)__FUNCTION__,__LINE__); \
NR_async_cancel(s,h); \
} while (0)
#else
#define NR_ASYNC_WAIT(a,b,c,d) NR_async_wait(a,b,c,d,(char *)__FUNCTION__,__LINE__)
#define NR_ASYNC_SCHEDULE(a,b) NR_async_schedule(a,b,(char *)__FUNCTION__,__LINE__)
#define NR_ASYNC_CANCEL NR_async_cancel
#endif

#endif

