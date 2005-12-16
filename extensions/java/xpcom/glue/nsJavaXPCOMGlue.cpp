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
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
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
#include <stdlib.h>

#define GRE_NATIVE(func) Java_org_mozilla_xpcom_internal_GREImpl_##func
#define XPCOM_NATIVE(func) Java_org_mozilla_xpcom_internal_XPCOMImpl_##func
#define JAVAPROXY_NATIVE(func) \
          Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_##func


/***********************
 *  JNI Load & Unload
 ***********************/

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* reserved)
{
  // Let the JVM know that we are using JDK 1.2 JNI features.
  return JNI_VERSION_1_2;
}

extern "C" JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM* vm, void* reserved)
{
}

/********************************
 *  JavaXPCOM JNI interfaces
 ********************************/

typedef void     (*JX_InitEmbeddingFunc) (JNIEnv*, jobject, jobject, jobject,
                                          jobject);
typedef void     (*JX_TermEmbeddingFunc) (JNIEnv*, jobject);
typedef jobject  (*JX_InitXPCOMFunc) (JNIEnv*, jobject, jobject, jobject);
typedef void     (*JX_ShutdownXPCOMFunc) (JNIEnv*, jobject, jobject);
typedef jobject  (*JX_NewLocalFileFunc) (JNIEnv*, jobject, jstring, jboolean);
typedef jobject  (*JX_GetComponentManagerFunc) (JNIEnv*, jobject);
typedef jobject  (*JX_GetComponentRegistrarFunc) (JNIEnv*, jobject);
typedef jobject  (*JX_GetServiceManagerFunc) (JNIEnv*, jobject);
typedef jobject  (*JX_CallXPCOMMethodFunc) (JNIEnv*, jclass, jobject, jstring,
                                            jobjectArray);
typedef void     (*JX_FinalizeProxyFunc) (JNIEnv*, jclass, jobject);

JX_InitEmbeddingFunc          InitEmbedding;
JX_TermEmbeddingFunc          TermEmbedding;
JX_InitXPCOMFunc              InitXPCOM;
JX_ShutdownXPCOMFunc          ShutdownXPCOM;
JX_NewLocalFileFunc           NewLocalFile;
JX_GetComponentManagerFunc    GetComponentManager;
JX_GetComponentRegistrarFunc  GetComponentRegistrar;
JX_GetServiceManagerFunc      GetServiceManager;
JX_CallXPCOMMethodFunc        CallXPCOMMethod;
JX_FinalizeProxyFunc          FinalizeProxy;

static nsDynamicFunctionLoad funcs[] = {
  { "Java_org_mozilla_xpcom_internal_GREImpl_initEmbedding",
          (NSFuncPtr*) &InitEmbedding },
  { "Java_org_mozilla_xpcom_internal_GREImpl_termEmbedding",
          (NSFuncPtr*) &TermEmbedding },
  { "Java_org_mozilla_xpcom_internal_XPCOMImpl_initXPCOM",
          (NSFuncPtr*) &InitXPCOM },
  { "Java_org_mozilla_xpcom_internal_XPCOMImpl_shutdownXPCOM",
          (NSFuncPtr*) &ShutdownXPCOM },
  { "Java_org_mozilla_xpcom_internal_XPCOMImpl_newLocalFile",
          (NSFuncPtr*) &NewLocalFile },
  { "Java_org_mozilla_xpcom_internal_XPCOMImpl_getComponentManager",
          (NSFuncPtr*) &GetComponentManager },
  { "Java_org_mozilla_xpcom_internal_XPCOMImpl_getComponentRegistrar",
          (NSFuncPtr*) &GetComponentRegistrar },
  { "Java_org_mozilla_xpcom_internal_XPCOMImpl_getServiceManager",
          (NSFuncPtr*) &GetServiceManager },
  { "Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_callXPCOMMethod",
          (NSFuncPtr*) &CallXPCOMMethod },
  { "Java_org_mozilla_xpcom_internal_XPCOMJavaProxy_finalizeProxy",
          (NSFuncPtr*) &FinalizeProxy },
  { nsnull, nsnull }
};

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
Initialize(JNIEnv* env, jobject aXPCOMPath)
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

extern "C" JNIEXPORT void JNICALL
GRE_NATIVE(initEmbeddingNative) (JNIEnv* env, jobject aObject,
                                 jobject aLibXULDirectory,
                                 jobject aAppDirectory, jobject aAppDirProvider)
{
  nsresult rv = Initialize(env, aLibXULDirectory);
  if (NS_FAILED(rv)) {
    ThrowException(env, rv, "Initialization failed in initEmbeddingNative");
    return;
  }

  InitEmbedding(env, aObject, aLibXULDirectory, aAppDirectory, aAppDirProvider);
}

extern "C" JNIEXPORT void JNICALL
GRE_NATIVE(termEmbedding) (JNIEnv *env, jobject aObject)
{
  TermEmbedding(env, aObject);
  XPCOMGlueShutdown();
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(initXPCOMNative) (JNIEnv* env, jobject aObject,
                               jobject aMozBinDirectory,
                               jobject aAppFileLocProvider)
{
  nsresult rv = Initialize(env, aMozBinDirectory);
  if (NS_FAILED(rv)) {
    ThrowException(env, rv, "Initialization failed in initXPCOMNative");
    return nsnull;
  }

  return InitXPCOM(env, aObject, aMozBinDirectory, aAppFileLocProvider);
}

extern "C" JNIEXPORT void JNICALL
XPCOM_NATIVE(shutdownXPCOM) (JNIEnv *env, jobject aObject, jobject aServMgr)
{
  ShutdownXPCOM(env, aObject, aServMgr);
  XPCOMGlueShutdown();
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(newLocalFile) (JNIEnv *env, jobject aObject, jstring aPath,
                            jboolean aFollowLinks)
{
  return NewLocalFile(env, aObject, aPath, aFollowLinks);
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(getComponentManager) (JNIEnv *env, jobject aObject)
{
  return GetComponentManager(env, aObject);
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(getComponentRegistrar) (JNIEnv *env, jobject aObject)
{
  return GetComponentRegistrar(env, aObject);
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(getServiceManager) (JNIEnv *env, jobject aObject)
{
  return GetServiceManager(env, aObject);
}

extern "C" JNIEXPORT jobject JNICALL
JAVAPROXY_NATIVE(callXPCOMMethod) (JNIEnv *env, jclass that, jobject aJavaProxy,
                                   jstring aMethodName, jobjectArray aParams)
{
  return CallXPCOMMethod(env, that, aJavaProxy, aMethodName, aParams);
}

extern "C" JNIEXPORT void JNICALL
JAVAPROXY_NATIVE(finalizeProxyNative) (JNIEnv *env, jclass that,
                                       jobject aJavaProxy)
{
  FinalizeProxy(env, that, aJavaProxy);
}

