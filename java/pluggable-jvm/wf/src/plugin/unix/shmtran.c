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
 * $Id: shmtran.c,v 1.2 2001/07/12 19:58:24 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include "shmtran.h"
#include <stdlib.h>
#include <stdarg.h> /* for va_list */
#include <stdio.h>
#include <string.h>
#include <strings.h> /* for index() */
#include <signal.h>
#include <errno.h>
#include "jvmp_cap_vals.h"

/* XXX: int->jint, long->jlong */

static volatile int g_terminate = 0;
#define MAX_SHM_MSG 40
#define MAX_SHM_MEM 10
static int _msgs[MAX_SHM_MSG];
static int _shms[MAX_SHM_MEM];
static struct sigaction _sigs[NSIG];
#define JVMP_SHM_LIMIT 1000000
static int g_side = 0;

/* to break, or not to break */
#define ADD_MQ(_idx, msg)  for(_idx=0; _idx<MAX_SHM_MSG; _idx++) \
                     if (_msgs[_idx] == -1) { _msgs[_idx] = msg; break; }
#define DEL_MQ(_idx, msg)  for(_idx=0; _idx<MAX_SHM_MSG; _idx++) \
                     if (_msgs[_idx] == msg) { _msgs[_idx] = -1; break; }
#define ADD_SHM(_idx, shm) for(_idx=0; _idx<MAX_SHM_MEM; _idx++) {\
                     if (_shms[_idx] == -1) { _shms[_idx] = shm; break; } };\
                     if (_idx == MAX_SHM_MEM) \
                        fprintf(stderr, "Leaking SHM(_shms[2]=%d)?\n", _shms[2]);
#define DEL_SHM(_idx, shm) for(_idx=0; _idx<MAX_SHM_MEM; _idx++) \
                     if (_shms[_idx] == shm) { _shms[_idx] = -1;}

int JVMP_msgget(key_t key, int msgflg)
{
  int rval, i;
  
  rval = msgget (key, msgflg);
  if (rval != -1)  { ADD_MQ(i, rval); }
  return rval;
}

int JVMP_shmget(key_t key, int size, int shmflg)
{
  int rval, i;
  
  rval = shmget (key, size, shmflg);
  if (rval != -1)  { ADD_SHM(i, rval); }
  return rval;
}

void* JVMP_shmat(int shmid, const void *shmaddr, int shmflg)
{
  void* rval;
  int i;
  
  rval = shmat(shmid, shmaddr, shmflg);
  if (rval != (void*)-1) { ADD_SHM(i, shmid); }
  return rval;
}


int JVMP_shmdt(const void *shmaddr, int shmid)
{
  int rval, i;
  
  DEL_SHM(i, shmid);
  rval = shmdt(shmaddr);
  return rval;
}

static int GetShmRealLen(int len)
{
  int rlen;
#ifdef __linux
  /* man shmget on Linux - page size is greater really */
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif
  rlen = (len + PAGE_SIZE) / PAGE_SIZE * PAGE_SIZE; 
#else
  rlen = len;
#endif
  return rlen;
}

JVMP_ShmRequest* JVMP_NewShmReq(int msg_id, unsigned int func_no)
{
  JVMP_ShmRequest* req;
  
  req = (JVMP_ShmRequest*)malloc(sizeof(JVMP_ShmRequest));
  if (!req) return NULL;
  req->rid = 0;
  req->argt = NULL;
  req->argc = 0;
  req->length = 0;
  req->data = NULL;
  req->func_no = func_no;
  req->retval = 0; /* determined later in JVMP_WaitConfirmShmRequest*/
  req->msg_id = msg_id;
  req->shmid = 0;
  req->shmdata = NULL;
  req->nullmask = 0;
  req->shmlen = 0;
  JVMP_FORBID_ALL_CAPS(req->caps);
  JVMP_ALLOW_CAP(req->caps, JVMP_CAP_SYS_PARITY); /* integrity bit */
  return req;
}

