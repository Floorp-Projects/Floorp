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
 * $Id: java_plugin.c,v 1.4 2005/11/25 21:56:46 timeless%mozdev.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */

/**
 * Main Waterfall C file. Contains implementation of Waterfall API 
 *  for in-process case 
 **/

/* all methods here but JVMP_GetPlugin() declared "static", as only access to 
   them is via structure returned by JVMP_GetPlugin(), so why to add needless
   records to export table of plugin DLL. Also it looks like JNI approach. */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#ifdef XP_UNIX
#include <unistd.h>
#endif
#include <string.h>
#include "jvmp.h"
#include "jpthreads.h"

struct JVM_Methods {
  jint (JNICALL *JNI_GetDefaultJavaVMInitArgs)(void *args);
  jint (JNICALL *JNI_CreateJavaVM)(JavaVM **pvm, void **penv, void *args);
  jint (JNICALL *JNI_GetCreatedJavaVMs)(JavaVM **, jsize, jsize *);
}; 
typedef struct JVM_Methods JVM_Methods;

static JVMP_RuntimeContext    JVMP_PluginContext;
static JVMP_PluginDescription JVMP_Description;
static JavaVM*                JVMP_JVM_running = NULL;
static jobject                JVMP_PluginInstance = NULL;
static jclass                 JVMP_PluginClass = NULL;
static JVM_Methods            JVMP_JVMMethods;
static char*                  JVMP_plugin_home = NULL;
static char*                  JDK_home = NULL;
static char*                  global_libpath = NULL;
static MUTEX_T*               jvm_mutex = NULL;
  

#define PLUGIN_LOG(s1)  { fprintf (stderr, "WF: "); \
                          fprintf(stderr, s1); fprintf(stderr, "\n"); }
#define PLUGIN_LOG2(s1, s2) { fprintf (stderr, "WF: "); \
                        fprintf(stderr, s1, s2); fprintf(stderr, "\n"); }
#define PLUGIN_LOG3(s1, s2, s3) { fprintf (stderr, "WF: "); \
                        fprintf(stderr, s1, s2, s3); fprintf(stderr, "\n"); }
#define PLUGIN_LOG4(s1, s2, s3, s4) { fprintf (stderr, "WF: "); \
                        fprintf(stderr, s1, s2, s3 ,s4); fprintf(stderr, "\n"); }
#ifdef XP_UNIX
#define LIBJVM "libjvm.so"
#endif
#ifdef XP_WIN32
#define LIBJVM "jvm.dll"
#endif

/* default JNI version used by Waterfall */ 
#ifdef  WF_PJAVA
#define DEFAULT_JNI_VERSION JNI_VERSION_1_1
#else
#define DEFAULT_JNI_VERSION JNI_VERSION_1_2
#endif
/* level of verbosity printed with PlugableJVM.trace method:
   lesser number - lesser output */
#define DEBUG_LEVEL         10

/* XXX: localization */
static char* g_jvmp_errors[] =
{
  "no error",
  "no such class",
  "no such method",
  "file not found",
  "remote transport failed",
  "failed due unknown reasons",
  "out of memory",
  "null pointer passed",
  "initialization failed",
  "forbidden to perform this action",
  "java exception happened while handling this call",
  "java method failed",
  "invalid arguments",
  "those capabilities are not granted",
  NULL
};

