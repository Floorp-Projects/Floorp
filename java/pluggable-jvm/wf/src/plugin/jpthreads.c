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
 * $Id: jpthreads.c,v 1.2 2001/07/12 19:58:16 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include "jpthreads.h"
#include <stdlib.h>

#ifdef _JVMP_PTHREADS

static pthread_key_t tid_key = (pthread_key_t) -1;
static int inited = 0;
static void cleanup(JVMP_Thread* thr)
{
  pthread_t tid;

  tid = pthread_self();
  if (!thr || (pthread_t)(thr->handle) != tid) 
    {
      fprintf(stderr, "ERROR: tid mismatch!!!!\n");
      return;
    }
  free(thr);
}

JVMP_Thread* ThreadInfoFromPthread()
{
  JVMP_Thread* thr = NULL;

  if (!inited)  
    {
      pthread_key_create(&tid_key,  NULL);
      inited = 1;
    }
  thr = pthread_getspecific(tid_key);
  if (thr) return thr;
  thr = (JVMP_Thread*)malloc(sizeof(JVMP_Thread));
  thr->handle = (long)pthread_self();
  thr->id = 0; /* not yet attached */
  thr->state = 0; /* XXX: find out state using pthread calls */
  thr->cleanup = &cleanup;
  thr->data = NULL;
  thr->ctx = NULL;
  pthread_setspecific(tid_key, thr);
  return thr;
}

#endif

#ifdef _JVMP_WIN32THREADS
static int inited = 0;

/* some ideas from JVM Win32 code - I have not so much experience 
   with low level Windows programming :( */
#define TLS_INVALID_INDEX 0xffffffffUL
static unsigned long tls_index = TLS_INVALID_INDEX;

static void cleanup(JVMP_Thread* thr)
{
  unsigned long tid;

  tid = (unsigned long)GetCurrentThread();
  if (!thr || (unsigned long)(thr->handle) != tid) 
    {
      fprintf(stderr, "ERROR: tid mismatch!!!!\n");
      return;
    }
  free(thr);
}

JVMP_Thread* ThreadInfoFromWthread()
{
  JVMP_Thread* thr = NULL;

  if (!inited)  
    {
      tls_index = TlsAlloc();
      if (tls_index == TLS_INVALID_INDEX) {
	  fprintf(stderr, "ERROR: cannot create TLS\n");
	  return NULL;
      }
      inited = 1;
    }
  thr = TlsGetValue(tls_index);
  if (thr) return thr;
  thr = (JVMP_Thread*)malloc(sizeof(JVMP_Thread));
  thr->handle = (long)GetCurrentThread();
  thr->id = 0; /* not yet attached */
  thr->state = 0; /* XXX: find out state using Win32 calls */
  thr->cleanup = &cleanup;
  thr->data = NULL;
  thr->ctx = NULL;
  TlsSetValue(tls_index, thr);
  return thr;
}
#endif


#ifdef XP_UNIX
dll_t JVMP_LoadDLL(const char* filename, int mode)
{
  return dlopen(filename, mode);
}

void* JVMP_FindSymbol(dll_t dll, const char* sym)
{
  return dlsym(dll, sym);
}

int   JVMP_UnloadDLL(dll_t dll)
{
  return dlclose(dll);
}

const char*  JVMP_LastDLLErrorString()
{
  return dlerror();
}

#endif

#ifdef XP_WIN32
dll_t JVMP_LoadDLL(const char* filename, int mode)
{
  return LoadLibrary(filename);
}

void* JVMP_FindSymbol(dll_t dll, const char* sym)
{
  return GetProcAddress(dll, sym);
}

int   JVMP_UnloadDLL(dll_t dll)
{
  /* XXX: fix me */
  return 0;
}

const char*  JVMP_LastDLLErrorString()
{
  static LPVOID lpMsgBuf = NULL;
  
  free(lpMsgBuf);
  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM |     
		FORMAT_MESSAGE_IGNORE_INSERTS,    
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR) &lpMsgBuf, 0, NULL ); 
  return lpMsgBuf;
}

#endif
