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
 * $Id: java_plugin_shm.c,v 1.2 2001/07/12 19:58:23 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

/* 
   all methods here but JVMP_GetPlugin() declared "static", as only access to 
   them is via structure returned by JVMP_GetPlugin(), so why to add needless
   records to export table of plugin DLL. Also it looks like JNI approach. 
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dlfcn.h>
#include "jvmp.h"
#include "shmtran.h"

//#define PLUGIN_DEBUG 1


static JavaVM* JVMP_JVM_running = NULL;
//static jobject JVMP_PluginInstance = NULL;
//static jclass  JVMP_PluginClass = NULL;
static char* JVMP_plugin_home = NULL;
static char* global_libpath = NULL;
static int   g_msgid;

#ifdef PLUGIN_DEBUG
#define PLUGIN_LOG(s1)  { fprintf (stderr, "Java Plugin: "); \
                          fprintf(stderr, s1); fprintf(stderr, "\n"); }
#define PLUGIN_LOG2(s1, s2) { fprintf (stderr, "Java Plugin: "); \
                        fprintf(stderr, s1, s2); fprintf(stderr, "\n"); }
#define PLUGIN_LOG3(s1, s2, s3) { fprintf (stderr, "Java Plugin: "); \
                        fprintf(stderr, s1, s2, s3); fprintf(stderr, "\n"); }
#define PLUGIN_LOG4(s1, s2, s3, s4) { fprintf (stderr, "Java Plugin: "); \
                        fprintf(stderr, s1, s2, s3 ,s4); fprintf(stderr, "\n"); }
#else
#define PLUGIN_LOG(s1) 
#define PLUGIN_LOG2(s1, s2) 
#define PLUGIN_LOG3(s1, s2, s3)
#define PLUGIN_LOG4(s1, s2, s3, s4)
#endif
#define JVMP_EXEC "jvmp_exec"

static char* getPluginLibPath(char* home) {
  static char* result = NULL;
  
  if (result != NULL) return result;
  
  result = (char*) malloc(3*strlen(home)+200);
  /* FIXME !! */
  sprintf(result,
#ifdef _JVMP_SUNJVM
	  "%s/java/jre/lib/"ARCH"/hotspot:%s/java/jre/lib/"ARCH":%s"
#endif
#ifdef _JVMP_IBMJVM
	  "%s:%s/java/jre/bin:%s/java/jre/bin/classic"
#endif
	, home, home, home);
  return result;
}  
static jint startSHM() {  
  static int started = 0;
  char *libpath, *newlibpath, *pluginlibpath;
  char *java_home, *jre_home;
  char *jvmp_exec; 
  pid_t pid;
  char* str;
  struct stat sbuf;
  int   tmplen;

  if (started) return JNI_TRUE;
  /* find out JAVA_PLUGIN_HOME */
  JVMP_plugin_home = getenv("JAVA_PLUGIN_HOME");
  if (JVMP_plugin_home == NULL) {
    PLUGIN_LOG("Env variable JAVA_PLUGIN_HOME not set");
    return JNI_FALSE;
  }
  if ((stat(JVMP_plugin_home, &sbuf) < 0) || !S_ISDIR(sbuf.st_mode)) {
    PLUGIN_LOG2("Bad value %s of JAVA_PLUGIN_HOME", JVMP_plugin_home);
    return JNI_FALSE;
  }
  /* LD_LIBRARY_PATH correction */
  
  pluginlibpath = getPluginLibPath(JVMP_plugin_home);
  libpath =  getenv("LD_LIBRARY_PATH");
  if (!libpath) libpath = "";
  tmplen = strlen(libpath) + strlen(pluginlibpath);
  global_libpath =  (char*) malloc(tmplen + 2);
  newlibpath = (char*) malloc(tmplen + 22);
  
  sprintf(global_libpath, "%s:%s", pluginlibpath, libpath);
  sprintf(newlibpath, "LD_LIBRARY_PATH=%s", global_libpath);
  putenv(newlibpath);
  java_home = (char*) malloc(strlen(JVMP_plugin_home)+40);
  jre_home =  (char*) malloc(strlen(JVMP_plugin_home)+40);
#ifdef _JVMP_IBMJVM
  sprintf(java_home, "JAVAHOME=%s/java/jre", JVMP_plugin_home);
  sprintf(jre_home, "JREHOME=%s/java/jre", JVMP_plugin_home);
#endif
#ifdef _JVMP_SUNJVM
  sprintf(java_home, "JAVA_HOME=%s/java/jre/", JVMP_plugin_home);
  sprintf(jre_home, "JREHOME=%s/java/jre/", JVMP_plugin_home);
#endif
  putenv(java_home);
  putenv(jre_home);
  JVMP_ShmInit(1); /* init transport on host side */
  if ((g_msgid = JVMP_msgget (0, IPC_CREAT | IPC_PRIVATE | 0770)) == -1)
    {
      perror("host: msgget");
      return JNI_FALSE;
    }
  if ((pid = fork()) == 0)
    {
      /* child */
      str = (char*)malloc(20);
      jvmp_exec = (char*)malloc(strlen(JVMP_EXEC)+
				strlen(JVMP_plugin_home)+2);
      sprintf(jvmp_exec, "%s/%s", JVMP_plugin_home, JVMP_EXEC);
      sprintf(str, "%d", g_msgid);
      execlp(jvmp_exec, "jvmp_exec", str, NULL);
      perror("exec");
      exit(1);
    }
  started = 1;
  return JNI_TRUE;
}

