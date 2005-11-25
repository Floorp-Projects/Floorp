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
 * $Id: java_plugin_shm_host.c,v 1.3 2005/11/25 21:56:46 timeless%mozdev.org Exp $
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include "jvmp.h"
#include "shmtran.h"

#define PLUGIN_DEBUG 1
#define MAX_EXT 50

static int g_msg_id;
static JVMP_CallingContext* g_ctx = NULL;
static JVMP_Extension* g_extensions[MAX_EXT];


struct JVM_Methods {
  jint JNICALL (*JNI_GetDefaultJavaVMInitArgs)(void *args);
  jint JNICALL (*JNI_CreateJavaVM)(JavaVM **pvm, void **penv, void *args);
  jint JNICALL (*JNI_GetCreatedJavaVMs)(JavaVM **, jsize, jsize *);
}; 
typedef struct JVM_Methods JVM_Methods;

static JavaVM* JVMP_JVM_running = NULL;
static jobject JVMP_PluginInstance = NULL;
static jclass  JVMP_PluginClass = NULL;
static JVM_Methods JVMP_JVMMethods;
static char* JVMP_plugin_home = NULL;
static char* global_libpath = NULL;

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

#define LIBJVM "libjvm.so"

static jint __JVMP_addOption(JavaVMInitArgs* opt, char* str, void* extra) {
  jint numOp;

  numOp = opt->nOptions + 1;
  opt->options = realloc(opt->options, numOp * sizeof(JavaVMOption));
  if (!opt->options) 
    {
      PLUGIN_LOG("cannot realloc() memory for JVM options");
      return JNI_FALSE;
    };
  opt->options[numOp-1].optionString = str;
  opt->options[numOp-1].extraInfo = extra;
  opt->nOptions = numOp;
  return JNI_TRUE;
}

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