static jint JVMP_addOption(JavaVMInitArgs* opt, char* str, void* extra) {
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

static void UpdateCaps(JVMP_CallingContext* ctx)
{
    JNIEnv* env = ctx->env;
    if (!env) return;
    if (ctx->jcaps == NULL)
	{
	    ctx->jcaps = (*env)->NewByteArray(env, sizeof(JVMP_SecurityCap));
	    if (ctx->jcaps == NULL) return;
	}
    (*env)->SetByteArrayRegion(env, ctx->jcaps, 
			       0, sizeof(JVMP_SecurityCap), 
			       (jbyte*)(ctx->caps).bits);    
}

static char* createURL(char* proto, char* dir, char* name)
{
    char *res;
    int i, len;
    char *newdir;
    
    len = strlen(dir); 
    newdir = (char*)malloc(len +1);
    for (i=0; i<=len; i++)
	{	    
	    if (dir[i] == '\\') 
	      newdir[i] = '/'; 
	    else
	      newdir[i] = dir[i];
	}
    res = (char*)malloc(strlen(proto)+strlen(newdir)+strlen(name)+5);
    sprintf(res, "%s:%s/%s", proto, newdir, name);
    free(newdir);
    return res;
}

/* returns JNI_TRUE if there is acceptable for reuse Java VM */
static jint JVMP_ReuseJVM(JavaVM* *pjvm)
{
  JavaVM* vm;
  jsize   nVMs;
  
  nVMs = 0;
  if (!(JVMP_JVMMethods.JNI_GetCreatedJavaVMs)) 
    return JNI_FALSE;
  JVMP_JVMMethods.JNI_GetCreatedJavaVMs(&vm, sizeof(vm), &nVMs);
  if (!nVMs) return JNI_FALSE;
  *pjvm = vm;
  return JNI_TRUE;
}

static char* getPluginLibPath(char* jdkhome, char* wfhome) {
  static char* result = NULL;
  
  if (result != NULL) return result;
  result = (char*) malloc(strlen(wfhome) + strlen(jdkhome)+50);
  /* FIXME !! */
  sprintf(result,
#ifdef _JVMP_SUNJVM
	  "%s"PATH_SEPARATOR"%s"FILE_SEPARATOR"jre"FILE_SEPARATOR"lib"FILE_SEPARATOR""ARCH
#endif
#ifdef _JVMP_IBMJVM
	  "%s"PATH_SEPARATOR"%s"FILE_SEPARATOR"jre"FILE_SEPARATOR"bin"
#endif
	, wfhome, jdkhome);
  return result;
}  

static char* getWFhome()
{
#ifdef XP_WIN32
  struct _stat sbuf;
#else
  struct stat sbuf;
#endif
   /* find out WFHOME */
  JVMP_plugin_home = getenv("WFHOME");
  if (JVMP_plugin_home == NULL) 
    {
      PLUGIN_LOG("Env variable WFHOME not set");
      return NULL;
    }
#ifdef XP_WIN32
  if ((_stat(JVMP_plugin_home, &sbuf) < 0) || !((sbuf.st_mode) & _S_IFDIR)) 
#else
  if ((stat(JVMP_plugin_home, &sbuf) < 0) || !S_ISDIR(sbuf.st_mode)) 
#endif
    {
      PLUGIN_LOG2("Bad value %s of WFHOME", JVMP_plugin_home);
      return NULL;
    }
  return JVMP_plugin_home;
}

static char* getJDKhome()
{
  JDK_home = getenv("WFJDKHOME");
  if (JDK_home == NULL) 
    {
      PLUGIN_LOG("Env variable JDKHOME not set");
      return NULL;
    }
  return JDK_home;
}



static jint loadJVM(const char* name) {
  dll_t handle;
  char *libpath, *newlibpath, *pluginlibpath;
  char *libjvm, *java_home, *jre_home, *libhpi;
  int   tmplen;
  static int loaded = 0;

  typedef jint (JNICALL *JNI_GetDefaultJavaVMInitArgs_t)(void *args);
  typedef jint (JNICALL *JNI_CreateJavaVM_t)(JavaVM **pvm, void **penv, void *args);
  typedef jint (JNICALL *JNI_GetCreatedJavaVMs_t)(JavaVM **, jsize, jsize *);
  typedef jint (JNICALL *JNI_OnLoad_t)(JavaVM *vm, void *reserved);
  typedef void (JNICALL *JNI_OnUnload_t)(JavaVM *vm, void *reserved);
#define JVM_RESOLVE(method) JVMP_JVMMethods.##method = \
    (method##_t) JVMP_FindSymbol(handle, #method); \
  if (JVMP_JVMMethods.##method == NULL)  { \
    PLUGIN_LOG2("FindSymbol: %s", JVMP_LastDLLErrorString()); \
    return JNI_FALSE; \
  }
  
  if (loaded) return JNI_TRUE;
  /* bzero() ? */
  memset(&JVMP_JVMMethods, 0, sizeof(JVMP_JVMMethods)); 
  if (!getWFhome()) return JNI_FALSE;
  if (!getJDKhome()) return JNI_FALSE;
  /* LD_LIBRARY_PATH correction */
  pluginlibpath = getPluginLibPath(JDK_home, JVMP_plugin_home);
  /* pretty much useless on Win32 :) */
  libpath =  getenv("LD_LIBRARY_PATH");
  if (!libpath) libpath = "";
  tmplen = strlen(libpath) + strlen(pluginlibpath);
  global_libpath =  (char*) malloc(tmplen + 2);
  newlibpath = (char*) malloc(tmplen + 22);
  
  sprintf(global_libpath, "%s:%s", pluginlibpath, libpath);
  free(pluginlibpath);
  sprintf(newlibpath, "LD_LIBRARY_PATH=%s", global_libpath);
  putenv(newlibpath);
  java_home = (char*) malloc(strlen(JDK_home)+40);
  jre_home =  (char*) malloc(strlen(JDK_home)+40);
  libjvm = (char*) malloc(strlen(JDK_home)+strlen(name)+50);
#ifdef _JVMP_IBMJVM
  sprintf(java_home, 
	  "JAVAHOME=%s"FILE_SEPARATOR"jre", JDK_home);
  sprintf(jre_home, 
	  "JREHOME=%s"FILE_SEPARATOR"jre", JDK_home);
  sprintf(libjvm, 
	  "%s"FILE_SEPARATOR"jre"FILE_SEPARATOR"bin"FILE_SEPARATOR""JVMKIND""FILE_SEPARATOR"%s", JDK_home,
	  name);
#endif
#ifdef _JVMP_SUNJVM
  sprintf(java_home, 
	  "JAVA_HOME=%s"FILE_SEPARATOR"jre", JDK_home);
  sprintf(jre_home, 
	  "JREHOME=%s"FILE_SEPARATOR"jre", JDK_home);
#ifdef XP_UNIX
  sprintf(libjvm, "%s/jre/lib/"ARCH"/"JVMKIND"/%s", JDK_home,
	  name);
  /* this stuff meaningful for strange platforms like Sparc Linux
     where libjvm.so uses libhpi.so, but doesn't depends on it */
  libhpi = (char*) malloc(strlen(JDK_home)+60);
  sprintf(libhpi, 
          "%s/jre/lib/"ARCH"/native_threads/libhpi.so", 
          JDK_home
          );
  /* we don't care about possible errors - just try to load libjvm.so later */
  dlopen(libhpi, RTLD_GLOBAL | RTLD_NOW);
  free (libhpi);
#else
  sprintf(libjvm, 
	  "%s"FILE_SEPARATOR"jre"FILE_SEPARATOR"bin"FILE_SEPARATOR""JVMKIND""FILE_SEPARATOR"%s", 
	  JDK_home, name);
#endif

#endif
  putenv(java_home);
  putenv(jre_home);
  /* putenv behaves differently in different cases, so safer not to free it */
  /*
    free(java_home);
    free(jre_home);
    free(newlibpath); */
  /* PLUGIN_LOG2("loading JVM from %s", libjvm); */
  handle = JVMP_LoadDLL (libjvm, DL_MODE);
  free (libjvm);
  if (!handle) {
    PLUGIN_LOG2("LoadDLL: %s", JVMP_LastDLLErrorString());
    return JNI_FALSE;
  }  
  JVM_RESOLVE(JNI_GetDefaultJavaVMInitArgs);
  JVM_RESOLVE(JNI_CreateJavaVM);
  JVM_RESOLVE(JNI_GetCreatedJavaVMs);
  loaded = 1;
  return JNI_TRUE;
}

/* join Plugin's classpaths to the arguments passed to JVM */
static jint JVMP_initClassPath(void* *pargs) {
  JavaVMInitArgs* opt;
  char *classpath, *newclasspath, *pluginclasspath;
  char *bootclasspath, *policypath, *jvmphome, *policyurl;
  char *java_libpath, *java_home;
  char* str;
  int  i;
  
  if (!pargs) return JNI_FALSE;
  opt = (JavaVMInitArgs*) *pargs;
  if (opt == NULL) 
      {
	  opt = (JavaVMInitArgs*)malloc(sizeof(JavaVMInitArgs));
	  memset(opt, 0, sizeof(JavaVMInitArgs));
	  opt->version = DEFAULT_JNI_VERSION;
      }
  for (i=0; i < (int)opt->nOptions; i++) 
    PLUGIN_LOG3("before option[%d]=%s", i, opt->options[i].optionString);
  
  /* add javaplugin classpath to standard classpath */
  classpath = getenv("CLASSPATH");
  if (!classpath) classpath = "";
  pluginclasspath = (char*) malloc(2*strlen(JVMP_plugin_home)+50);
  sprintf(pluginclasspath, 
	  "%s"FILE_SEPARATOR"classes"PATH_SEPARATOR"%s"FILE_SEPARATOR"wf.jar"	  
	  , JVMP_plugin_home, JVMP_plugin_home);
  newclasspath = (char*) malloc(strlen(classpath) + 
				strlen(pluginclasspath) + 1);
  strcpy(newclasspath, pluginclasspath);
  if (classpath != NULL) {
    strcat(newclasspath, PATH_SEPARATOR);
    strcat(newclasspath, classpath);
  }
  str = (char*) malloc(strlen(newclasspath)+30);
  sprintf(str, "-Djava.class.path=%s", newclasspath);
  if (!JVMP_addOption(opt, str, NULL)) return JNI_FALSE; 

  java_libpath = (char*) malloc(strlen(global_libpath) + 30);
  sprintf(java_libpath, "-Djava.library.path=%s", global_libpath);
  if (!JVMP_addOption(opt, java_libpath, NULL)) return JNI_FALSE; 

  java_home = (char*) malloc(strlen(JVMP_plugin_home)+30);
  sprintf(java_home, "-Djava.home=%s"FILE_SEPARATOR"jre", JDK_home);
  if (!JVMP_addOption(opt, java_home, NULL)) return JNI_FALSE;
  free(pluginclasspath);

  bootclasspath = (char*) malloc(3*strlen(JVMP_plugin_home)+180); 
  sprintf(bootclasspath, "-Xbootclasspath:"
	  "%s"FILE_SEPARATOR"jre"FILE_SEPARATOR"classes"PATH_SEPARATOR
	  "%s"FILE_SEPARATOR"jre"FILE_SEPARATOR"lib"FILE_SEPARATOR"rt.jar"PATH_SEPARATOR
  	  "%s"FILE_SEPARATOR"jre"FILE_SEPARATOR"lib"FILE_SEPARATOR"i18n.jar",
	  JDK_home, JDK_home, JDK_home
  	  );
  if (!JVMP_addOption(opt, bootclasspath, NULL)) 
    return JNI_FALSE;

  if (!JVMP_addOption(opt, "-Djava.security.manager", NULL)) 
    return JNI_FALSE;

  jvmphome = (char*) malloc(strlen(JVMP_plugin_home)+20);
  sprintf(jvmphome, "-Djvmp.home=%s", JVMP_plugin_home);
  if (!JVMP_addOption( opt, jvmphome, NULL)) 
    return JNI_FALSE;

  policyurl = createURL("file", JVMP_plugin_home, "plugin.policy");
  policypath = (char*) malloc(strlen(policyurl)+30);
  sprintf(policypath, "-Djava.security.policy=%s", policyurl);
  free(policyurl);
  if (!JVMP_addOption( opt, policypath, NULL)) 
    return JNI_FALSE;

  for (i=0; i < (int)opt->nOptions; i++)  
    PLUGIN_LOG3("JVM option[%d]=%s", i, opt->options[i].optionString);  
  *pargs = opt;
  return JNI_TRUE;
}


static jint JNICALL JVMP_initJavaClasses(JavaVM* jvm, JNIEnv* env) {
  jclass clz;
  jmethodID meth;
  jobject   inst;
  jstring   jhome;

#ifdef XP_WIN32
#define WF_MAIN_CLASS "sun/jvmp/generic/win32/Plugin"
#endif
#ifdef XP_UNIX
#define WF_MAIN_CLASS "sun/jvmp/generic/motif/Plugin"
#endif

  (*env)->ExceptionClear(env);
  /* Find main plugin's class */
  
  clz = (*env)->FindClass(env, WF_MAIN_CLASS);
  if (clz ==  NULL) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return JNI_FALSE;
  }
  jhome = (*env)->NewStringUTF(env, JVMP_plugin_home);
  
  /* Find its "startJVM" method */
  meth = (*env)->GetStaticMethodID(env, clz, "startJVM", 
				   "(Ljava/lang/String;I)Lsun/jvmp/PluggableJVM;");
  if ((*env)->ExceptionOccurred(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);
    return JNI_FALSE;
  };
  inst = (*env)->CallStaticObjectMethod(env, clz, meth, jhome, DEBUG_LEVEL);
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
static jint JNICALL JVMP_GetCallingContext(JVMP_CallingContext **pctx);
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
  return JVMP_IS_CAP_ALLOWED(ctx->caps, cap_no) ? JNI_TRUE : JNI_FALSE;
}



static jint JVMP_IsActionAllowed(JVMP_CallingContext *ctx,
				 JVMP_SecurityAction *caps)
{
  int i, r; 
  for(i=0, r=1; i < JVMP_MAX_CAPS_BYTES; i++)
    {
      r = 
	r && ((~(caps->bits)[i]) |
	      (((ctx->caps).bits)[i] & (caps->bits)[i]));
      if (!r) return JNI_FALSE;
    }
  return JNI_TRUE;
} 
static jint JNICALL JVMP_AttachCurrentThread(JVMP_CallingContext* ctx, 
					     jint *pID);
static jint JNICALL JVMP_DetachCurrentThread(JVMP_CallingContext* ctx);
static JVMP_CallingContext* NewCallingContext()
{
  JVMP_CallingContext* ctx_new = NULL;
  jint tid;

  ctx_new = (JVMP_CallingContext *) malloc(sizeof(JVMP_CallingContext));
  if (!ctx_new) return NULL;
  ctx_new->This = ctx_new;
  ctx_new->source_thread = THIS_THREAD();
  ctx_new->env = NULL;
  ctx_new->data = NULL;
  ctx_new->err = JVMP_ERROR_NOERROR;
  ctx_new->source_thread->ctx = ctx_new;

  JVMP_FORBID_ALL_CAPS(ctx_new->caps);
  /* basic capabilities of every calling context */
  JVMP_ALLOW_CAP(ctx_new->caps, JVMP_CAP_SYS_PARITY);
  JVMP_ALLOW_CAP(ctx_new->caps, JVMP_CAP_SYS_CHCAP);
  /* nothing useful here, as no JNIEnv yet. Later call UpdateCaps() */
  ctx_new->jcaps = NULL;
  ctx_new->AllowCap = &JVMP_SetCap;
  ctx_new->GetCap = &JVMP_GetCap;
  ctx_new->SetCaps = &JVMP_SetCaps;  
  ctx_new->IsActionAllowed = &JVMP_IsActionAllowed;
  if (JVMP_JVM_running != NULL) JVMP_AttachCurrentThread(ctx_new, &tid);
  return ctx_new;
}
static void DestroyCallingContext(JVMP_CallingContext* ctx)
{  
  free(ctx);
}
static jint ValidCtx(JVMP_CallingContext* ctx)
{
  JVMP_Thread* thr;
  if (!ctx) return JNI_FALSE;
  thr = THIS_THREAD();
  if (ctx->source_thread != thr) 
    {
      PLUGIN_LOG("USED FROM INCORRECT THREAD. JVMP_CallingContext is per-thread structure");
      return JNI_FALSE;
    }
  /* clean up errors in this context */
  ctx->err = JVMP_ERROR_NOERROR;
  return JNI_TRUE;
}

static jint JNICALL JVMP_GetDescription(JVMP_PluginDescription* *pdesc)
{
  if (!pdesc) return JNI_FALSE;
  *pdesc = &JVMP_Description;
  return JNI_TRUE;
}

/* arguments are taken into account only if JVM isn't already created */
static jint JNICALL JVMP_GetRunningJVM(JVMP_CallingContext **pctx, 
				       void* args,
				       jint allow_reuse) 
{
  JavaVM* jvm_new = NULL;
  JVMP_CallingContext* ctx_new = NULL;
  JNIEnv* env_new = NULL;
  jint jvm_err, tid;
  
  MUTEX_LOCK(jvm_mutex);
  if (JVMP_JVM_running == NULL) 
  {
    if (!JVMP_initClassPath(&args)) return JNI_FALSE;
    ctx_new = NewCallingContext();
    if (!ctx_new) return JNI_FALSE;
     /* let's start with attempt to find out is Java VM already loaded */
    if (allow_reuse && JVMP_ReuseJVM(&jvm_new))
      {
	(*jvm_new)->GetEnv(jvm_new, (void**)&(ctx_new->env), DEFAULT_JNI_VERSION);
      }
    else if ((jvm_err = JVMP_JVMMethods.JNI_CreateJavaVM
	      (&jvm_new, (void**)&(ctx_new->env), args)) != JNI_OK)  
      {
	PLUGIN_LOG2("JVMP: JNI_CreateJavaVM failed due %d", (int)jvm_err);
	MUTEX_UNLOCK(jvm_mutex);
	return jvm_err;
      }
    
    env_new = ctx_new->env;
    JVMP_JVM_running = jvm_new;
    JVMP_PluginContext.jvm = jvm_new;
  } else {
    /* XXX: FIXME version */
    MUTEX_UNLOCK(jvm_mutex);
    return JVMP_GetCallingContext(pctx);
  };
    
  if (env_new) {
    if (!JVMP_initJavaClasses(jvm_new, env_new)) {
      PLUGIN_LOG("Problems with Java classes of JVMP. Cannot continue.");
      JVMP_JVM_running = NULL;
      MUTEX_UNLOCK(jvm_mutex);
      return JNI_FALSE;
    };
    (*pctx) = ctx_new;
    (*env_new)->ExceptionClear(env_new);
     /* this call from NewCallingContext() not executed, as JVM was NULL */
    JVMP_AttachCurrentThread(ctx_new, &tid);
    MUTEX_UNLOCK(jvm_mutex);
    return JNI_TRUE;  
  }
  else {
    MUTEX_UNLOCK(jvm_mutex);
    return JNI_FALSE; 
  };
}
static jint JNICALL JVMP_SendSysEvent(JVMP_CallingContext* ctx,
				      jint   event,
				      jlong  data,
				      jint   priority)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  
  
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSEVENT) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "sendSysEvent", "([BIJI)I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, event, data, priority);
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      }
  if (result == 0) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  return JNI_TRUE;
}

