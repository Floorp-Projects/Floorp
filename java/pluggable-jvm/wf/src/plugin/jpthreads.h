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
 * $Id: jpthreads.h,v 1.2 2001/07/12 19:58:16 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#include "jvmp.h"

#ifdef XP_WIN32
#include <windows.h>
#endif
#ifdef XP_UNIX
#include <dlfcn.h>
#endif


#ifdef _JVMP_PTHREADS
#include <pthread.h>
JVMP_Thread*             ThreadInfoFromPthread();
#define THIS_THREAD()    ThreadInfoFromPthread()
#define MUTEX_T          pthread_mutex_t
/* arguments are pointers */
#define MUTEX_CREATE(m)  m = (MUTEX_T*)malloc(sizeof(MUTEX_T)); pthread_mutex_init(m, NULL)   
#define MUTEX_LOCK(m)    pthread_mutex_lock(m)
#define MUTEX_UNLOCK(m)  pthread_mutex_unlock(m)
#define MUTEX_DESTROY(m) pthread_mutex_destroy(m); free(m)
#endif /* of #ifdef _JVMP_PTHREADS */

#ifdef _JVMP_WIN32THREADS
JVMP_Thread*             ThreadInfoFromWthread();
#define THIS_THREAD()    ThreadInfoFromWthread()
#define MUTEX_T          CRITICAL_SECTION
/* arguments are pointers */
#define MUTEX_CREATE(m)  m = (MUTEX_T*)malloc(sizeof(MUTEX_T)); InitializeCriticalSection(m)   
#define MUTEX_LOCK(m)    EnterCriticalSection(m)
#define MUTEX_UNLOCK(m)  LeaveCriticalSection(m)
#define MUTEX_DESTROY(m) free(m); 
#endif /* of #ifdef _JVMP_WIN32THREADS */


#ifdef XP_UNIX
#define dll_t   void*
#define DL_MODE RTLD_NOW
#define FILE_SEPARATOR "/"
#define PATH_SEPARATOR ":"
#endif

#ifdef XP_WIN32
#define dll_t HINSTANCE
#define DL_MODE 0
#define FILE_SEPARATOR "\\"
#define PATH_SEPARATOR ";"
#endif

/* DLL handling here */
dll_t        JVMP_LoadDLL(const char* filename, int mode);
void*        JVMP_FindSymbol(dll_t dll, const char* sym);
int          JVMP_UnloadDLL(dll_t dll);
const char*  JVMP_LastDLLErrorString();
