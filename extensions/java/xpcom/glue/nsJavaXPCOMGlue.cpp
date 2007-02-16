/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jni.h"
#include "nsXPCOMPrivate.h" // for XPCOM_DLL defines.
#include "nsXPCOMGlue.h"
#include "nsDebug.h"
#include <stdlib.h>

#if defined(XP_WIN) || defined(XP_OS2)
#define JX_EXPORT   JNIEXPORT
#else
#define JX_EXPORT   JNIEXPORT NS_EXPORT
#endif


/***********************
 *  JNI Load & Unload
 ***********************/

extern "C" JX_EXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved)
{
  // Let the JVM know that we are using JDK 1.2 JNI features.
  return JNI_VERSION_1_2;
}

extern "C" JX_EXPORT void JNICALL
JNI_OnUnload(JavaVM* vm, void* reserved)
{
}

/********************************
 *  JavaXPCOM JNI interfaces
 ********************************/

#define JXM_NATIVE(func) Java_org_mozilla_xpcom_internal_JavaXPCOMMethods_##func

enum {
  kFunc_Initialize,
  kFunc_InitEmbedding,
  kFunc_TermEmbedding,
  kFunc_LockProfileDirectory,
  kFunc_NotifyProfile,
  kFunc_InitXPCOM,
  kFunc_ShutdownXPCOM,
  kFunc_GetComponentManager,
  kFunc_GetComponentRegistrar,
  kFunc_GetServiceManager,
  kFunc_NewLocalFile,
  kFunc_CallXPCOMMethod,
  kFunc_FinalizeProxy,
  kFunc_IsSameXPCOMObject,
  kFunc_ReleaseProfileLock,
  kFunc_GetNativeHandleFromAWT,
  kFunc_WrapJavaObject,
  kFunc_WrapXPCOMObject
};

#define JX_NUM_FUNCS 18


// Get path string from java.io.File object.
jstring
GetJavaFilePath(JNIEnv* env, jobject aFile)
{
  jclass clazz = env->FindClass("java/io/File");
  if (clazz) {
    jmethodID pathMID = env->GetMethodID(clazz, "getCanonicalPath",
                                         "()Ljava/lang/String;");
    if (pathMID) {
      return (jstring) env->CallObjectMethod(aFile, pathMID);
    }
  }

  return nsnull;
}