static jint JNICALL JVMP_PostSysEvent(JVMP_CallingContext* ctx,
				      jint   event,
				      jlong  data,
				      jint   priority)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  
  
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSEVENT) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "postSysEvent", "([BIJI)I");
	  if (myMethod == NULL) 
	      {      		
		  (*env)->ExceptionDescribe(env);		  
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, event, data, priority);
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);	  
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      }
  if (result == 0) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  return JNI_TRUE;
}

static jint JNICALL JVMP_StopJVM(JVMP_CallingContext* ctx) {
    static  jmethodID myMethod=NULL;
    JNIEnv* env;
    jint    res;

    if (!ValidCtx(ctx)) return JNI_FALSE;
    if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSTEM) != JNI_TRUE)
	{
	    ctx->err = JVMP_ERROR_NOACCESS;
	    return JNI_FALSE;
	}
    env = ctx->env;
    if (JVMP_JVM_running == NULL) return JNI_TRUE;
    if (!myMethod)
	{
	    myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					   "stopJVM", "([B)I");
	    if (myMethod == NULL) {      
		(*env)->ExceptionDescribe(env);
		(*env)->ExceptionClear(env);
		ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		return JNI_FALSE;
	    }
	}
    UpdateCaps(ctx);
    res = (*env)->CallIntMethod(env, JVMP_PluginInstance, 
				myMethod, ctx->jcaps);    
    if ((*env)->ExceptionOccurred(env)) {      
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      ctx->err = JVMP_ERROR_JAVAEXCEPTION;
      return JNI_FALSE;
    }
    /* if stopJVM returns > 0, it means there are some not unregistered
       extensions, so not try to kill JVM */
    if (res > 0) return JNI_TRUE;
    /* FIXME: I don't know how to completely destroy JVM.
       Current code waits until only one thread left - but how to kill 'em?
       Host application should be able to control such stuff as execution of 
       JVM, even if there are some java threads running
     */
    (*JVMP_JVM_running)->DestroyJavaVM(JVMP_JVM_running);
    JVMP_JVM_running = NULL;
    return JNI_TRUE;
}

