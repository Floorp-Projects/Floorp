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
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#include "nsJavaWrapper.h"
#include "nsJavaXPCOMBindingUtils.h"
#include "nsJavaXPTCStub.h"
#include "nsEmbedAPI.h"
#include "nsIComponentRegistrar.h"
#include "nsString.h"
#include "nsISimpleEnumerator.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIInputStream.h"
#include "nsEnumeratorUtils.h"
#include "nsArray.h"
#include "nsAppFileLocProviderProxy.h"

#define GECKO_NATIVE(func) Java_org_mozilla_xpcom_GeckoEmbed_##func
#define XPCOM_NATIVE(func) Java_org_mozilla_xpcom_XPCOM_##func
#define XPCOMPRIVATE_NATIVE(func) Java_org_mozilla_xpcom_XPCOMPrivate_##func


extern "C" JX_EXPORT void JNICALL
GECKO_NATIVE(initEmbedding) (JNIEnv* env, jclass, jobject aMozBinDirectory,
                             jobject aAppFileLocProvider)
{
  nsresult rv = NS_OK;
  if (!InitializeJavaGlobals(env)) {
    rv = NS_ERROR_FAILURE;
  } else {
    // Create an nsILocalFile from given java.io.File
    nsCOMPtr<nsILocalFile> directory;
    if (aMozBinDirectory) {
      rv = File_to_nsILocalFile(env, aMozBinDirectory, getter_AddRefs(directory));
    }

    if (NS_SUCCEEDED(rv)) {
      nsAppFileLocProviderProxy* provider = nsnull;
      if (aAppFileLocProvider) {
        provider = new nsAppFileLocProviderProxy(env, aAppFileLocProvider);
        if (!provider)
          rv = NS_ERROR_OUT_OF_MEMORY;
      }

      if (NS_SUCCEEDED(rv)) {
        rv = NS_InitEmbedding(directory, provider);
        if (provider) {
          delete provider;
        }
        if (NS_SUCCEEDED(rv)) {
          return;
        }
      }
    }
  }

  ThrowException(env, rv, "Failure in initEmbedding");
  FreeJavaGlobals(env);
}