// Calls XPCOMGlueStartup using the given java.io.File object, and loads
// the JavaXPCOM methods from the XUL shared library.
nsresult
LoadXULMethods(JNIEnv* env, jobject aXPCOMPath, void** aFunctions)
{
  jstring pathString = GetJavaFilePath(env, aXPCOMPath);
  if (!pathString)
    return NS_ERROR_FAILURE;
  const char* path = env->GetStringUTFChars(pathString, nsnull);
  if (!path)
    return NS_ERROR_OUT_OF_MEMORY;

  int len = strlen(path);
  char* xpcomPath = (char*) malloc(len + sizeof(XPCOM_DLL) +
                                   sizeof(XPCOM_FILE_PATH_SEPARATOR) + 1);
  if (!xpcomPath)
    return NS_ERROR_OUT_OF_MEMORY;
  sprintf(xpcomPath, "%s" XPCOM_FILE_PATH_SEPARATOR XPCOM_DLL, path);

  nsresult rv = XPCOMGlueStartup(xpcomPath);
  free(xpcomPath);
  if (NS_FAILED(rv))
    return rv;

#ifdef XP_WIN32
  // The JNICALL calling convention defines to "__stdcall" on Win32, which
  // mangles the name.
  nsDynamicFunctionLoad funcs[] = {
    { "_Java_org_mozilla_xpcom_internal_MozillaImpl_initialize@8",
            (NSFuncPtr*) &aFunctions[kFunc_Initialize] },
    { "_Java_org_mozilla_xpcom_internal_GREImpl_initEmbedding@20",
            (NSFuncPtr*) &aFunctions[kFunc_InitEmbedding] },
    { "_Java_org_mozilla_xpcom_internal_GREImpl_termEmbedding@8",
            (NSFuncPtr*) &aFunctions[kFunc_TermEmbedding] },
    { "_Java_org_mozilla_xpcom_internal_GREImpl_lockProfileDirectory@12",
            (NSFuncPtr*) &aFunctions[kFunc_LockProfileDirectory] },
    { "_Java_org_mozilla_xpcom_internal_GREImpl_notifyProfile@8",
            (NSFuncPtr*) &aFunctions[kFunc_NotifyProfile] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMImpl_initXPCOM@16",
            (NSFuncPtr*) &aFunctions[kFunc_InitXPCOM] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMImpl_shutdownXPCOM@12",
            (NSFuncPtr*) &aFunctions[kFunc_ShutdownXPCOM] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMImpl_getComponentManager@8",
            (NSFuncPtr*) &aFunctions[kFunc_GetComponentManager] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMImpl_getComponentRegistrar@8",
            (NSFuncPtr*) &aFunctions[kFunc_GetComponentRegistrar] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMImpl_getServiceManager@8",
            (NSFuncPtr*) &aFunctions[kFunc_GetServiceManager] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMImpl_newLocalFile@16",
            (NSFuncPtr*) &aFunctions[kFunc_NewLocalFile] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_callXPCOMMethod@20",
            (NSFuncPtr*) &aFunctions[kFunc_CallXPCOMMethod] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_finalizeProxy@12",
            (NSFuncPtr*) &aFunctions[kFunc_FinalizeProxy] },
    { "_Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_isSameXPCOMObject@16",
            (NSFuncPtr*) &aFunctions[kFunc_IsSameXPCOMObject] },
    { "_Java_org_mozilla_xpcom_ProfileLock_release@16",
            (NSFuncPtr*) &aFunctions[kFunc_ReleaseProfileLock] },
    { "_Java_org_mozilla_xpcom_internal_MozillaImpl_getNativeHandleFromAWT@12",
            (NSFuncPtr*) &aFunctions[kFunc_GetNativeHandleFromAWT] },
    { "_Java_org_mozilla_xpcom_internal_JavaXPCOMMethods_wrapJavaObject@16",
            (NSFuncPtr*) &aFunctions[kFunc_WrapJavaObject] },
    { "_Java_org_mozilla_xpcom_internal_JavaXPCOMMethods_wrapXPCOMObject@20",
            (NSFuncPtr*) &aFunctions[kFunc_WrapXPCOMObject] },
    { nsnull, nsnull }
  };
#else
  nsDynamicFunctionLoad funcs[] = {
    { "Java_org_mozilla_xpcom_internal_MozillaImpl_initialize",
            (NSFuncPtr*) &aFunctions[kFunc_Initialize] },
    { "Java_org_mozilla_xpcom_internal_GREImpl_initEmbedding",
            (NSFuncPtr*) &aFunctions[kFunc_InitEmbedding] },
    { "Java_org_mozilla_xpcom_internal_GREImpl_termEmbedding",
            (NSFuncPtr*) &aFunctions[kFunc_TermEmbedding] },
    { "Java_org_mozilla_xpcom_internal_GREImpl_lockProfileDirectory",
            (NSFuncPtr*) &aFunctions[kFunc_LockProfileDirectory] },
    { "Java_org_mozilla_xpcom_internal_GREImpl_notifyProfile",
            (NSFuncPtr*) &aFunctions[kFunc_NotifyProfile] },
    { "Java_org_mozilla_xpcom_internal_XPCOMImpl_initXPCOM",
            (NSFuncPtr*) &aFunctions[kFunc_InitXPCOM] },
    { "Java_org_mozilla_xpcom_internal_XPCOMImpl_shutdownXPCOM",
            (NSFuncPtr*) &aFunctions[kFunc_ShutdownXPCOM] },
    { "Java_org_mozilla_xpcom_internal_XPCOMImpl_getComponentManager",
            (NSFuncPtr*) &aFunctions[kFunc_GetComponentManager] },
    { "Java_org_mozilla_xpcom_internal_XPCOMImpl_getComponentRegistrar",
            (NSFuncPtr*) &aFunctions[kFunc_GetComponentRegistrar] },
    { "Java_org_mozilla_xpcom_internal_XPCOMImpl_getServiceManager",
            (NSFuncPtr*) &aFunctions[kFunc_GetServiceManager] },
    { "Java_org_mozilla_xpcom_internal_XPCOMImpl_newLocalFile",
            (NSFuncPtr*) &aFunctions[kFunc_NewLocalFile] },
    { "Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_callXPCOMMethod",
            (NSFuncPtr*) &aFunctions[kFunc_CallXPCOMMethod] },
    { "Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_finalizeProxy",
            (NSFuncPtr*) &aFunctions[kFunc_FinalizeProxy] },
    { "Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_isSameXPCOMObject",
            (NSFuncPtr*) &aFunctions[kFunc_IsSameXPCOMObject] },
    { "Java_org_mozilla_xpcom_ProfileLock_release",
            (NSFuncPtr*) &aFunctions[kFunc_ReleaseProfileLock] },
    { "Java_org_mozilla_xpcom_internal_MozillaImpl_getNativeHandleFromAWT",
            (NSFuncPtr*) &aFunctions[kFunc_GetNativeHandleFromAWT] },
    { "Java_org_mozilla_xpcom_internal_JavaXPCOMMethods_wrapJavaObject",
            (NSFuncPtr*) &aFunctions[kFunc_WrapJavaObject] },
    { "Java_org_mozilla_xpcom_internal_JavaXPCOMMethods_wrapXPCOMObject",
            (NSFuncPtr*) &aFunctions[kFunc_WrapXPCOMObject] },
    { nsnull, nsnull }
  };
#endif

  rv = XPCOMGlueLoadXULFunctions(funcs);
  if (NS_FAILED(rv))
    return rv;

  return NS_OK;
}

