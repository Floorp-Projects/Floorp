/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
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
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: shmtran.h,v 1.2 2001/07/12 19:58:08 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_SHMTRAN_H
#define _JVMP_SHMTRAN_H
#ifdef __cplusplus
extern "C" {
#endif
#include "jvmp_caps.h"
#ifdef JVMP_USE_SHM
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/fcntl.h>
#endif
/**
 * SHM transport used by Waterfall in out of proc situation.
 * Typical usage is:
 * Host -> libjvmp.so ->(JNI) Java implementation (no shm case, same process)
 * Host -> libjvmp_shm.so -> (forward request with SHM) -> 
 *                                 jvmp process ->(JNI) Java implementation 
 * Consider event sending, as generic enough: 
 * jint JVMP_SendEvent(JVMP_ExecutionContext* ctx, 
 *                     jint target, jint event, jlong data)
 * To use this transport we need:
 * in  JVMP_GetPlugin():
 *     - create message queue, using msgget().
 *     - start helper JVM process with argument - this queue ID
 *     - wait until JVMP send us a message, that it started msg loop
 * in JVMP_GetContext():
 *     - fill reserved field with message queue ID
 * in JVMP_SendEvent():
 *     - using message queue ID from ctx create SHM request
 * XXX: outdated comment - see sources 
 */ 

struct JVMP_ShmRequest 
{
  unsigned long      rid;       /* msg ID of message used to pass this request */
  int                msg_id;    /* ID of message queue */
  unsigned int       func_no;   /* number of function in JVMP_Context */
  int                retval;    /* returned by function value */
  int                argc;      /* number of function arguments */
  char*    argt;     /* string of arguments types, encoded by chars:
		        I - int, J - long, C - character, 
                        S - zero terminated string, P - any pointer
			also possible to pass/return values - like C's "&" - 
			use low register letters
			i - int, j - long, c - character,
			and pass arbitrary length structures:
			A[40] - 40 bytes of data passed
			a[40] - 40 bytes passed/returned.
			If you don't know exact length of returned value
			just pass a[0], but then you must set to 1
			parameter alloc_ref of JVMP_DecodeRequest. */
  int                 nullmask; /* bitmask of pointers args == 0. 
				   So you shouldn't
				   pass meaningfull == 0 pointers 
				   as > 32th argument ;) */
  int                 length;   /* length of data */
  char*               data;     /* marshaled arguments */
  int                 shmlen;   /* length of shm data */
  int                 shmid;    /* cached shmid of this request's data */
  JVMP_SecurityCap    caps;     /* capabilities of this call */
  char*               shmdata;  /* marshalled request in SHM */
};

typedef struct JVMP_ShmRequest JVMP_ShmRequest;
typedef void* jpointer;

#ifdef JVMP_USE_SHM
JVMP_ShmRequest* 
JVMP_NewShmReq(int msg_id, unsigned int func_no);
/*  JVMP_ShmRequest* JVMP_NewEncodedShmReq(int msg_id, unsigned int func_no, */
/*  				       char* sign, ...); */
int   JVMP_EncodeRequest(JVMP_ShmRequest* req, 
			 char* sign, ...);
int   JVMP_DecodeRequest(JVMP_ShmRequest* req, int alloc_ref,
			 char* sign, ...);
int   JVMP_DeleteShmReq(JVMP_ShmRequest* req);
int   JVMP_GetArgSize(JVMP_ShmRequest* req, int argno);
int   JVMP_SendShmRequest(JVMP_ShmRequest* req, int sync);
int   JVMP_RecvShmRequest(int msg_id, int sync, JVMP_ShmRequest* *preq);
int   JVMP_ExecuteShmRequest(JVMP_ShmRequest* req);
int   JVMP_ConfirmShmRequest(JVMP_ShmRequest* req);
int   JVMP_WaitConfirmShmRequest(JVMP_ShmRequest* req);
/* Argument is side on which transport is used, if in 
 * separate process situation (0 - not separate process case
 * 1 - host side, 2 - JVM side).
 */
int   JVMP_ShmInit(int side);
void  JVMP_ShmShutdown();
int   JVMP_ShmMessageLoop(int msg_id);
int   JVMP_msgget(key_t key, int msgflg);
void* JVMP_shmat(int shmid, const void *shmaddr, int shmflg);
int   JVMP_shmget(key_t key, int size, int shmflg);
int   JVMP_shmdt(const void *shmaddr, int shmid);

struct JVMP_ShmMessage
{
  long      mtype;
  int       message; /* also used to save returned by function value */
  int       shm_id;  /* also used to indicate that memory size changed */
  int       shm_len; /* also used to show updated len */
};

typedef struct JVMP_ShmMessage JVMP_ShmMessage;


#define IS_NULL_VAL(req, argn) (req->nullmask & (0x1 << (argn - 1)))
/* this one defines base for user function numbers - i.e. use 
   to numerate your functions use 
   JVMP_USER_BASE + MY_VENDOR_ID*10000 + funcno */ 
#define JVMP_USER_BASE 1000
#define JVMP_NEW_EXT_FUNCNO(_vendorID, _funcno)   \
        (JVMP_USER_BASE+_vendorID*10000+_funcno)
#define JVMP_GET_VENDOR(_extfuncno) \
        ((_extfuncno - JVMP_USER_BASE) / 10000)
#define JVMP_GET_EXT_FUNCNO(_funcno)   \
        (_funcno-JVMP_USER_BASE -JVMP_GET_VENDOR(_funcno)*10000)
#endif /* JVMP_USE_SHM */
#ifdef __cplusplus
};
#endif
#endif