static jint JNICALL JVMP_RegisterWindow(JVMP_CallingContext* ctx, 
					JVMP_DrawingSurfaceInfo *win, 
					jint *pID) 
{
    jint handle;
    jint ID=0;
    static jmethodID myMethod=NULL;
    JNIEnv* env;
    
    if (!ValidCtx(ctx)) return JNI_FALSE;
    if (win == NULL) return JNI_FALSE;
    if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSTEM) != JNI_TRUE)
	{
	    ctx->err = JVMP_ERROR_NOACCESS;
	    return JNI_FALSE;
	}
    env = ctx->env;
    handle = (jint)(win->window);
    if (!myMethod)
	{
	    myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					   "RegisterWindow", "(III)I");
	    if (myMethod == NULL) 
		{      
		    (*env)->ExceptionDescribe(env);
		    (*env)->ExceptionClear(env);
		    ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		    return JNI_FALSE;
		}
	}
    ID = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
			       handle, win->width, win->height);    
    if ((*env)->ExceptionOccurred(env))
      {      
	(*env)->ExceptionDescribe(env);
	(*env)->ExceptionClear(env);
	ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	return JNI_FALSE;
      }
    if (ID == 0) 
      { 
	ctx->err = JVMP_ERROR_METHODFAILED;
	return JNI_FALSE; 
      };
    if (pID) *pID = ID;
    return JNI_TRUE;
}