static jint loadJVM(const char* name) {
  void* handle;
  char* error;
  char *libpath, *newlibpath, *pluginlibpath;
  char *libjvm, *java_home, *jre_home;
  int   tmplen;
  struct stat sbuf;
  static int loaded = 0;

  typedef jint JNICALL (*JNI_GetDefaultJavaVMInitArgs_t)(void *args);
  typedef jint JNICALL (*JNI_CreateJavaVM_t)(JavaVM **pvm, void **penv, void *args);
  typedef jint JNICALL (*JNI_GetCreatedJavaVMs_t)(JavaVM **, jsize, jsize *);
  typedef jint JNICALL (*JNI_OnLoad_t)(JavaVM *vm, void *reserved);
  typedef void JNICALL (*JNI_OnUnload_t)(JavaVM *vm, void *reserved);
  #define JVM_RESOLVE(method) JVMP_JVMMethods.##method = \
    (method##_t) dlsym(handle, #method); \
  if ((error = dlerror()) != NULL)  { \
    PLUGIN_LOG2("dlsym: %s", error); \
    return JNI_FALSE; \
  }
  
  if (loaded) return JNI_TRUE;
  /* bzero() ? */
  memset(&JVMP_JVMMethods, 0, sizeof(JVMP_JVMMethods));
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
  //putenv(newlibpath);
  java_home = (char*) malloc(strlen(JVMP_plugin_home)+41);
  jre_home =  (char*) malloc(strlen(JVMP_plugin_home)+41);
  libjvm = (char*) malloc(strlen(JVMP_plugin_home)+strlen(name)+50);
#ifdef _JVMP_IBMJVM
  sprintf(java_home, "JAVAHOME=%s/java/jre", JVMP_plugin_home);
  sprintf(jre_home, "JREHOME=%s/java/jre", JVMP_plugin_home);
  sprintf(libjvm, "%s/java/jre/bin/classic/%s", JVMP_plugin_home,
	  name);
#endif
#ifdef _JVMP_SUNJVM
  sprintf(java_home, "JAVA_HOME=%s/java/jre/", JVMP_plugin_home);
  sprintf(jre_home, "JREHOME=%s/java/jre/", JVMP_plugin_home);  
  sprintf(libjvm, "%s/java/jre/lib/"ARCH"/hotspot/%s", JVMP_plugin_home,
	  name);
#endif
  //putenv(java_home);
  //putenv(jre_home);
  PLUGIN_LOG2("loading JVM from %s", libjvm);
  handle = dlopen (libjvm, RTLD_NOW);
  free (libjvm);
  if (!handle) {
    PLUGIN_LOG2("dlopen: %s", dlerror());
    return JNI_FALSE;
  };
  
  JVM_RESOLVE(JNI_GetDefaultJavaVMInitArgs);
  JVM_RESOLVE(JNI_CreateJavaVM);
  JVM_RESOLVE(JNI_GetCreatedJavaVMs);
  loaded = 1;
  return JNI_TRUE;
}

/* join Plugin's classpaths to the arguments passed to JVM */
static jint JVMP_initClassPath(void* args) {
  JavaVMInitArgs* opt;
  char *classpath, *newclasspath, *pluginclasspath, *bootclasspath;
  char *java_libpath, *java_home;
  char* str;
  int  i;

  opt = (JavaVMInitArgs*) args;
  for (i=0; i < (int)opt->nOptions; i++) 
    PLUGIN_LOG3("before option[%d]=%s", i, opt->options[i].optionString);
  
  /* add javaplugin classpath to standard classpath */
  classpath = getenv("CLASSPATH");
  if (!classpath) classpath = "";
  pluginclasspath = (char*) malloc(2*strlen(JVMP_plugin_home)+50);
  /* XXX: FIX ME - use both JARS and dirs */
  sprintf(pluginclasspath, 
	  "%s/classes:"
/*  #ifdef _JVMP_IBMJVM */
/*  	  "%s/java/jre/lib/rt.jar:" */
/*  #endif  */
/*  #ifdef _JVMP_SUNJVM */
/*  	  "%s/java/jre/lib/classes:" */
/*  #endif */
	  , JVMP_plugin_home);
  newclasspath = (char*) malloc(strlen(classpath) + 
				strlen(pluginclasspath) + 1);
  strcpy(newclasspath, pluginclasspath);
  if (classpath != NULL)  
    strcat(newclasspath, classpath);
  str = (char*) malloc(strlen(newclasspath)+30);
  sprintf(str, "-Djava.class.path=%s", newclasspath);
  if (!__JVMP_addOption(opt, str, NULL)) return JNI_FALSE; 

  java_libpath = (char*) malloc(strlen(global_libpath) + 30);
  sprintf(java_libpath, "-Djava.library.path=%s", global_libpath);
  if (!__JVMP_addOption(opt, java_libpath, NULL)) return JNI_FALSE; 

  java_home = (char*) malloc(strlen(JVMP_plugin_home)+30);
  sprintf(java_home, "-Djava.home=%s/java/jre", JVMP_plugin_home);
  if (!__JVMP_addOption(opt, java_home, NULL)) return JNI_FALSE;
  free(pluginclasspath);
  bootclasspath = (char*) malloc(3*strlen(JVMP_plugin_home)+180); 
  sprintf(bootclasspath, "-Xbootclasspath:" 
	  "%s/java/jre/classes:"
	  "%s/java/jre/lib/rt.jar:"
  	  "%s/java/jre/lib/i18n.jar",
	  JVMP_plugin_home, JVMP_plugin_home, JVMP_plugin_home
  	  );
  if (!__JVMP_addOption(opt, bootclasspath, NULL)) return JNI_FALSE; 
  for (i=0; i < (int)opt->nOptions; i++)  
    PLUGIN_LOG3("JVM option[%d]=%s", i, opt->options[i].optionString);
  return JNI_TRUE;
}


static jint JNICALL JVMP_initJavaClasses(JavaVM* jvm, JNIEnv* env) {
  jclass clz;
  jmethodID meth;
  jboolean trace = JNI_TRUE;
  jobject inst;

  (*env)->ExceptionClear(env);
  /* Find main plugin's class */
  clz = (*env)->FindClass(env, "sun/jvmp/generic/motif/Plugin");
  if (clz == 0) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return JNI_FALSE;
  };
  /* Find its "startJVM" method */
  meth = (*env)->GetStaticMethodID(env, clz, "startJVM", 
				   "(Z)Lsun/jvmp/PluggableJVM;");
  if ((*env)->ExceptionOccurred(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return JNI_FALSE;
  };
  inst = (*env)->CallStaticObjectMethod(env, clz, meth, trace);
  if ((*env)->ExceptionOccurred(env)) {      
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return JNI_FALSE;
  };
  JVMP_PluginInstance = (*env)->NewWeakGlobalRef(env, inst);
  JVMP_PluginClass = (*env)->NewWeakGlobalRef(env, clz);
  return JNI_TRUE;
}

/* prototype for this function */
static jint JVMP_GetCallingContext(JavaVM *jvm, 
				   JVMP_CallingContext **pctx, 
				   jint version,
				   JVMP_ThreadInfo* target);

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
  JVMP_FORBID_ALL_CAPS(ctx_new->caps);
  ctx_new->AllowCap = &JVMP_SetCap;
  ctx_new->GetCap = &JVMP_GetCap;
  ctx_new->SetCaps = &JVMP_SetCaps;  
  ctx_new->IsActionAllowed = &JVMP_IsActionAllowed;
  return ctx_new;
}