int JVMP_EncodeRequest(JVMP_ShmRequest* req,
		       char* fmt, ...) 
{
#define GET_AND_COPY(ap, type, pos, len) \
       type##_val = va_arg(ap, type); \
       size = sizeof(type); \
       if (pos+size >= len) { len = step+pos+size; buf = realloc(buf, len); } \
       memcpy(buf+pos, &type##_val, size); \
       pos += size; argc++;
#define GET_AND_COPY_STRING(ap, pos, len) \
       string_val = va_arg(ap, char*); \
       if (!string_val) {nullmask |= (0x1 << argc);  } else {\
       size = strlen(string_val) + 1; \
       if (pos+size >= len) { len = step+pos+size; buf = realloc(buf, len); } \
       memcpy(buf+pos, string_val, size); \
       pos += size; } argc++;
#define GET_AND_COPY_DATA(ap, pos, len, size) \
       jpointer_val = va_arg(ap, void*); \
       if (!jpointer_val) { nullmask |= (0x1 << argc);  } else {\
       if (pos+size >= len) { len = step+pos+size; buf = realloc(buf, len); } \
       memcpy(buf+pos, jpointer_val, size); \
       pos += size;}  argc++;
  char*    buf = NULL;
  char     *next, lens[20];
  int      len, step, pos, size, argc, slen, nullmask;
  int      int_val;
  char*    string_val;
  long     long_val;
  jpointer jpointer_val;
  va_list ap;
  if (!req || !fmt) return 0;
  if (req->argt) free(req->argt);
  req->argt = strdup(fmt);
  nullmask = 0;
  len = 0; step = 20; pos = 0; argc = 0; size = 0;
  va_start(ap, fmt);
  while(*fmt)
    {
      switch(*fmt++) 
	{
	case 'I':           /* integer */
	  GET_AND_COPY(ap, int, pos, len);
	  //printf("int %d of size %d\n", int_val, size);
	  break;
	case 'i':           /* integer ref */
	  GET_AND_COPY_DATA(ap, pos, len, sizeof(int));
	  //printf("int ref %d\n", (int)*(int*)jpointer_val);
	  break;
	case 'J':           /* long */
	  GET_AND_COPY(ap, long, pos, len);
	  //printf("long %ld of size %d\n", long_val, size);
	  break;
	case 'j':           /* long ref */
	  GET_AND_COPY_DATA(ap, pos, len, sizeof(long));
	  //printf("long ref %ld\n", (long)*(long*)jpointer_val);
	  break;
	case 'P':           /* pointer */
	  GET_AND_COPY(ap, jpointer, pos, len);
	  //printf("pointer %p of size %d\n", jpointer_val, size);
	  break;
	case 'p':           /* pointer ref */
	  GET_AND_COPY_DATA(ap, pos, len, sizeof(void*));
	  //printf("pointer ref %p\n", *(void**)jpointer_val);
	  break;
	case 'C':           /* char */
	  /* `char' is promoted to `int' when passed through `...' */
	  GET_AND_COPY(ap, int, pos, len);
	  //printf("char %c of size %d\n", int_val, size);
	  break;
	case 'c':           /* char ref - not string */
	  GET_AND_COPY_DATA(ap, pos, len, sizeof(void*));
	  break;
	case 'S':           /* zero terminated string */
	case 's':
	  GET_AND_COPY_STRING(ap, pos, len);
	  //printf("string \"%s\" of size %d\n", string_val, size);
	  break;
	case 'A':          /* any data - length follows */
	case 'a':
	  if (*fmt != '[') break; /* invalid description */
	  next = index(fmt, ']');
	  if (!next) break; /* invalid description */
	  slen = (int)(next - fmt);
	  if (slen > 20) break;
	  memset(lens, 0, slen);
	  memcpy(lens, fmt+1, slen-1);
	  slen = atoi(lens);
	  GET_AND_COPY_DATA(ap, pos, len, slen)
	  fmt = next + 1;
	  //fprintf(stderr, "data %p of size %d\n", jpointer_val, slen);
	  break;
	default:
	  //printf("UNKNOWN\n");
	  break;
	}
    };
  va_end(ap);
  //printf("Total len is %d, argc=%d nullmask=0x%x\n", pos, argc, nullmask);
  req->argc = argc;
  req->length = pos;
  if (req->data) free(req->data);
  req->data = buf;
  req->nullmask = nullmask;
  return 1;
}

int JVMP_DecodeRequest(JVMP_ShmRequest* req, int alloc_ref,
		       char* sign, ...)
{
#define CHECK_SIGNATURE(sign1, sign2) \
         if (strcmp(sign1, sign2)) { \
	  fprintf(stderr, "signature mismatch\n"); return 0; }

  char* fmt;
  char *buf, *next, lens[20];
  int size, argc, slen, byval;
  jpointer* ptr;
  va_list ap;  

  if (!req || !sign) return 0;
  //CHECK_SIGNATURE(req->argt, sign);
  fmt = req->argt;
  buf = req->data;
  argc = 0;
  va_start(ap, sign);

  while(*fmt)
    {
      byval = 0;
      switch(*fmt++) 
	{
	case 'I':           /* integer */
	  byval = 1;
	case 'i':
	  size = sizeof(int);
	  break;
	case 'J':           /* long */
	  byval = 1;
	case 'j':
	  size = sizeof(long);
	  break;
	case 'P':           /* pointer */
	  byval = 1;
	case 'p':
	  size = sizeof(void*);
	  break;
	case 'C':           /* char */
	  byval = 1;
	case 'c':
	  /* `char' is promoted to `int' when passed through `...' */
	  size = sizeof(int);
	  break;
	case 'S':           /* zero terminated string */
	case 's':
	  size = strlen(buf)+1;
	  break;
	case 'A':
	case 'a':           /* any data - length follows */
	  size = 0;
	  if (*fmt != '[') break; /* invalid description */
	  next = index(fmt, ']');
	  if (!next) break; /* invalid description */
	  slen = (int)(next - fmt);
	  memset(lens, 0, slen);
	  memcpy(lens, fmt+1, slen-1);
	  slen = atoi(lens);
	  size = slen;
	  fmt = next + 1;
	  break;
	  // not yet
#if 0
	case 'Z':
	case 'z':          /* really special, but important case of char** 
			      Z[10] means array of 10 strings, or z[0] 
			      if you don't know length of returned value.
			      But another side must know, when calling 
			      JVMP_EncodeRequest, and it should change 
			      signature to smth meaningful. 
			      Use with care. */
	  size = 0;
	  if (*fmt != '[') break; /* invalid description */
	  next = index(fmt, ']');
	  if (!next) break; /* invalid description */
	  slen = (int)(next - fmt);
	  if (slen > 20) break;
	  memset(lens, 0, slen);
	  memcpy(lens, fmt+1, slen-1);
	  slen = atoi(lens);
	  if (!slen) { size = -1; break; } 
	  size = slen;
	  fmt = next + 1;
	  break;
#endif
	default:
	  size = 0;
	  fprintf(stderr, "shmtran: UNKNOWN signature: %c\n",*(fmt-1));
	  break;
	}
      if (size != 0) argc++;
      if (size == -1) size = 0;
      ptr = va_arg(ap, jpointer*);
      if (ptr) 
	{
	  if (IS_NULL_VAL(req, argc))
	    {
	      *ptr = NULL;
	      continue;
	    }
	  if (byval)
	    memcpy(ptr, buf, size);
	  else
	    {
	      if (alloc_ref) *ptr = malloc(size);
	      memcpy(*ptr, buf, size);
	    }
	}
      if (!IS_NULL_VAL(req, argc))
	buf += size;
    };
  va_end(ap);
  return 1;
}

int JVMP_GetArgSize(JVMP_ShmRequest* req, int argno)
{
  char* fmt;
  char *buf, *next, lens[40];
  int size, argc, slen;
  
  if (!req) return 0;
  fmt = strdup(req->argt);
  buf = req->data;
  argc = 0;
  while(*fmt)
    {
      switch(*fmt++) 
	{
	case 'I':           /* integer */
	case 'i':
	  size = sizeof(int);
	  break;
	case 'J':           /* long */
	case 'j':
	  size = sizeof(long);
	  break;
	case 'P':           /* pointer */
	case 'p':
	  size = sizeof(void*);
	  break;
	case 'C':           /* char */
	case 'c':
	  /* `char' is promoted to `int' when passed through `...' */
	  size = sizeof(int);
	  break;
	case 'S':           /* zero terminated string */
	case 's':
	  size = strlen(buf)+1;
	  break;
	case 'A':
	case 'a':           /* any data - length follows */
	  size = 0;
	  if (*fmt != '[') break; /* invalid description */
	  next = index(fmt, ']');
	  if (!next) break; /* invalid description */
	  slen = (int)(next - fmt);
	  if (slen > 20) break;
	  memset(lens, 0, slen);
	  memcpy(lens, fmt+1, slen-1);
	  slen = atoi(lens);
	  if (!slen) { size = -1; break; } 
	  size = slen;
	  fmt = next + 1;
	  break;
	default:
	  size = 0;
	  printf("UNKNOWN: %c\n",*(fmt-1));
	  break;
	}
      if (size != 0) argc++;
      if (size == -1) size = 0;
      if (argc == argno)
	{
	  if (IS_NULL_VAL(req, argc))
	    {
	      return 0;
	    }
	  return size;
	}
      if (!IS_NULL_VAL(req, argc))
	  buf += size;
    };
  return -1;
}


int JVMP_DeleteShmReq(JVMP_ShmRequest* req)
{
  JVMP_shmdt(req->shmdata, req->shmid);
  /* XXX: this can be inefficient, but otherwise I'll get EINVAL
     if this segment will be reused with greater size */
  shmctl(req->shmid, IPC_RMID, NULL);
  free(req->data); 
  free(req->argt);
  free(req);
  return 1;
}

// XXX: update when JVMP_ShmRequest changed
static int JVMP_MarshallShmReq(JVMP_ShmRequest* req, 
			       char* *res, int *plen)
{
#define ADD_VAL(val) \
    size = sizeof(val); \
    if (pos+size >= len) { len = pos+size+step; buf = realloc(buf, len); } \
    memcpy(buf+pos, &val, size); \
    pos += size; argc++;
#define ADD_REF(ref, size) \
    if (pos+size >= len) { len = pos+size+step; buf = realloc(buf, len); } \
    memcpy(buf+pos, ref, size); \
    pos += size; argc++;
  int len=0, pos=0, size, step=20, argc=0;
  char* buf = NULL;
  if (!req) return 0;
  
  ADD_VAL(req->rid);
  ADD_VAL(req->msg_id);
  ADD_VAL(req->func_no);
  ADD_VAL(req->retval);
  ADD_VAL(req->argc);
  ADD_REF(req->argt, strlen(req->argt)+1);
  ADD_VAL(req->nullmask);
  ADD_VAL(req->length);
  ADD_REF(req->data, req->length);
  ADD_VAL(req->shmlen);
  ADD_VAL(req->shmid);
  ADD_VAL(req->caps);
  // there's no ADD_REF(req->shmdata, req->length); as it's SHARED
  *res = buf;
  *plen = pos;
  return 1;
}
static int JVMP_DemarshallShmReq(char* buf, int buf_len,
				 JVMP_ShmRequest* *preq)
{
#define EXTRACT_VAL(val) \
    size = sizeof(val); \
    memcpy(&(val), buf+pos, size); \
    pos += size; argc++;
#define EXTRACT_REF(ref, size) \
    ref = (char*)malloc(size); \
    memcpy(ref, buf+pos, size); \
    pos += size; argc++;
  int pos=0, size, argc=0;
  JVMP_ShmRequest* req;
  req = JVMP_NewShmReq(0, 0);
  if (!req) return 0;
  
  EXTRACT_VAL(req->rid);
  EXTRACT_VAL(req->msg_id);
  EXTRACT_VAL(req->func_no);
  EXTRACT_VAL(req->retval);
  EXTRACT_VAL(req->argc);
  EXTRACT_REF(req->argt, strlen(buf+pos)+1);
  EXTRACT_VAL(req->nullmask);
  EXTRACT_VAL(req->length);
  EXTRACT_REF(req->data, req->length);
  EXTRACT_VAL(req->shmlen);
  EXTRACT_VAL(req->shmid);
  EXTRACT_VAL(req->caps);
  if (!JVMP_IS_CAP_ALLOWED(req->caps, JVMP_CAP_SYS_PARITY)) 
    {
      fprintf(stderr, "Parity cap is not set - probably WF bug\n");
      return 0;
    }
  *preq = req;
  return 1;
}
/* there's additional #if 0'ed handshake code for cases
   when it desired to detach shared memory ASAP */
int JVMP_SendShmRequest(JVMP_ShmRequest* req, int sync)
{
  char* buf = NULL; 
  int len = 0;
  int shmid;
  void* shmbuf;
  static key_t key = 0;
  JVMP_ShmMessage msg;
  static unsigned long id = 0;
  
  id += 2;
  if (id >= JVMP_SHM_LIMIT) id = 0;
  req->rid = id;
  //if (!key) key = ftok(".", 's');
  JVMP_MarshallShmReq(req, &buf, &len);
  shmid = JVMP_shmget(key, len, IPC_CREAT | IPC_EXCL | IPC_PRIVATE | 0660);
  if (shmid == -1)
    {
      perror("JVMP_SendShmRequest: shmget");
      return 0;
    }
  if ((shmbuf = JVMP_shmat(shmid, NULL, 0)) == (void*)-1)
    {
      perror("JVMP_SendShmRequest: shmat");
      return 0;
    }
  req->shmid = shmid;
  req->shmdata = shmbuf;
  req->shmlen = GetShmRealLen(len);

  /* yes, double marshalling is stupid, but I dunno how to find out len */
  free(buf);
  JVMP_MarshallShmReq(req, &buf, &len);
  memcpy(shmbuf, buf, len);
  free(buf);
  msg.mtype = id; 
  msg.message = sync ? 1 : 2; /* 1 - sync, 
				 wait confirmtion
				 2 -  async, 
				 never wait confirmation */
  msg.shm_len = req->shmlen;
  msg.shm_id = shmid;
  if ((msgsnd(req->msg_id, (struct msgbuf *)&msg, 
	      sizeof(msg)-sizeof(long), 0)) ==-1)
    {
      perror("JVMP_SendShmRequest: msgsnd");
      return 0;
    }
  return 1;
}

int JVMP_RecvShmRequest(int msg_id, int sync, 
			JVMP_ShmRequest* *preq)
{
  int qflags = sync ? 0 : IPC_NOWAIT;
  JVMP_ShmMessage msg;
  char* buf;
  void* shmbuf;
  int len;
  
  /* this msgrcv() won't recieve system messages */
 restart:
  if ((msgrcv(msg_id, 
	      (struct msgbuf *)&msg, 
	      sizeof(msg)-sizeof(long), 
	      -JVMP_SHM_LIMIT, qflags)) ==-1)
    {  
      if (errno == EINTR && !g_terminate) goto restart;
      /* just to remove not needed output */
      if (!(((errno == ENOMSG || errno == EAGAIN) && !sync) || g_terminate)) 
	perror("JVMP_RecvShmRequest: msgrcv");
      *preq = NULL;
      return 0;
    }
  if ((shmbuf = JVMP_shmat(msg.shm_id, NULL, 0)) == (void*)-1)
    {
      perror("JVMP_RecvShmRequest: shmat");
      return 0;
    }
  len = msg.shm_len;
  buf = (char*)malloc(len);
  memcpy(buf, shmbuf, len);
  JVMP_DemarshallShmReq(buf, len, preq);
  free(buf);
  (*preq)->shmid   = msg.shm_id;
  (*preq)->shmdata = shmbuf;
  
  return 1;
}

int JVMP_ConfirmShmRequest(JVMP_ShmRequest* req)
{
  JVMP_ShmMessage msg;
  char*           buf;
  int             len;

  msg.mtype = req->rid + JVMP_SHM_LIMIT + 1;
  msg.message = req->retval;
  msg.shm_id = req->shmid;
  JVMP_MarshallShmReq(req, &buf, &len);
  /* can optimize it, using knowledge, that really segment size is whole pages
     number on some systems - see GetShmRealLen() */
  if (req->shmlen < len) 
    {
      JVMP_shmdt(req->shmdata, req->shmid);
      req->shmid = 
	JVMP_shmget(0, len, IPC_CREAT | IPC_EXCL | IPC_PRIVATE | 0660);
      if (req->shmid == -1)
	{
	  perror("JVMP_ConfirmShmRequest: shmget");
	  return 0;
	}
      if ((req->shmdata 
	   = JVMP_shmat(req->shmid, NULL, 0)) == (void*)-1)
	{
	  perror("JVMP_ConfirmShmRequest: shmat");
	  return 0;
	}
      req->shmlen = GetShmRealLen(len);
      if ((req->shmdata = JVMP_shmat(req->shmid, NULL, 0)) == (void*)-1)
	{
	  perror("JVMP_WaitConfirmShmRequest: shmat");
	  return 0;
	}
      msg.shm_id = req->shmid;
    }
  msg.shm_len = req->shmlen;
  memcpy(req->shmdata, buf, len);
  if ((msgsnd(req->msg_id, (struct msgbuf *)&msg, 
	      sizeof(msg)-sizeof(long), 0)) == -1)
    {
      perror("JVMP_ConfirmShmRequest: msgsnd");
      return 0;
    }
  return 1;
}

int JVMP_WaitConfirmShmRequest(JVMP_ShmRequest* req)
{
  JVMP_ShmMessage msg;
  char* buf;
  int   len;
  JVMP_ShmRequest* req1;
  
  //fprintf(stderr, "Waiting confirm %ld\n", req->rid + JVMP_SHM_LIMIT + 1);
 restart:
  if ((msgrcv(req->msg_id, 
	      (struct msgbuf *)&msg, 
	      sizeof(msg)-sizeof(long), 
	      req->rid + JVMP_SHM_LIMIT + 1, 0)) == -1)
    {  
      /* I know, it's wrong, but what else I can do 
	 to reliably restart after alarams? */
      if (errno == EINTR && !g_terminate) goto restart;
      if (!!g_terminate) perror("JVMP_WaitConfirmShmRequest: msgrcv");
      return 0;
    }
  req->retval = msg.message;
  req->shmlen = msg.shm_len;
  if (req->shmid != msg.shm_id) /* SHM segment changed - reattach to new */
    {
      JVMP_shmdt(req->shmdata, req->shmid);
      shmctl(req->shmid, IPC_RMID, NULL); /* nobody uses it */
      req->shmid = msg.shm_id;
      if ((req->shmdata = JVMP_shmat(req->shmid, NULL, 0)) == (void*)-1)
	{
	  perror("JVMP_WaitConfirmShmRequest: shmat");
	  return 0;
	}
    }
  len = req->shmlen;
  buf = (char*)malloc(len);
  memcpy(buf, req->shmdata, len);
  JVMP_DemarshallShmReq(buf, len, &req1);
  free(buf);
  free(req->argt); req->argt = req1->argt;
  req->length = req1->length;
  free(req->data); req->data = req1->data;
  req->nullmask = req1->nullmask;
  /* update call capabilities, if changed on another side.
     Higher level code can use it to update thread current capabilities,
     if some caps changing call happened. Maybe not need, commented out yet. */
  //memcpy(&req->caps, &req1->caps, sizeof(JVMP_SecurityCap));
  free(req1);
  //fprintf(stderr, "Got confirm %ld retval=%d\n", 
  //	  req->rid + JVMP_SHM_LIMIT + 1,  req->retval);
  return 1;
}


int JVMP_ShmMessageLoop(int msg_id)
{
  JVMP_ShmRequest* req;
  while (!g_terminate) 
    {
      if (!JVMP_RecvShmRequest(msg_id, 1, &req)) break;
      JVMP_ExecuteShmRequest(req);
      JVMP_ConfirmShmRequest(req);
      JVMP_DeleteShmReq(req);
    }
  JVMP_ShmShutdown();
  return 1;
}

static void JVMP_SigHandler(int sig)
{
  fprintf(stderr, "Got signal %d\n", sig);
  g_terminate = 1;
  JVMP_ShmShutdown();
  raise(sig);
}

int JVMP_ShmInit(int side)
{
  int i;
  struct sigaction sigact;

  for (i=0; i < MAX_SHM_MSG; i++) _msgs[i] = -1;
  for (i=0; i < MAX_SHM_MEM; i++) _shms[i] = -1;

  sigact.sa_handler = JVMP_SigHandler;
  sigact.sa_flags = 0;
  g_side = side;
  /* interrupts chaining */
  if (side == 1) 
    {
      /* host side - all signal handlers setting are safe */
      sigaction(SIGSEGV, &sigact, &(_sigs[SIGSEGV]));  
      sigaction(SIGCHLD, &sigact, &(_sigs[SIGCHLD]));
      //sigaction(SIGILL, &sigact, &(_sigs[SIGILL]));
    }
  else
    {
      /* JVM side - some signal handlers unsafe to set */
#ifndef _JVMP_SUNJVM
      /* Sun's JVM stupidly coredumps if change those handlers */
      sigaction(SIGSEGV, &sigact, &(_sigs[SIGSEGV]));  
      sigaction(SIGCHLD, &sigact, &(_sigs[SIGCHLD]));
#endif
    }
  sigaction(SIGINT, &sigact, &(_sigs[SIGINT]));
  sigaction(SIGKILL, &sigact, &(_sigs[SIGKILL]));
  sigaction(SIGTERM, &sigact, &(_sigs[SIGTERM]));
  atexit(&JVMP_ShmShutdown);
  return 1;
}

void JVMP_ShmShutdown()
{
  struct msqid_ds mctl;
  struct shmid_ds sctl;
  int i;

  for (i=0; i < MAX_SHM_MSG; i++)
    if (_msgs[i] != -1) { 
      msgctl(_msgs[i], IPC_RMID, &mctl); 
      _msgs[i] = -1;
    }
  for (i=0; i < MAX_SHM_MEM; i++)
    if (_shms[i] != -1) {
      shmctl(_shms[i], IPC_RMID, &sctl);
      _shms[i] = -1;
    }
  if (g_side == 1) 
    {
      sigaction(SIGSEGV, &(_sigs[SIGSEGV]), 0);
      sigaction(SIGCHLD, &(_sigs[SIGCHLD]), 0);
    }
  else
    {
      /* Sun's JVM stupidly coredumps if change those handlers */
#ifndef _JVMP_SUNJVM
      sigaction(SIGSEGV, &(_sigs[SIGSEGV]), 0);
      sigaction(SIGCHLD, &(_sigs[SIGCHLD]), 0);
#endif
    }
  sigaction(SIGINT, &(_sigs[SIGINT]), 0); 
  sigaction(SIGKILL, &(_sigs[SIGKILL]), 0);
  sigaction(SIGTERM, &(_sigs[SIGTERM]), 0);
  //sigaction(SIGILL, &(_sigs[SIGILL]), 0);
}

/*
JVMP_ShmRequest* JVMP_NewEncodedShmReq(int msg_id, unsigned int func_no,
				       char* sign, ...)
{
  JVMP_ShmRequest* req = JVMP_NewShmReq(msg_id, func_no);
  if (!req || !JVMP_EncodeRequest(req, sign, ...)) return 0;
  return 1;
}
*/