static jint JNICALL JVMP_UnregisterWindow(JVMP_CallingContext* ctx, 
					  jint ID) 
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env;

  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSTEM) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  if (ID <= 0) 
    {
      ctx->err = JVMP_ERROR_INVALIDARGS;
      return JNI_FALSE;
    }
  env = ctx->env;
  if (!myMethod) 
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "UnregisterWindow", "(I)I");
	  if (myMethod == NULL) {      
	      (*env)->ExceptionDescribe(env);
	      (*env)->ExceptionClear(env);
	      ctx->err = JVMP_ERROR_NOSUCHMETHOD;
	      return JNI_FALSE;
	  }
      }
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ID);    
  if ((*env)->ExceptionOccurred(env)) 
    {      
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      ctx->err = JVMP_ERROR_JAVAEXCEPTION;
      return JNI_FALSE;
    };
  if (result != ID) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  return JNI_TRUE;
}

static jint JNICALL JVMP_GetCallingContext(JVMP_CallingContext* *pctx)
{
  jint ret;
  JVMP_CallingContext* ctx_new;
  JVMP_Thread*  thr = THIS_THREAD();

  if (thr->ctx) 
    {
      *pctx = thr->ctx;
      return JNI_TRUE;
    }
  ctx_new = NewCallingContext();
  if (!ctx_new) return JNI_FALSE;
  ret = (*JVMP_JVM_running)->GetEnv(JVMP_JVM_running, 
				    (void**)&(ctx_new->env), 
				    DEFAULT_JNI_VERSION);
  *pctx = ctx_new;
  return (ret == JNI_OK ? JNI_TRUE : JNI_FALSE);
}
/* registers native object, that consist of pair 
   mutex/condvar and can be used to synchronize execution of Java and
   native threads.
*/
static jint JNICALL JVMP_RegisterMonitorObject(JVMP_CallingContext* ctx, 
					       JVMP_MonitorInfo *monitor, 
					       jint *pID) 
{  
  jint ID=0;
  static jmethodID myMethod=NULL;
  JNIEnv* env;
  
  if (!ValidCtx(ctx)) return JNI_FALSE;
  /* we can register only correctly
     (using JVMP_MONITOR_INITIALIZER macros) initialized monitors */
  if ((monitor == NULL) || (monitor->monitor_state != JVMP_STATE_NOTINITED) ||
      (monitor->mutex_state != JVMP_STATE_NOTINITED))
      return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSTEM) != JNI_TRUE)
    {
      ctx->err = JVMP_ERROR_NOACCESS;
      return JNI_FALSE;
    }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "RegisterMonitorObject", "(J)I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  monitor->owner = THIS_THREAD();
  ID = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
			     PTR_TO_JLONG(monitor));    
  if ((*env)->ExceptionOccurred(env))
    {      
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      ctx->err = JVMP_ERROR_JAVAEXCEPTION;
      return JNI_FALSE;
    }
  if (ID == 0) return JNI_FALSE;
  monitor->id = ID;
  monitor->monitor_state = JVMP_STATE_INITED;
  monitor->mutex_state   = JVMP_STATE_INITED;
  if (pID) *pID = ID;
  return JNI_TRUE;
}

