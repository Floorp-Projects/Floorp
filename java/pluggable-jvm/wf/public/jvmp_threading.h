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
 * $Id: jvmp_threading.h,v 1.2 2001/07/12 19:58:08 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

#ifndef _JVMP_THREADING_H
#define _JVMP_THREADING_H
#ifdef __cplusplus
extern "C" {
#endif

/* void pointers, used in MD way to refer on proper MD structures */ 
struct JPluginMonitor;
struct JVMP_CallingContext;
struct JVMP_Monitor
{
  /* field to be sure that our binary manipulations are correct */
  jint                     magic;
  /**
   * pointers to native structure describing mutex and monitor, like
   * pthread_mutex_t*, pthread_cond_t*   in pthread API
   * CRITICAL_SECTION*, ???              in Win32 threads, 
   * use macros JVMP_MONITOR_INITIALIZER with those argument
   * to initialize JVMP_Monitor.
   */
  struct JPluginMutex*     mutex;   
  struct JPluginMonitor*   monitor;
  struct JVMP_Thread*      owner;
  jint                     id;
  int                      monitor_state;
  int                      mutex_state;
};
typedef void (*JVMPThreadDumpProc)(struct JVMP_Thread *t, void *arg);
struct JVMP_Thread
{
  long                  handle;  /* MD thread handle */
  jint                  id;      /* unique TID */
  int                   state;
  void*                 data;
  void                  (*cleanup)(struct JVMP_Thread* me);
  JVMPThreadDumpProc    dump;    /* dump thread specific info */
  struct 
  JVMP_CallingContext*  ctx;     /* context assotiated with this thread */
};
typedef struct JVMP_Thread JVMP_Thread;
typedef struct JVMP_Monitor JVMP_Monitor;
/* backward compatibility */
typedef JVMP_Thread  JVMP_ThreadInfo;
typedef JVMP_Monitor JVMP_MonitorInfo;

#define JVMP_STATE_NOTINITED -2
/* to prevent occasional incorrect initialization of monitor */
#define JVMP_STATE_INCORRECT  0
#define JVMP_STATE_INITED     1
#define JVMP_STATE_LOCKED     2
#define JVMP_STATE_DESTROYED  3
#define JVMP_MONITOR_MAGIC    0xcafeface
#define JVMP_MONITOR_INITIALIZER(mutex, condvar)    \
   {JVMP_MONITOR_MAGIC, (JPluginMutex*)mutex, (JPluginMonitor*)condvar, \
     NULL, 0, JVMP_STATE_NOTINITED, JVMP_STATE_NOTINITED }
#define JVMP_ERROR_INCORRECT_MUTEX_STATE   -1000
#define JVMP_ERROR_MUTEX_NOT_LOCKED        -1001
#ifdef __cplusplus
};
#endif
#endif