/* arguments are taken into account only if JVM isn't already created */
/* XXX: fix possible race conditions when different threads calls
   this method in the same time, as JNI_CreateJavaVM() is long.
   I've skipped it yet, as haven't decided which thread library to
   use. I can't use MonitorEnter/MonitorExit yet, as there is no 
   JNIEnv yet.
*/ 
static jint JNICALL JVMP_GetRunningJVM(JavaVM **pjvm, 
				       JVMP_CallingContext **pctx, 
				       void *args, jint allow_reuse) 
{
  JavaVM* jvm_new = NULL;
  JVMP_CallingContext* ctx_new = NULL;
  JNIEnv* env_new = NULL;
  jint jvm_err;

  if (JVMP_JVM_running == NULL) {
    if (!JVMP_initClassPath(args)) return JNI_FALSE;
    ctx_new = NewCallingContext();
    if (!ctx_new) return JNI_FALSE;
    if ((jvm_err = JVMP_JVMMethods.JNI_CreateJavaVM
	 (&jvm_new, (void*)&(ctx_new->env), args)) != JNI_OK)  
      {
	PLUGIN_LOG2("JVMP: JNI_CreateJavaVM failed due %d", (int)jvm_err);
	return jvm_err;
      }
    env_new = ctx_new->env;
  } else {
    (*pjvm) = JVMP_JVM_running;
    /* XXX: FIXME version */
    return JVMP_GetCallingContext(JVMP_JVM_running, 
				  pctx, 
				  JNI_VERSION_1_2,
				  NULL);
  };
    
  if (env_new) {
    if (!JVMP_initJavaClasses(jvm_new, env_new)) {
      PLUGIN_LOG("Problems with Java classes of JVMP. Exiting...");
      return JNI_FALSE;
    };
    JVMP_JVM_running = jvm_new;
    (*pjvm) = JVMP_JVM_running;
    (*pctx) = ctx_new;
    (*env_new)->ExceptionClear(env_new);
    return JNI_TRUE;  
  }
  else {
    return JNI_FALSE; 
  };
}

static jint JNICALL JVMP_StopJVM(JVMP_CallingContext* ctx) {
    jmethodID myMethod;
    JNIEnv* env = ctx->env;

    if (JVMP_JVM_running == NULL) return JNI_TRUE;
    myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
				   "stopPlugin", "()V");
    if ((*env)->ExceptionOccurred(env)) {      
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
	return JNI_FALSE;
    };
    (*env)->CallVoidMethod(env, JVMP_PluginInstance, myMethod);    
    if ((*env)->ExceptionOccurred(env)) {      
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return JNI_FALSE;
    };
    /* FIXME: I don't know how to completely destroy JVM.
       Current code waits until only one thread left - but how to kill 'em?
       Host application should be able to control such stuff as execution of 
       JVM, even if there are some java threads running
     */
    (*JVMP_JVM_running)->DestroyJavaVM(JVMP_JVM_running);
    JVMP_JVM_running = NULL;
    return JNI_TRUE;
}

static jint JNICALL JVMP_GetDefaultJavaVMInitArgs(void *args) 
{
  if (JVMP_JVMMethods.JNI_GetDefaultJavaVMInitArgs) 
    { 
      return JVMP_JVMMethods.JNI_GetDefaultJavaVMInitArgs(args);
    }
  return JNI_FALSE;
}