static jint JNICALL JVMP_UnregisterMonitorObject(JVMP_CallingContext* ctx, 
						 jint ID) 
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env;

  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ID <= 0) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSTEM) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "UnregisterMonitorObject", "(I)I");
	  if (myMethod == NULL) {      
	      (*env)->ExceptionDescribe(env);
	      (*env)->ExceptionClear(env);
	      ctx->err = JVMP_ERROR_NOSUCHMETHOD;
	      return JNI_FALSE;
	  }
      }
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ID);    
  if ((*env)->ExceptionOccurred(env)) 
    {      
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      ctx->err = JVMP_ERROR_JAVAEXCEPTION;
      return JNI_FALSE;
    };
  if (result != ID) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  return JNI_TRUE;
}

static jint JNICALL JVMP_AttachCurrentThread(JVMP_CallingContext* ctx, 
					     jint *pID)
{
  static jmethodID myMethod = NULL;
  jint result = 0;
  JNIEnv* env;
  
  /* 
     to prevent people from using this method incorrectly - GetCallingContext
     should be used instead
  */
  if (!ValidCtx(ctx)) return JNI_FALSE;
  result = (*JVMP_JVM_running)->AttachCurrentThread(JVMP_JVM_running, 
						    (void**)&(ctx->env), NULL);
  env = ctx->env;
  if (result != JNI_OK) return JNI_FALSE;
  
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "attachThread", "()I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod);    
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  if (result == 0) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  ctx->source_thread->id = result;
  *pID = result;
  return JNI_TRUE;
}

static jint JNICALL JVMP_DetachCurrentThread(JVMP_CallingContext* ctx)
{
  static jmethodID myMethod = NULL;
  jint result = 0;
  JNIEnv* env;
  
  if (!ValidCtx(ctx)) return JNI_FALSE;
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "detachThread", "()I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod);    
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      }
  if (result == 0) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  (*JVMP_JVM_running)->DetachCurrentThread(JVMP_JVM_running);
  if (ctx->source_thread->cleanup) 
      ctx->source_thread->cleanup(ctx->source_thread);
  DestroyCallingContext(ctx);
  return JNI_TRUE;
}

static jint JNICALL JVMP_CreatePeer(JVMP_CallingContext* ctx,
				    const char*          cid,
				    jint                 version,
				    jint                 *target)
{
  static jmethodID myMethod = NULL;
  jint             result = 0;
  JNIEnv*          env = NULL;
  jstring          jcid = NULL;

  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_EVENT) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "createPeer", "([BLjava/lang/String;I)I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  jcid = (*env)->NewStringUTF(env, cid);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, jcid, version);    
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      }
  if (result == 0) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  *target = result;
  return JNI_TRUE;
}

static jint JNICALL JVMP_SendEvent(JVMP_CallingContext* ctx,
				   jint   target,
				   jint   event,
				   jlong  data,
				   jint   priority)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  
  
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_EVENT) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "sendEvent", "([BIIJI)I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      };
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, target, event, data, priority);
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      }
  return result;
}

static jint JNICALL JVMP_PostEvent(JVMP_CallingContext* ctx,
				   jint   target,
				   jint   event,
				   jlong  data,
				   jint   priority)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_EVENT) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "postEvent", "([BIIJI)I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, target, event, data, priority);  
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  if (result == 0) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  return JNI_TRUE;
}

static jint JNICALL JVMP_DestroyPeer(JVMP_CallingContext* ctx,
				     jint   target)
{
  
  jmethodID myMethod = NULL;
  jint result = 0;
  JNIEnv* env;

  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_EVENT) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env  = ctx->env;
  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
				 "destroyPeer", "([BI)I");
  if (myMethod == NULL) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
	  return JNI_FALSE;
      };
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, target);    
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  if (result != target) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  return JNI_TRUE;
}


static jint JNICALL JVMP_RegisterExtension(JVMP_CallingContext* ctx,
					   const char* extPath,
					   jint *pID,
					   jlong data)
{
    dll_t handle;
    JVMP_GetExtension_t JVMP_GetExtension;
    JVMP_Extension* ext;
    const char*   cid=NULL;
    jint version, result=0;
    static jmethodID myMethod = NULL;
    char *classpath, *classname;
    jstring jclasspath=NULL, jclassname=NULL, jcid=NULL;
    JNIEnv* env;
    JVMP_SecurityCap caps, sh_mask;
    jbyteArray jcaps, jmask;
 
    if (!ValidCtx(ctx)) return JNI_FALSE;
    if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSTEM) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
    env = ctx->env;
    handle = JVMP_LoadDLL(extPath, DL_MODE);
    if (!handle) {
	PLUGIN_LOG2("extension LoadDLL: %s", JVMP_LastDLLErrorString());
	return JNI_FALSE;
    };
    JVMP_GetExtension = 
	(JVMP_GetExtension_t)JVMP_FindSymbol(handle, "JVMP_GetExtension");
    if (JVMP_GetExtension == NULL)
	{
	    PLUGIN_LOG2("extension FindSymbol: %s", JVMP_LastDLLErrorString());
	    return JNI_FALSE;
	}
    /* to implement JVMP_UnregisterExtension we should support
       GHashTable of opened extensions, but later */
    if (((*JVMP_GetExtension)(&ext) != JNI_TRUE))
	{
	    PLUGIN_LOG2("Cannot obtain extension from %s", extPath);
	    ctx->err = JVMP_ERROR_INITFAILED;
	    return JNI_FALSE;
	}
    (ext->JVMPExt_GetExtInfo)(&cid, &version, NULL);
    if ((ext->JVMPExt_Init)(0) != JNI_TRUE) 
      {
	ctx->err = JVMP_ERROR_INITFAILED;
	return JNI_FALSE;
      }
    /* get capability range reserved by this extension */ 
    if ((ext->JVMPExt_GetCapsRange)(&caps, &sh_mask) != JNI_TRUE) 
      {
	ctx->err = JVMP_ERROR_INITFAILED;
	return JNI_FALSE;
      }
    jcaps = (*env)->NewByteArray(env, sizeof(JVMP_SecurityCap));
    if (jcaps == NULL) return JNI_FALSE;
    (*env)->SetByteArrayRegion(env, jcaps, 
			       0, sizeof(JVMP_SecurityCap), (jbyte*)caps.bits);
    jmask = (*env)->NewByteArray(env, sizeof(JVMP_SecurityCap));
    if (jmask == NULL) return JNI_FALSE;
    (*env)->SetByteArrayRegion(env, jmask, 
			       0, sizeof(JVMP_SecurityCap), 
			       (jbyte*)sh_mask.bits);
    if (!myMethod)
	{
	    myMethod = 
		(*env)->GetMethodID(env, JVMP_PluginClass, 
				    "registerExtension",
				    "(Ljava/lang/String;ILjava/lang/String;Ljava/lang/String;[B[BJ)I");
	    if (myMethod == NULL) 
		{
		    (*env)->ExceptionDescribe(env);
		    (*env)->ExceptionClear(env);
		    ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		    return JNI_FALSE;
		}
	}
    
    (ext->JVMPExt_GetBootstrapClass)(&classpath, &classname);
    jclasspath = (*env)->NewStringUTF(env, classpath);
    jclassname = (*env)->NewStringUTF(env, classname);
    jcid       = (*env)->NewStringUTF(env, cid);
    if (jclasspath == NULL || jclassname == NULL || jcid == NULL) 
	{
	    ctx->err = JVMP_ERROR_INITFAILED;
	    return JNI_FALSE;
       }
    
     result = (*env)->CallIntMethod(env, 
				    JVMP_PluginInstance,
				    myMethod, 
				    jcid, 
				    version, 
				    jclasspath,
				    jclassname, 
				    jcaps, 
				    jmask,
				    data);
     
     /* XXX: maybe leak of jclasspath, jclassname, jcaps - ReleaseStringUTFChars*/
     if ((*env)->ExceptionOccurred(env)) 
	 {      
	     (*env)->ExceptionDescribe(env);
	     (*env)->ExceptionClear(env);
	     ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	     return JNI_FALSE;
	 }
     *pID = result;
     return JNI_TRUE;
}