void
ThrowException(JNIEnv* env, const nsresult aErrorCode, const char* aMessage)
{
  // Only throw this exception if one hasn't already been thrown, so we don't
  // mask a previous exception/error.
  if (env->ExceptionCheck())
    return;

  // If the error code we get is for an Out Of Memory error, try to throw an
  // OutOfMemoryError.  The JVM may have enough memory to create this error.
  if (aErrorCode == NS_ERROR_OUT_OF_MEMORY) {
    jclass clazz = env->FindClass("java/lang/OutOfMemoryError");
    if (clazz) {
      env->ThrowNew(clazz, aMessage);
    }
    env->DeleteLocalRef(clazz);
    return;
  }

  // If the error was not handled above, then create an XPCOMException with the
  // given error code.
  jthrowable throwObj = nsnull;
  jclass exceptionClass = env->FindClass("org/mozilla/xpcom/XPCOMException");
  if (exceptionClass) {
    jmethodID mid = env->GetMethodID(exceptionClass, "<init>",
                                     "(JLjava/lang/String;)V");
    if (mid) {
      throwObj = (jthrowable) env->NewObject(exceptionClass, mid,
                                             (PRInt64) aErrorCode,
                                             env->NewStringUTF(aMessage));
    }
  }
  NS_ASSERTION(throwObj, "Failed to create XPCOMException object");

  // throw exception
  if (throwObj) {
    env->Throw(throwObj);
  }
}