extern "C" JX_EXPORT void JNICALL
GECKO_NATIVE(termEmbedding) (JNIEnv *env, jclass)
{
  nsresult rv = NS_TermEmbedding();
  if (NS_FAILED(rv))
    ThrowException(env, rv, "NS_TermEmbedding failed");

  FreeJavaGlobals(env);
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(initXPCOM) (JNIEnv* env, jclass, jobject aMozBinDirectory,
                         jobject aAppFileLocProvider)
{
  nsresult rv = NS_OK;
  if (!InitializeJavaGlobals(env)) {
    rv = NS_ERROR_FAILURE;
  } else {
    // Create an nsILocalFile from given java.io.File
    nsCOMPtr<nsILocalFile> directory;
    if (aMozBinDirectory) {
      rv = File_to_nsILocalFile(env, aMozBinDirectory, getter_AddRefs(directory));
    }

    if (NS_SUCCEEDED(rv)) {
      nsAppFileLocProviderProxy* provider = nsnull;
      if (aAppFileLocProvider) {
        provider = new nsAppFileLocProviderProxy(env, aAppFileLocProvider);
        if (!provider)
          rv = NS_ERROR_OUT_OF_MEMORY;
      }

      if (NS_SUCCEEDED(rv)) {
        nsIServiceManager* servMan = nsnull;
        rv = NS_InitXPCOM2(&servMan, directory, provider);
        if (provider) {
          delete provider;
        }

        if (NS_SUCCEEDED(rv)) {
          jobject javaProxy;
          rv = CreateJavaProxy(env, servMan, NS_GET_IID(nsIServiceManager),
                               &javaProxy);
          NS_RELEASE(servMan);   // JavaXPCOMInstance has owning ref
          if (NS_SUCCEEDED(rv))
            return javaProxy;
        }
      }
    }
  }

  ThrowException(env, rv, "Failure in initXPCOM");
  FreeJavaGlobals(env);
  return nsnull;
}

extern "C" JX_EXPORT void JNICALL
XPCOM_NATIVE(shutdownXPCOM) (JNIEnv *env, jclass, jobject aServMgr)
{
  nsCOMPtr<nsIServiceManager> servMgr;
  if (aServMgr) {
    // Find corresponding XPCOM object
    void* xpcomObj = gBindings->GetXPCOMObject(env, aServMgr);
    NS_ASSERTION(xpcomObj != nsnull, "Failed to get XPCOM obj for ServiceMgr.");

    // Even if we failed to get the matching xpcom object, we don't abort this
    // function.  Just call NS_ShutdownXPCOM with a null service manager.

    if (xpcomObj) {
      NS_ASSERTION(!IsXPTCStub(xpcomObj),
                   "Expected JavaXPCOMInstance, but got nsJavaXPTCStub");
      servMgr = do_QueryInterface(((JavaXPCOMInstance*) xpcomObj)->GetInstance());
    }
  }

  nsresult rv = NS_ShutdownXPCOM(servMgr);
  if (NS_FAILED(rv))
    ThrowException(env, rv, "NS_ShutdownXPCOM failed");

  FreeJavaGlobals(env);
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(newLocalFile) (JNIEnv *env, jclass, jstring aPath,
                            jboolean aFollowLinks)
{
  // Create a Mozilla string from the jstring
  const PRUnichar* buf = nsnull;
  if (aPath) {
    buf = env->GetStringChars(aPath, nsnull);
    if (!buf)
      return nsnull;  // exception already thrown
  }

  nsAutoString path_str(buf);
  env->ReleaseStringChars(aPath, buf);

  // Make call to given function
  nsILocalFile* file = nsnull;
  nsresult rv = NS_NewLocalFile(path_str, aFollowLinks, &file);

  if (NS_SUCCEEDED(rv)) {
    jobject javaProxy;
    rv = CreateJavaProxy(env, file, NS_GET_IID(nsILocalFile), &javaProxy);
    NS_RELEASE(file);   // JavaXPCOMInstance has owning ref
    if (NS_SUCCEEDED(rv))
      return javaProxy;
  }

  ThrowException(env, rv, "Failure in newLocalFile");
  return nsnull;
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(getComponentManager) (JNIEnv *env, jclass)
{
  // Call XPCOM method
  nsIComponentManager* cm = nsnull;
  nsresult rv = NS_GetComponentManager(&cm);

  if (NS_SUCCEEDED(rv)) {
    jobject javaProxy;
    rv = CreateJavaProxy(env, cm, NS_GET_IID(nsIComponentManager), &javaProxy);
    NS_RELEASE(cm);   // JavaXPCOMInstance has owning ref
    if (NS_SUCCEEDED(rv))
      return javaProxy;
  }

  ThrowException(env, rv, "Failure in getComponentManager");
  return nsnull;
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(getComponentRegistrar) (JNIEnv *env, jclass)
{
  // Call XPCOM method
  nsIComponentRegistrar* cr = nsnull;
  nsresult rv = NS_GetComponentRegistrar(&cr);

  if (NS_SUCCEEDED(rv)) {
    jobject javaProxy;
    rv = CreateJavaProxy(env, cr, NS_GET_IID(nsIComponentRegistrar), &javaProxy);
    NS_RELEASE(cr);   // JavaXPCOMInstance has owning ref
    if (NS_SUCCEEDED(rv))
      return javaProxy;
  }

  ThrowException(env, rv, "Failure in getComponentRegistrar");
  return nsnull;
}

extern "C" JX_EXPORT jobject JNICALL
XPCOM_NATIVE(getServiceManager) (JNIEnv *env, jclass)
{
  // Call XPCOM method
  nsIServiceManager* sm = nsnull;
  nsresult rv = NS_GetServiceManager(&sm);

  if (NS_SUCCEEDED(rv)) {
    jobject javaProxy;
    rv = CreateJavaProxy(env, sm, NS_GET_IID(nsIServiceManager), &javaProxy);
    NS_RELEASE(sm);   // JavaXPCOMInstance has owning ref
    if (NS_SUCCEEDED(rv))
      return javaProxy;
  }

  ThrowException(env, rv, "Failure in getServiceManager");
  return nsnull;
}

extern "C" JX_EXPORT void JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodVoid) (JNIEnv *env, jclass that,
                                          jobject aJavaObject,
                                          jint aMethodIndex,
                                          jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
}

extern "C" JX_EXPORT jboolean JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodBool) (JNIEnv *env, jclass that,
                                          jobject aJavaObject,
                                          jint aMethodIndex,
                                          jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.z;
}

extern "C" JX_EXPORT jbooleanArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodBoolA) (JNIEnv *env, jclass that,
                                           jobject aJavaObject,
                                           jint aMethodIndex,
                                           jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jbooleanArray) rc.l;
}

extern "C" JX_EXPORT jbyte JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodByte) (JNIEnv *env, jclass that,
                                          jobject aJavaObject,
                                          jint aMethodIndex,
                                          jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.b;
}