static jint JNICALL JVMP_SendExtensionEvent(JVMP_CallingContext* ctx,
					    jint   target,
					    jint   event,
					    jlong  data,
					    jint   priority)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  
  
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_EVENT) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "sendExtensionEvent", "([BIIJI)I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, target, event, data, priority); 
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  if (result == 0) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  return JNI_TRUE;
}

static jint JNICALL JVMP_PostExtensionEvent(JVMP_CallingContext* ctx,
					    jint   target,
					    jint   event,
					    jlong  data,
					    jint   priority)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_EVENT) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "postExtensionEvent", "([BIIJI)I");
	  
	  if (myMethod == NULL)
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      };
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, target, event, data, priority);  
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  if (result == 0) 
    {
      ctx->err = JVMP_ERROR_METHODFAILED;
      return JNI_FALSE;
    }
  return JNI_TRUE;
}


static jint JNICALL JVMP_DirectPeerCall(JVMP_CallingContext* ctx,
					jint                 target,
					jint                 arg1,
					jlong                arg2)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_CALL) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "handleCall", "([BIIIJ)I");
	  if (myMethod == NULL)
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, 1, target, arg1, arg2);  
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  return result;
}

static jint JNICALL JVMP_DirectExtCall(JVMP_CallingContext*  ctx,
				       jint                  target,
				       jint                  arg1,
				       jlong                 arg2)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_CALL) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "handleCall", "([BIIIJ)I");
	  if (myMethod == NULL)
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, 2, target, arg1, arg2);  
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  return result;
}

static jint JNICALL JVMP_DirectJVMCall(JVMP_CallingContext*    ctx,
				       jint                    arg1,
				       jlong                   arg2)
{
  static jmethodID myMethod=NULL;
  jint result;
  JNIEnv* env; 
  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSCALL) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "handleCall", "([BIIJ)I");
	  if (myMethod == NULL)
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, 3, arg1, arg2);  
  if ((*env)->ExceptionOccurred(env)) 
      {
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  return result;
}

static jint JNICALL JVMP_UnregisterExtension(JVMP_CallingContext* ctx,
					     jint ID)
{
  jint result;
  JNIEnv* env;
  static jmethodID myMethod = NULL;

  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_SYSTEM) != JNI_TRUE)
    {
      ctx->err = JVMP_ERROR_NOACCESS;
      return JNI_FALSE;
    }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = (*env)->GetMethodID(env, JVMP_PluginClass, 
					 "unregisterExtension", "([BI)I");
	  if (myMethod == NULL)
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  UpdateCaps(ctx);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, myMethod, 
				 ctx->jcaps, ID);  
  if ((*env)->ExceptionOccurred(env)) 
    {
      (*env)->ExceptionDescribe(env);
      (*env)->ExceptionClear(env);
      ctx->err = JVMP_ERROR_JAVAEXCEPTION;
      return JNI_FALSE;
    }
  return result;
}