static jint JNICALL JVMP_RegisterWindow(JVMP_CallingContext* ctx, 
					JVMP_DrawingSurfaceInfo *win, 
					jint *pID) 
{
    jlong handle;
    jint ID;
    jmethodID myMethod;
    JNIEnv* env = ctx->env;
    
    PLUGIN_LOG("JVMP_RegisterWindow");
    if (win == NULL) return JNI_FALSE;

    handle = (jlong)(jint)(win->window);
    myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
				   "RegisterWindow", "(JII)I");
    if ((*env)->ExceptionOccurred(env)) 
      {      
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
	return JNI_FALSE;
      }
    
    /*  fprintf(stderr, "calling RegisterWindow()\n"); */
    ID = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
			       handle, win->width, win->height);    
    if ((*env)->ExceptionOccurred(env))
      {      
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
	return JNI_FALSE;
      }
    if (ID == 0) 
      { 
	PLUGIN_LOG("ID == 0");
	return JNI_FALSE; 
      };
    if (pID) *pID = ID;
    return JNI_TRUE;
}

static jint JNICALL JVMP_UnregisterWindow(JVMP_CallingContext* ctx, 
					  jint ID) 
{
  jmethodID myMethod;
  jint result;
  JNIEnv* env = ctx->env;

  if (ID <= 0) return JNI_FALSE;
  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
				 "UnregisterWindow", "(I)I");
  if ((*env)->ExceptionOccurred(env)) {      
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return JNI_FALSE;
  };
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ID);    
  if ((*env)->ExceptionOccurred(env)) 
    {      
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      return JNI_FALSE;
    };
  if (result != ID) return JNI_FALSE;
  return JNI_TRUE;
}

static jint JVMP_GetCallingContext(JavaVM *jvm, 
				   JVMP_CallingContext* *pctx, 
				   jint version,
				   JVMP_ThreadInfo* target)
{
  jint ret;
  JVMP_CallingContext* ctx_new = NewCallingContext(); 
  if (!ctx_new) return JNI_FALSE;
  g_ctx = ctx_new;
  ret = (*jvm)->GetEnv(jvm, (void*)&(ctx_new->env), version);
  return (ret == JNI_OK ? JNI_TRUE : JNI_FALSE);
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
  jmethodID myMethod = NULL;
  jint result = 0;
  JNIEnv* env = ctx->env;
  
  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
				 "createPeer", "(II)I");
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  return JNI_FALSE;
      };
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 hostApp, version);    
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  return JNI_FALSE;
      };
  if (result == 0) return JNI_FALSE;
  *target = result;
  return JNI_TRUE;
}

static jint JNICALL JVMP_SendEvent(JVMP_CallingContext* ctx,
				   jint   target,
				   jint   event,
				   jlong  data)
{
  jmethodID myMethod;
  jint result;
  JNIEnv* env = ctx->env;
  
  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
				 "sendEvent", "(IIJ)I");
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  return JNI_FALSE;
      };
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 target, event, data);    
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  return JNI_FALSE;
      };
  if (result == 0) return JNI_FALSE;
  return JNI_TRUE;
}

static jint JNICALL JVMP_PostEvent(JVMP_CallingContext* ctx,
				   jint   target,
				   jint   event,
				   jlong  data)
{
  jmethodID myMethod;
  jint result;
  JNIEnv* env = ctx->env;
  
  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
				 "postEvent", "(IIJ)I");
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  return JNI_FALSE;
      };
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 target, event, data);    
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  return JNI_FALSE;
      };
  if (result == 0) return JNI_FALSE;
  return JNI_TRUE;
}

static jint JNICALL JVMP_DestroyPeer(JVMP_CallingContext* ctx,
				     jint   target)
{
  
  jmethodID myMethod = NULL;
  jint result = 0;
  JNIEnv* env = ctx->env;
  
  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
				 "removePeer", "(I)I");
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  return JNI_FALSE;
      };
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 target);    
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  return JNI_FALSE;
      };
  if (result != target) return JNI_FALSE;
  return JNI_TRUE;
}