static jint JVMP_SetCap(JVMP_CallingContext *ctx, jint cap_no)
{
  JVMP_ALLOW_CAP(ctx->caps, cap_no);
  return JNI_TRUE;
}

static jint JVMP_SetCaps(JVMP_CallingContext *ctx,
			 JVMP_SecurityCap *new_caps)
{
  if (!new_caps) return JNI_FALSE;
  memcpy(&(ctx->caps), new_caps, sizeof(JVMP_SecurityCap));
  return JNI_TRUE;
}

static jint JVMP_GetCap(JVMP_CallingContext *ctx, jint cap_no)
{
  return JVMP_IS_CAP_ALLOWED(ctx->caps, cap_no);
}



static jint JVMP_IsActionAllowed(JVMP_CallingContext *ctx,
				 JVMP_SecurityAction *caps)
{
  int i, r; 
  for(i=0, r=1; i < JVMP_MAX_CAPS_BYTES; i++)
    {
      r = 
	r && ((!(caps->bits)[i]) |
	      (((ctx->caps).bits)[i] & (caps->bits)[i]));
      if (!r) return JNI_FALSE;
    }
  return JNI_TRUE;
} 

static JVMP_CallingContext* NewCallingContext()
{
  JVMP_CallingContext* ctx_new = NULL;

  ctx_new = (JVMP_CallingContext *) malloc(sizeof(JVMP_CallingContext));
  if (!ctx_new) return NULL;
  ctx_new->source_thread = NULL; /* must be current thread */
  ctx_new->env = NULL;
  ctx_new->source = NULL;
  ctx_new->reserved0 = (void*)g_msgid;
  JVMP_FORBID_ALL_CAPS(ctx_new->caps);
  ctx_new->AllowCap = &JVMP_SetCap;
  ctx_new->GetCap = &JVMP_GetCap;
  ctx_new->SetCaps = &JVMP_SetCaps;  
  ctx_new->IsActionAllowed = &JVMP_IsActionAllowed;
  return ctx_new;
}


/* prototype for this function */
static jint JVMP_GetCallingContext(JavaVM *jvm, 
				   JVMP_CallingContext **pctx, 
				   jint version,
				   JVMP_ThreadInfo* target);
/* Arguments are taken into account only if JVM isn't already created.
   No clear what to do with JVM reusing - just ignore for now.
 */