static jint JNICALL JVMP_EnableCapabilities(JVMP_CallingContext* ctx,
					    JVMP_SecurityCap*    caps,
					    jint                 num_principals,
					    jint*                principals_len,
					    jbyte*               *principals)
{
  JNIEnv* env;
  static jmethodID myMethod = NULL;
  jbyteArray jcaps = NULL, jprinc;
  jobjectArray  jprincipals = NULL;
  jint result, i;
  jclass byteArrayClass;

  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_CHCAP) != JNI_TRUE)
      {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env = ctx->env;
  if (!myMethod)
      {
	  myMethod = 
	      (*env)->GetMethodID(env, JVMP_PluginClass, 
				  "enableCapabilities",
				  "([B[[B)I");
	  if (myMethod == NULL) 
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
  byteArrayClass = (*env)->FindClass(env, "[B");
  jprincipals = (*env)->NewObjectArray(env, num_principals, 
				       byteArrayClass, NULL);
  if (jprincipals == NULL) return JNI_FALSE;
  for (i=0; i < num_principals; i++)
      {
	  jprinc = (*env)->NewByteArray(env, principals_len[i]);
	  if (jprinc == NULL) return JNI_FALSE;
	  (*env)->SetByteArrayRegion(env, jprinc, 
				     0, principals_len[i], 
				     (jbyte*)principals[i]);
	  (*env)->SetObjectArrayElement(env, jprincipals, i,  jprinc);
      }   
  jcaps = (*env)->NewByteArray(env, sizeof(JVMP_SecurityCap));
  if (jcaps == NULL) return JNI_FALSE;
  (*env)->SetByteArrayRegion(env, jcaps, 
			     0, sizeof(JVMP_SecurityCap), (jbyte*)caps->bits);
  result = (*env)->CallIntMethod(env, JVMP_PluginInstance, 
				 myMethod, 
				 jcaps,
				 jprincipals);
  if ((*env)->ExceptionOccurred(env)) 
      {      
	  (*env)->ExceptionDescribe(env);
	  (*env)->ExceptionClear(env);
	  ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	  return JNI_FALSE;
      };
  /* if Java side doesn't allow those capabilities */
  if (result != 1) 
    {
      ctx->err =  JVMP_ERROR_CAPSNOTGIVEN;
      return JNI_FALSE;
    }
  /* update current context capabilities */
  JVMP_SET_CAPS(ctx->caps, *caps);
  return JNI_TRUE;
}

static jint JNICALL JVMP_DisableCapabilities(JVMP_CallingContext* ctx,
					     JVMP_SecurityCap* caps)
{
  JNIEnv* env;
  static jmethodID myMethod = NULL;
  jobject jprincipal = NULL;
  jbyteArray jcaps = NULL;
  jint result;

  if (!ValidCtx(ctx)) return JNI_FALSE;
  if (ctx->GetCap(ctx, JVMP_CAP_SYS_CHCAP) != JNI_TRUE)
    {
	  ctx->err = JVMP_ERROR_NOACCESS;
	  return JNI_FALSE;
      }
  env  = ctx->env;
  if (!myMethod)
      {
	  myMethod = 
	      (*env)->GetMethodID(env, JVMP_PluginClass, 
				  "disableCapabilities",
				  "([B)I");
	  if (myMethod == NULL)
	      {      
		  (*env)->ExceptionDescribe(env);
		  (*env)->ExceptionClear(env);
		  ctx->err = JVMP_ERROR_NOSUCHMETHOD;
		  return JNI_FALSE;
	      }
      }
   jcaps = (*env)->NewByteArray(env, sizeof(JVMP_SecurityCap));
   if (jcaps == NULL) return JNI_FALSE;
   (*env)->SetByteArrayRegion(env, jcaps, 0, 
			      sizeof(JVMP_SecurityCap), (jbyte*)caps->bits);
   result = (*env)->CallIntMethod(env, JVMP_PluginInstance, 
				  myMethod, 
				  jcaps,
				  jprincipal);
   if ((*env)->ExceptionOccurred(env)) 
	 {      
	   (*env)->ExceptionDescribe(env);
	   (*env)->ExceptionClear(env);
	   ctx->err = JVMP_ERROR_JAVAEXCEPTION;
	   return JNI_FALSE;
	 };
   /* update current context capabilities */
   /* XXX: check for system caps */
  JVMP_RESET_CAPS(ctx->caps, *caps);
  return JNI_TRUE;
}

static jint JNICALL JVMP_GetLastErrorString(JVMP_CallingContext*  ctx,
					    char*                 buf)
{
  jint err;
  if (ctx) err=ctx->err;
  if (!ValidCtx(ctx) || !buf) return JNI_FALSE;
  strcpy(buf,  g_jvmp_errors[err]);
  return JNI_TRUE;
}
static JVMP_PluginDescription JVMP_Description = {
/* JVM vendor */
#ifdef _JVMP_SUNJVM
  "Sun",
#endif
#ifdef _JVMP_IBMJVM
  "IBM",
#endif
/* JVM version: XXX - set it correctly */
  "1.3",
/*  JVM kind */
  JVMKIND,
/* JVMP version */
  "1.0",
/* platform */
  PLATFORM,
/* arch */
  ARCH,
/* vendor specific data */
  NULL
};
/* Here main structure is initialized */

static JVMP_RuntimeContext JVMP_PluginContext = {
    NULL,
    &JVMP_GetDescription,
    &JVMP_GetRunningJVM,
    &JVMP_SendSysEvent,
    &JVMP_PostSysEvent,
    &JVMP_StopJVM,
    &JVMP_RegisterWindow, 
    &JVMP_UnregisterWindow,
    &JVMP_RegisterMonitorObject,
    &JVMP_UnregisterMonitorObject,
    &JVMP_AttachCurrentThread,
    &JVMP_DetachCurrentThread,
    &JVMP_GetCallingContext,
    &JVMP_CreatePeer,
    &JVMP_SendEvent,
    &JVMP_PostEvent,
    &JVMP_DestroyPeer,
    &JVMP_RegisterExtension,
    &JVMP_SendExtensionEvent,
    &JVMP_PostExtensionEvent,
    &JVMP_UnregisterExtension,
    &JVMP_EnableCapabilities,
    &JVMP_DisableCapabilities,
    &JVMP_DirectPeerCall,
    &JVMP_DirectExtCall,
    &JVMP_DirectJVMCall,
    &JVMP_GetLastErrorString,
    { NULL }
};

jint _JVMP_IMPORT_OR_EXPORT JNICALL JVMP_GetPlugin(JVMP_RuntimeContext** cx) {
    if (cx == NULL) return JNI_FALSE;
    if (jvm_mutex == NULL) MUTEX_CREATE(jvm_mutex);
    if (!loadJVM(LIBJVM)) { 
      PLUGIN_LOG("BAD - JVM loading failed");
      *cx = NULL;
      return JNI_FALSE;
    }
    (*cx) = &JVMP_PluginContext;
    return JNI_TRUE;
}