static jint JNICALL JVMP_RegisterExtension(JVMP_CallingContext* ctx,
					   const char* extPath,
					   jint *pID)
{
    void* handle;
    char* error;
    JVMP_GetExtension_t JVMP_GetExtension;
    JVMP_Extension* ext;
    jint vendorID, version, result;
    jmethodID myMethod = NULL;
    char *classpath, *classname;
    jstring jclasspath, jclassname;
    static int ext_idx = 0;
    
    JNIEnv* env = ctx->env;

    PLUGIN_LOG("JVMP_RegisterExtension - JVM");
    handle = dlopen(extPath, RTLD_NOW);
    if (!handle) {
	PLUGIN_LOG2("extension dlopen: %s", dlerror());
	return JNI_FALSE;
    };
    JVMP_GetExtension = 
	(JVMP_GetExtension_t)dlsym(handle, "JVMP_GetExtension");
    if ((error = dlerror()) != NULL)
	{
	    PLUGIN_LOG2("extension dlsym: %s", error);
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
    /* init it on JVM side */
    if ((ext->JVMPExt_Init)(2) != JNI_TRUE) return JNI_FALSE;
    g_extensions[ext_idx++] = ext;
    myMethod = 
	(*env)->GetMethodID(env, JVMP_PluginClass, 
			    "registerExtension",
			    "(IILjava/lang/String;Ljava/lang/String;)I");
     if ((*env)->ExceptionOccurred(env)) 
	 {      
	     (*env)->ExceptionDescribe(env);
	     (*env)->ExceptionClear(env);
	     return JNI_FALSE;
	 };
     (ext->JVMPExt_GetBootstrapClass)(&classpath, &classname);
     jclasspath = (*env)->NewStringUTF(env, classpath);
     jclassname = (*env)->NewStringUTF(env, classname);
     if (jclasspath == NULL || jclassname == NULL) return JNI_FALSE;
     result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				    vendorID, version, 
				    jclasspath, jclassname);
     /* XXX: maybe leak of jclasspath, jclassname - ReleaseStringUTFChars*/
     if ((*env)->ExceptionOccurred(env)) 
	 {      
	     (*env)->ExceptionDescribe(env);
	     (*env)->ExceptionClear(env);
	     return JNI_FALSE;
	 };
     *pID = result;
     
     return JNI_TRUE;
}