static jint JNICALL JVMP_GetRunningJVM(JavaVM **pjvm, 
				       JVMP_CallingContext **pctx, 
				       void *args, jint allow_reuse) 
{
  JavaVM* jvm_new = NULL;
  JVMP_CallingContext* ctx_new = NULL;
  JVMP_ShmRequest* req;
  jint retval;

  if (JVMP_JVM_running == NULL) {
    ctx_new = NewCallingContext();
    if (!ctx_new) return JNI_FALSE;
    *pctx = ctx_new;
    jvm_new = (JavaVM*)malloc(sizeof(JavaVM));
    if (!jvm_new) return JNI_FALSE;
    memset(jvm_new, 0, sizeof(JavaVM));
    req = JVMP_NewShmReq(g_msgid, 1);
    if (!JVMP_EncodeRequest(req, "") 
	|| !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
    JVMP_WaitConfirmShmRequest(req);
    retval = req->retval;
    JVMP_DeleteShmReq(req);
  } else {
    (*pjvm) = JVMP_JVM_running;
    /* XXX: FIXME version */
    return JVMP_GetCallingContext(JVMP_JVM_running, 
				  pctx, 
				  JNI_VERSION_1_2,
				  NULL);
  };
  
  return retval;  
}

static jint JNICALL JVMP_StopJVM(JVMP_CallingContext* ctx) {
    JVMP_ShmRequest* req;
    jint retval;

    if (JVMP_JVM_running == NULL) return JNI_TRUE;
    req = JVMP_NewShmReq(g_msgid, 2);
    if (!JVMP_EncodeRequest(req, "") 
	|| !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
    JVMP_WaitConfirmShmRequest(req);
    retval = req->retval;
    JVMP_DeleteShmReq(req);
    return retval;
}

static jint JNICALL JVMP_GetDefaultJavaVMInitArgs(void *args) 
{
  return JNI_TRUE;
}

static jint JNICALL JVMP_RegisterWindow(JVMP_CallingContext* ctx, 
					JVMP_DrawingSurfaceInfo *win, 
					jint *pID) 
{
    JVMP_ShmRequest* req;
    jint retval;
    char sign[50];

    PLUGIN_LOG("JVMP_RegisterWindow");
    if (win == NULL) return JNI_FALSE;
    sprintf(sign, "A[%d]i", sizeof(JVMP_DrawingSurfaceInfo));
    req = JVMP_NewShmReq(g_msgid, 4);    
    if (!JVMP_EncodeRequest(req, sign, win, pID) 
	|| !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
    JVMP_WaitConfirmShmRequest(req);
    retval = req->retval;
    JVMP_DecodeRequest(req, 0/* not alloc references */, sign,
		     &win, &pID);
    JVMP_DeleteShmReq(req);
    return retval;
}

static jint JNICALL JVMP_UnregisterWindow(JVMP_CallingContext* ctx, 
					  jint ID) 
{  
  JVMP_ShmRequest* req;
  jint retval;
  char sign[50];
  
  PLUGIN_LOG("JVMP_UnregisterWindow");
  sprintf(sign, "I");
  req = JVMP_NewShmReq(g_msgid, 5);    
  if (!JVMP_EncodeRequest(req, sign, ID) 
      || !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
  JVMP_WaitConfirmShmRequest(req);
  retval = req->retval;
  JVMP_DeleteShmReq(req);
  return retval;
}

static jint JVMP_GetCallingContext(JavaVM *jvm, 
				   JVMP_CallingContext* *pctx, 
				   jint version,
				   JVMP_ThreadInfo* target)
{
  JVMP_CallingContext* ctx_new = NewCallingContext();
  if (!ctx_new) return JNI_FALSE;
  if (!target) 
    ctx_new->dest_thread = ctx_new->source_thread;
  else
    ctx_new->dest_thread = target;
  ctx_new->reserved0 = (void*)g_msgid;
  *pctx = ctx_new;
  return JNI_TRUE;
}

static jint JNICALL JVMP_RegisterMonitor(JVMP_CallingContext* ctx, 
					 JVMP_MonitorInfo *monitor, 
					 jint *pID) 
{  
  return JNI_FALSE;
}

static jint JNICALL JVMP_UnregisterMonitor(JVMP_CallingContext* ctx, 
					   jint ID) 
{
  return JNI_FALSE;
}

static jint JNICALL JVMP_MonitorEnter(JVMP_CallingContext* ctx, 
				      jint ID) 
{
  return JNI_FALSE;
}

static jint JNICALL JVMP_MonitorExit(JVMP_CallingContext* ctx, 
				     jint ID) 
{
  return JNI_FALSE;
}

static jint JNICALL JVMP_MonitorWait(JVMP_CallingContext* ctx, 
				     jint ID, jlong milli)
{
  return JNI_FALSE;
}
static jint JNICALL JVMP_MonitorNotify(JVMP_CallingContext* ctx, 
				       jint ID)
{
  return JNI_FALSE;
}
static jint JNICALL JVMP_MonitorNotifyAll(JVMP_CallingContext* ctx, 
					  jint ID)
{
  return JNI_FALSE;
}
static jint JNICALL JVMP_CreatePeer(JVMP_CallingContext* ctx,
				    jint   hostApp,
				    jint   version,
				    jint   *target)
{  
  JVMP_ShmRequest* req;
  jint retval;
  char sign[50];
  
  PLUGIN_LOG("JVMP_CreatePeer");
  sprintf(sign, "IIi");
  req = JVMP_NewShmReq(g_msgid, 14);    
  if (!JVMP_EncodeRequest(req, sign, hostApp, hostApp, target) 
      || !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
  JVMP_WaitConfirmShmRequest(req);
  retval = req->retval;
  JVMP_DecodeRequest(req, 0/* not alloc references */, sign,
		     NULL, NULL, &target);
  JVMP_DeleteShmReq(req);
  return retval;
}


static jint JNICALL JVMP_SendEvent(JVMP_CallingContext* ctx,
				   jint   target,
				   jint   event,
				   jlong  data)
{
  JVMP_ShmRequest* req;
  jint retval;
  char sign[50];
  
  PLUGIN_LOG("JVMP_SendEvent");
  sprintf(sign, "IIA[%d]", sizeof(jlong));
  req = JVMP_NewShmReq(g_msgid, 15);
  if (!JVMP_EncodeRequest(req, sign, target, event, &data)
      || !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
  JVMP_WaitConfirmShmRequest(req);
  retval = req->retval;
  JVMP_DeleteShmReq(req);
  return retval;
}

static jint JNICALL JVMP_PostEvent(JVMP_CallingContext* ctx,
				   jint   target,
				   jint   event,
				   jlong  data)
{
  JVMP_ShmRequest* req;
  jint retval;
  char sign[50];
  
  PLUGIN_LOG("JVMP_PostEvent");
  sprintf(sign, "IIA[%d]", sizeof(jlong));
  req = JVMP_NewShmReq(g_msgid, 16);    
  if (!JVMP_EncodeRequest(req, sign, target, event, &data) 
      || !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
  JVMP_WaitConfirmShmRequest(req);
  retval = req->retval;
  JVMP_DeleteShmReq(req);
  return retval;
}

static jint JNICALL JVMP_DestroyPeer(JVMP_CallingContext* ctx,
				     jint   target)
{
  JVMP_ShmRequest* req;
  jint retval;
  char sign[50];
  
  PLUGIN_LOG("JVMP_DestroyPeer");
  sprintf(sign, "I");
  req = JVMP_NewShmReq(g_msgid, 17);    
  if (!JVMP_EncodeRequest(req, sign, target) 
      || !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
  JVMP_WaitConfirmShmRequest(req);
  retval = req->retval;
  JVMP_DeleteShmReq(req);
  return retval;
}

static jint JNICALL JVMP_RegisterExtension(JVMP_CallingContext* ctx,
					   const char* extPath,
					   jint *pID)
{
  JVMP_ShmRequest* req;
  jint retval;
  char sign[50];
  void* handle;
  JVMP_GetExtension_t JVMP_GetExtension;
  JVMP_Extension* ext;
  jint vendorID, version;
 
  PLUGIN_LOG("JVMP_RegisterExtension - Netscape");
  /* firstly, load it into the host */
  
  handle = dlopen(extPath, RTLD_NOW);
  if (!handle) {
    PLUGIN_LOG2("extension dlopen: %s", dlerror());
    return JNI_FALSE;
  };
  JVMP_GetExtension = 
    (JVMP_GetExtension_t)dlsym(handle, "JVMP_GetExtension");
  if (handle == NULL)
    {
      PLUGIN_LOG2("extension dlsym: %s", dlerror());
      return JNI_FALSE;
    }
  /* to implement JVMP_UnregisterExtension we should support
     GHashTable of opened extensions, but later */
  if (((*JVMP_GetExtension)(&ext) != JNI_TRUE))
    {
      PLUGIN_LOG2("Cannot obtain extension from %s", extPath);
      return JNI_FALSE;
    }
  (ext->JVMPExt_GetExtInfo)(&vendorID, &version);
  /* init it on host side */
  if ((ext->JVMPExt_Init)(1) != JNI_TRUE) return JNI_FALSE;
  /* then ask slave to do it */
  sprintf(sign, "Si");
  req = JVMP_NewShmReq(g_msgid, 18);    
  if (!JVMP_EncodeRequest(req, sign, extPath, pID) 
      || !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
  JVMP_WaitConfirmShmRequest(req);
  retval = req->retval;
  JVMP_DecodeRequest(req, 0/* not alloc references */, sign,
		     NULL, &pID);
  JVMP_DeleteShmReq(req);
  return retval;
}

static jint JNICALL JVMP_UnregisterExtension(JVMP_CallingContext* ctx,
					     jint ID)
{
  JVMP_ShmRequest* req;
  jint retval;
  char sign[50];
  
  PLUGIN_LOG("JVMP_UnregisterExtension");
  sprintf(sign, "I");
  req = JVMP_NewShmReq(g_msgid, 19);    
  if (!JVMP_EncodeRequest(req, sign, ID) 
      || !JVMP_SendShmRequest(req, 1))
    {
      return JNI_FALSE;
    }
  JVMP_WaitConfirmShmRequest(req);
  retval = req->retval;
  JVMP_DeleteShmReq(req);
  return retval;
} 

/* Here main structure is initialized */

static JVMP_RuntimeContext JVMP_PluginContext = {
    &JVMP_GetRunningJVM, 
    &JVMP_StopJVM, 
    &JVMP_GetDefaultJavaVMInitArgs,
    &JVMP_RegisterWindow, 
    &JVMP_UnregisterWindow,
    &JVMP_RegisterMonitor,
    &JVMP_UnregisterMonitor,
    &JVMP_MonitorEnter,
    &JVMP_MonitorExit,
    &JVMP_MonitorWait,
    &JVMP_MonitorNotify,
    &JVMP_MonitorNotifyAll,
    &JVMP_GetCallingContext,
    &JVMP_CreatePeer,
    &JVMP_SendEvent,
    &JVMP_PostEvent,
    &JVMP_DestroyPeer,
    &JVMP_RegisterExtension,
    &JVMP_UnregisterExtension
};

jint JNICALL JVMP_GetPlugin(JVMP_RuntimeContext** cx) {
    if (cx == NULL) return JNI_FALSE;
    if (!startSHM()) { 
      PLUGIN_LOG("BAD - SHM not started"); 
      return JNI_FALSE;
    } 
    PLUGIN_LOG("OK - SHM started");
    (*cx) = &JVMP_PluginContext;
    return JNI_TRUE;
}


#ifdef _JVMP_PTHREADS
JVMP_ThreadInfo* ThreadInfoFromPthread(pthread_t thr)
{
  JVMP_ThreadInfo* res = 
    (JVMP_ThreadInfo*)malloc(sizeof(JVMP_ThreadInfo));
  /* pthread_t is unsigned int on Linux and Solaris :) */
  res->handle = (void*)thr;
  return res;
}
#endif /* of pthread case */