// Register the JavaXPCOM native methods.  This associates a native Java
// method with its C implementation.
nsresult
RegisterNativeMethods(JNIEnv* env, void** aFunctions)
{
  JNINativeMethod mozilla_methods[] = {
    { "initializeNative", "()V",
      (void*) aFunctions[kFunc_Initialize] },
    { "getNativeHandleFromAWT", "(Ljava/lang/Object;)J",
      (void*) aFunctions[kFunc_GetNativeHandleFromAWT] }
  };

  JNINativeMethod gre_methods[] = {
    { "initEmbeddingNative",
      "(Ljava/io/File;Ljava/io/File;Lorg/mozilla/xpcom/IAppFileLocProvider;)V",
      (void*) aFunctions[kFunc_InitEmbedding] },
    { "termEmbedding", "()V",
      (void*) aFunctions[kFunc_TermEmbedding] },
    { "lockProfileDirectory", "(Ljava/io/File;)Lorg/mozilla/xpcom/ProfileLock;",
      (void*) aFunctions[kFunc_LockProfileDirectory] },
    { "notifyProfile", "()V",
      (void*) aFunctions[kFunc_NotifyProfile] },
  };

  JNINativeMethod xpcom_methods[] = {
    { "initXPCOMNative",
      "(Ljava/io/File;Lorg/mozilla/xpcom/IAppFileLocProvider;)Lorg/mozilla/interfaces/nsIServiceManager;",
      (void*) aFunctions[kFunc_InitXPCOM] },
    { "shutdownXPCOM", "(Lorg/mozilla/interfaces/nsIServiceManager;)V",
      (void*) aFunctions[kFunc_ShutdownXPCOM] },
    { "getComponentManager", "()Lorg/mozilla/interfaces/nsIComponentManager;",
      (void*) aFunctions[kFunc_GetComponentManager] },
    { "getComponentRegistrar", "()Lorg/mozilla/interfaces/nsIComponentRegistrar;",
      (void*) aFunctions[kFunc_GetComponentRegistrar] },
    { "getServiceManager", "()Lorg/mozilla/interfaces/nsIServiceManager;",
      (void*) aFunctions[kFunc_GetServiceManager] },
    { "newLocalFile", "(Ljava/lang/String;Z)Lorg/mozilla/interfaces/nsILocalFile;",
      (void*) aFunctions[kFunc_NewLocalFile] }
  };

  JNINativeMethod proxy_methods[] = {
    { "callXPCOMMethod",
      "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;",
      (void*) aFunctions[kFunc_CallXPCOMMethod] },
    { "finalizeProxyNative", "(Ljava/lang/Object;)V",
      (void*) aFunctions[kFunc_FinalizeProxy] },
    { "isSameXPCOMObject", "(Ljava/lang/Object;Ljava/lang/Object;)Z",
      (void*) aFunctions[kFunc_IsSameXPCOMObject] }
  };

  JNINativeMethod lockProxy_methods[] = {
    { "releaseNative", "(J)V",
      (void*) aFunctions[kFunc_ReleaseProfileLock] }
  };

  JNINativeMethod util_methods[] = {
    { "wrapJavaObject", "(Ljava/lang/Object;Ljava/lang/String;)J",
      (void*) aFunctions[kFunc_WrapJavaObject] },
    { "wrapXPCOMObject", "(JLjava/lang/String;)Ljava/lang/Object;",
      (void*) aFunctions[kFunc_WrapXPCOMObject] }
  };

  jint rc = -1;
  jclass clazz = env->FindClass("org/mozilla/xpcom/internal/MozillaImpl");
  if (clazz) {
    rc = env->RegisterNatives(clazz, mozilla_methods,
                          sizeof(mozilla_methods) / sizeof(mozilla_methods[0]));
  }
  NS_ENSURE_TRUE(rc == 0, NS_ERROR_FAILURE);

  rc = -1;
  clazz = env->FindClass("org/mozilla/xpcom/internal/GREImpl");
  if (clazz) {
    rc = env->RegisterNatives(clazz, gre_methods,
                              sizeof(gre_methods) / sizeof(gre_methods[0]));
  }
  NS_ENSURE_TRUE(rc == 0, NS_ERROR_FAILURE);

  rc = -1;
  clazz = env->FindClass("org/mozilla/xpcom/internal/XPCOMImpl");
  if (clazz) {
    rc = env->RegisterNatives(clazz, xpcom_methods,
                              sizeof(xpcom_methods) / sizeof(xpcom_methods[0]));
  }
  NS_ENSURE_TRUE(rc == 0, NS_ERROR_FAILURE);

  rc = -1;
  clazz = env->FindClass("org/mozilla/xpcom/internal/XPCOMJavaProxy");
  if (clazz) {
    rc = env->RegisterNatives(clazz, proxy_methods,
                              sizeof(proxy_methods) / sizeof(proxy_methods[0]));
  }
  NS_ENSURE_TRUE(rc == 0, NS_ERROR_FAILURE);

  rc = -1;
  clazz = env->FindClass("org/mozilla/xpcom/ProfileLock");
  if (clazz) {
    rc = env->RegisterNatives(clazz, lockProxy_methods,
                      sizeof(lockProxy_methods) / sizeof(lockProxy_methods[0]));
  }
  NS_ENSURE_TRUE(rc == 0, NS_ERROR_FAILURE);

  rc = -1;
  clazz = env->FindClass("org/mozilla/xpcom/internal/JavaXPCOMMethods");
  if (clazz) {
    rc = env->RegisterNatives(clazz, util_methods,
                              sizeof(util_methods) / sizeof(util_methods[0]));
  }
  NS_ENSURE_TRUE(rc == 0, NS_ERROR_FAILURE);

  return NS_OK;
}

// Load the JavaXPCOM methods from the XUL shared library, and registers them
// as Java native methods.
extern "C" JX_EXPORT void JNICALL
JXM_NATIVE(registerJavaXPCOMMethodsNative) (JNIEnv *env, jclass that,
                                            jobject aXPCOMPath)
{
  void* functions[JX_NUM_FUNCS];
  memset(functions, 0, JX_NUM_FUNCS * sizeof(void*));

  nsresult rv = LoadXULMethods(env, aXPCOMPath, functions);
  if (NS_SUCCEEDED(rv)) {
    rv = RegisterNativeMethods(env, functions);
  }

  if (NS_FAILED(rv)) {
    ThrowException(env, rv, "Failed to register JavaXPCOM methods");
  }
}