static jint JNICALL JVMP_UnregisterExtension(JVMP_CallingContext* ctx,
					     jint ID)
{
  return JNI_TRUE;
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
    if (!loadJVM(LIBJVM)) { 
      PLUGIN_LOG("BAD - JVM loading failed"); 
      return JNI_FALSE;
    }
    PLUGIN_LOG("OK - JVM dll at least loaded");
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

static int JVMP_ExecuteExtReq(JVMP_ShmRequest* req)
{
  int i;

  for (i=0; i<MAX_EXT; i++)
    {      
      if (g_extensions[i] != NULL && 
	  ((g_extensions[i])->JVMPExt_ScheduleRequest)(req, JNI_TRUE))
	break;
    }
  return 1;
}



int JVMP_ExecuteShmRequest(JVMP_ShmRequest* req)
{
  JavaVMInitArgs vm_args;
  JVMP_DrawingSurfaceInfo win, *pwin;
  jint ID, *pID;
  char* extPath;
  char sign[100];
  jint vendorID, version, *ptarget, target, event;
  jlong data, *pdata;

  if (!req) return 0;
  switch(req->func_no)
    {
    case 1:
      // JVMP_GetRunningJVM(JavaVM **jvm, 
      //                   JVMP_CallingContext* *pctx,  void *args)
      memset(&vm_args, 0, sizeof(vm_args));
      vm_args.version = JNI_VERSION_1_2;
      JVMP_GetDefaultJavaVMInitArgs((void*)&vm_args);      
      req->retval = JVMP_GetRunningJVM(&JVMP_JVM_running, 
				       &g_ctx, &vm_args, JNI_FALSE);
      break;
    case 2:
      // JVMP_StopJVM(JVMP_CallingContext* ctx)
      req->retval = JVMP_StopJVM(g_ctx);
      break;
    case 3:
      // jint JNICALL (*JVMP_GetDefaultJavaVMInitArgs)(void *args);
      req->retval = JNI_TRUE;
      break;
    case 4:
      /*  jint JNICALL (*JVMP_RegisterWindow)(JVMP_CallingContext* ctx, 
                                      JVMP_DrawingSurfaceInfo *win,
                                      jint *pID);
      */
      pwin = &win;
      pID = &ID;
      sprintf(sign, "A[%d]i", sizeof(JVMP_DrawingSurfaceInfo));
      JVMP_DecodeRequest(req, 0, sign, &pwin, &pID);
      req->retval = JVMP_RegisterWindow(g_ctx, pwin, pID);
      JVMP_EncodeRequest(req, sign, pwin, pID);      
      break;
    case 5:
      /*  jint JNICALL (*JVMP_UnregisterWindow)(JVMP_CallingContext* ctx, 
	                                        jint ID);
      */
      sprintf(sign, "I");
      JVMP_DecodeRequest(req, 0, sign, &ID);
      req->retval = JVMP_UnregisterWindow(g_ctx, ID);
      break;
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
      req->retval = JNI_FALSE;
      break;
    case 14:
      /* jint JNICALL (*JVMP_CreatePeer)(JVMP_CallingContext* ctx,
	                          jint   vendorID,
				  jint   version,
				  jint   *target); */
      sprintf(sign, "IIi");
      ptarget = &target;
      JVMP_DecodeRequest(req, 0, sign, &vendorID, &version, &ptarget);
      req->retval = JVMP_CreatePeer(g_ctx, vendorID, version, ptarget);
      JVMP_EncodeRequest(req, sign, vendorID, version, ptarget);
      break;
    case 15:
      /* jint JNICALL (*JVMP_SendEvent)(JVMP_CallingContext* ctx,
				 jint   target,
				 jint   event,
				 jlong  data); */
      sprintf(sign, "IIA[%d]", sizeof(jlong));
      pdata = &data;
      JVMP_DecodeRequest(req, 0, sign, &target, &event, &pdata);
      req->retval = JVMP_SendEvent(g_ctx, target, event, data);
      break;
    case 16:
      /* jint JNICALL (*JVMP_PostEvent)(JVMP_CallingContext* ctx,
				 jint   target,
				 jint   event,
				 jlong  data); */
      sprintf(sign, "IIA[%d]", sizeof(jlong));
      pdata = &data;
      JVMP_DecodeRequest(req, 0, sign, &target, &event, &pdata);
      req->retval = JVMP_PostEvent(g_ctx, target, event, data);
      break;
    case 17:
      /* jint JNICALL (*JVMP_DestroyPeer)(JVMP_CallingContext* ctx,
	 jint   target); */
      sprintf(sign, "I");
      JVMP_DecodeRequest(req, 0, sign, &target);
      req->retval = JVMP_DestroyPeer(g_ctx, target);
      break;
    case 18:
      /* jint JNICALL (*JVMP_RegisterExtension)(JVMP_CallingContext* ctx,
					 const char* extPath,
					 jint *pID); */
      sprintf(sign, "Si");
      JVMP_DecodeRequest(req, 1, sign, &extPath, &pID);
      req->retval = JVMP_RegisterExtension(g_ctx, extPath, pID);
      JVMP_EncodeRequest(req, sign, NULL, pID);
      free(extPath); 
      free(pID);
      break;
    case 19:
      /* jint JNICALL (*JVMP_UnregisterExtension)(JVMP_CallingContext* ctx,
	 jint ID); */
      sprintf(sign, "I");
      JVMP_DecodeRequest(req, 0, sign, &ID);
      req->retval = JVMP_UnregisterExtension(g_ctx, ID);
      break;
    default:
      return JVMP_ExecuteExtReq(req);
    }
  return 1;
}

int main(int argc, char** argv)
{
  int id = 0;
  JVMP_RuntimeContext* cx = NULL;
  /* one and only argument should be message queue id used to communicate
     with host application */ 
  if (argc != 2) return 1;
  id = atoi(argv[1]);
  g_msg_id = id;
  //sleep(30);
  JVMP_ShmInit(2); /* init it on JVM side */
  memset(g_extensions, 0, MAX_EXT*sizeof(JVMP_Extension*));
  if (!JVMP_GetPlugin(&cx) || !cx) return 1;
  JVMP_ShmMessageLoop(g_msg_id);
  return 0;
}