extern "C" JX_EXPORT jbyteArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodByteA) (JNIEnv *env, jclass that,
                                           jobject aJavaObject,
                                           jint aMethodIndex,
                                           jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jbyteArray) rc.l;
}

extern "C" JX_EXPORT jchar JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodChar) (JNIEnv *env, jclass that,
                                          jobject aJavaObject,
                                          jint aMethodIndex,
                                          jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.c;
}

extern "C" JX_EXPORT jcharArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodCharA) (JNIEnv *env, jclass that,
                                           jobject aJavaObject,
                                           jint aMethodIndex,
                                           jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jcharArray) rc.l;
}

extern "C" JX_EXPORT jshort JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodShort) (JNIEnv *env, jclass that,
                                           jobject aJavaObject,
                                           jint aMethodIndex,
                                           jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.s;
}

extern "C" JX_EXPORT jshortArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodShortA) (JNIEnv *env, jclass that,
                                            jobject aJavaObject,
                                            jint aMethodIndex,
                                            jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jshortArray) rc.l;
}

extern "C" JX_EXPORT jint JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodInt) (JNIEnv *env, jclass that,
                                         jobject aJavaObject,
                                         jint aMethodIndex,
                                         jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.i;
}

extern "C" JX_EXPORT jintArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodIntA) (JNIEnv *env, jclass that,
                                          jobject aJavaObject,
                                          jint aMethodIndex,
                                          jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jintArray) rc.l;
}

extern "C" JX_EXPORT jlong JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodLong) (JNIEnv *env, jclass that,
                                          jobject aJavaObject,
                                          jint aMethodIndex,
                                          jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.j;
}

extern "C" JX_EXPORT jlongArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodLongA) (JNIEnv *env, jclass that,
                                           jobject aJavaObject,
                                           jint aMethodIndex,
                                           jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jlongArray) rc.l;
}

extern "C" JX_EXPORT jfloat JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodFloat) (JNIEnv *env, jclass that,
                                           jobject aJavaObject,
                                           jint aMethodIndex,
                                           jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.f;
}

extern "C" JX_EXPORT jfloatArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodFloatA) (JNIEnv *env, jclass that,
                                            jobject aJavaObject,
                                            jint aMethodIndex,
                                            jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jfloatArray) rc.l;
}

extern "C" JX_EXPORT jdouble JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodDouble) (JNIEnv *env, jclass that,
                                            jobject aJavaObject,
                                            jint aMethodIndex,
                                            jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.d;
}

extern "C" JX_EXPORT jdoubleArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodDoubleA) (JNIEnv *env, jclass that,
                                             jobject aJavaObject,
                                             jint aMethodIndex,
                                             jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jdoubleArray) rc.l;
}

extern "C" JX_EXPORT jobject JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodObj) (JNIEnv *env, jclass that,
                                         jobject aJavaObject,
                                         jint aMethodIndex,
                                         jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.l;
}

extern "C" JX_EXPORT jobjectArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodObjA) (JNIEnv *env, jclass that,
                                          jobject aJavaObject,
                                          jint aMethodIndex,
                                          jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jobjectArray) rc.l;
}

extern "C" JX_EXPORT void JNICALL
XPCOMPRIVATE_NATIVE(FinalizeStub) (JNIEnv *env, jclass that,
                                   jobject aJavaObject)
{
#ifdef DEBUG_pedemonte
  jclass clazz = env->GetObjectClass(aJavaObject);
  jstring name = (jstring) env->CallObjectMethod(clazz, getNameMID);
  const char* javaObjectName = env->GetStringUTFChars(name, nsnull);
  LOG(("*** Finalize(java_obj=%s)\n", javaObjectName));
  env->ReleaseStringUTFChars(name, javaObjectName);
#endif

  // Due to Java's garbage collection, this finalize statement may get called
  // after FreeJavaGlobals().  So check to make sure that everything is still
  // initialized.
  if (gJavaXPCOMInitialized) {
    void* obj = gBindings->GetXPCOMObject(env, aJavaObject);
    NS_ASSERTION(obj != nsnull, "No matching XPCOM obj in FinalizeStub");

    if (obj) {
      NS_ASSERTION(!IsXPTCStub(obj),
                   "Expecting JavaXPCOMInstance, got nsJavaXPTCStub");
      gBindings->RemoveBinding(env, aJavaObject, nsnull);
      delete (JavaXPCOMInstance*) obj;
    }
  }
}

