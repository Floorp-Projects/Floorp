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


extern "C" JNIEXPORT void JNICALL
GECKO_NATIVE(initEmbedding) (JNIEnv* env, jclass, jobject aMozBinDirectory,
                             jobject aAppFileLocProvider)
{
  nsresult rv = NS_OK;
  if (InitializeJavaGlobals(env))
  {
    // Create an nsILocalFile from given java.io.File
    nsCOMPtr<nsILocalFile> directory;
    if (aMozBinDirectory) {
      rv = File_to_nsILocalFile(env, aMozBinDirectory, getter_AddRefs(directory));
    }

    if (NS_SUCCEEDED(rv)) {
      nsAppFileLocProviderProxy* provider = nsnull;
      if (aAppFileLocProvider) {
        provider = new nsAppFileLocProviderProxy(env, aAppFileLocProvider);
      }

      rv = NS_InitEmbedding(directory, provider);
      if (provider) {
        delete provider;
      }
      if (NS_SUCCEEDED(rv)) {
        return;
      }
    }
  }

  ThrowXPCOMException(env, rv, "Failure in initEmbedding");
  FreeJavaGlobals(env);
}

extern "C" JNIEXPORT void JNICALL
GECKO_NATIVE(termEmbedding) (JNIEnv *env, jclass)
{
  nsresult rv = NS_TermEmbedding();
  if (NS_FAILED(rv))
    ThrowXPCOMException(env, rv, "NS_TermEmbedding failed");

  FreeJavaGlobals(env);
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(initXPCOM) (JNIEnv* env, jclass, jobject aMozBinDirectory,
                         jobject aAppFileLocProvider)
{
  nsresult rv = NS_OK;
  if (InitializeJavaGlobals(env))
  {
    // Create an nsILocalFile from given java.io.File
    nsCOMPtr<nsILocalFile> directory;
    if (aMozBinDirectory) {
      rv = File_to_nsILocalFile(env, aMozBinDirectory, getter_AddRefs(directory));
    }

    if (NS_SUCCEEDED(rv)) {
      nsAppFileLocProviderProxy* provider = nsnull;
      if (aAppFileLocProvider) {
        provider = new nsAppFileLocProviderProxy(env, aAppFileLocProvider);
      }

      nsIServiceManager* servMan = nsnull;
      rv = NS_InitXPCOM2(&servMan, directory, provider);
      if (provider) {
        delete provider;
      }

      jobject java_stub = nsnull;
      if (NS_SUCCEEDED(rv)) {
        // wrap xpcom instance
        JavaXPCOMInstance* inst;
        inst = CreateJavaXPCOMInstance(servMan, &NS_GET_IID(nsIServiceManager));
        NS_RELEASE(servMan);   // JavaXPCOMInstance has owning ref

        if (inst) {
          // create java stub
          java_stub = CreateJavaWrapper(env, "nsIServiceManager");

          if (java_stub) {
            // Associate XPCOM object w/ Java stub
            AddJavaXPCOMBinding(env, java_stub, inst);
            return java_stub;
          }
        }
      }
    }
  }

  ThrowXPCOMException(env, rv, "Failure in initXPCOM");
  FreeJavaGlobals(env);
  return nsnull;
}

extern "C" JNIEXPORT void JNICALL
XPCOM_NATIVE(shutdownXPCOM) (JNIEnv *env, jclass, jobject aServMgr)
{
  nsCOMPtr<nsIServiceManager> servMgr;
  if (aServMgr) {
    // Find corresponding XPCOM object
    void* xpcomObj = GetMatchingXPCOMObject(env, aServMgr);
    NS_ASSERTION(xpcomObj != nsnull, "Failed to get matching XPCOM object");
    if (xpcomObj == nsnull) {
      ThrowXPCOMException(env, 0,
                          "No matching XPCOM obj for service manager proxy");
      return;
    }

    NS_ASSERTION(!IsXPTCStub(xpcomObj),
                 "Expected JavaXPCOMInstance, but got nsJavaXPTCStub");
    servMgr = do_QueryInterface(((JavaXPCOMInstance*) xpcomObj)->GetInstance());
  }

  nsresult rv = NS_ShutdownXPCOM(servMgr);
  if (NS_FAILED(rv))
    ThrowXPCOMException(env, rv, "NS_ShutdownXPCOM failed");

  FreeJavaGlobals(env);
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(newLocalFile) (JNIEnv *env, jclass, jstring aPath,
                            jboolean aFollowLinks)
{
  jobject java_stub = nsnull;

  // Create a Mozilla string from the jstring
  jboolean isCopy;
  const PRUnichar* buf = nsnull;
  if (aPath) {
    buf = env->GetStringChars(aPath, &isCopy);
  }

  nsAutoString path_str(buf);
  if (isCopy) {
    env->ReleaseStringChars(aPath, buf);
  }

  // Make call to given function
  nsILocalFile* file = nsnull;
  nsresult rv = NS_NewLocalFile(path_str, aFollowLinks, &file);

  if (NS_SUCCEEDED(rv)) {
    // wrap xpcom instance
    JavaXPCOMInstance* inst;
    inst = CreateJavaXPCOMInstance(file, &NS_GET_IID(nsILocalFile));
    NS_RELEASE(file);   // JavaXPCOMInstance has owning ref

    if (inst) {
      // create java stub
      java_stub = CreateJavaWrapper(env, "nsILocalFile");

      if (java_stub) {
        // Associate XPCOM object w/ Java stub
        AddJavaXPCOMBinding(env, java_stub, inst);
      }
    }
  }

  if (java_stub == nsnull)
    ThrowXPCOMException(env, rv, "Failure in newLocalFile");

  return java_stub;
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(getComponentManager) (JNIEnv *env, jclass)
{
  jobject java_stub = nsnull;

  // Call XPCOM method
  nsIComponentManager* cm = nsnull;
  nsresult rv = NS_GetComponentManager(&cm);

  if (NS_SUCCEEDED(rv)) {
    // wrap xpcom instance
    JavaXPCOMInstance* inst;
    inst = CreateJavaXPCOMInstance(cm, &NS_GET_IID(nsIComponentManager));
    NS_RELEASE(cm);   // JavaXPCOMInstance has owning ref

    if (inst) {
      // create java stub
      java_stub = CreateJavaWrapper(env, "nsIComponentManager");

      if (java_stub) {
        // Associate XPCOM object w/ Java stub
        AddJavaXPCOMBinding(env, java_stub, inst);
      }
    }
  }

  if (java_stub == nsnull)
    ThrowXPCOMException(env, rv, "Failure in getComponentManager");

  return java_stub;
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(getComponentRegistrar) (JNIEnv *env, jclass)
{
  jobject java_stub = nsnull;

  // Call XPCOM method
  nsIComponentRegistrar* cr = nsnull;
  nsresult rv = NS_GetComponentRegistrar(&cr);

  if (NS_SUCCEEDED(rv)) {
    // wrap xpcom instance
    JavaXPCOMInstance* inst;
    inst = CreateJavaXPCOMInstance(cr, &NS_GET_IID(nsIComponentRegistrar));
    NS_RELEASE(cr);   // JavaXPCOMInstance has owning ref

    if (inst) {
      // create java stub
      java_stub = CreateJavaWrapper(env, "nsIComponentRegistrar");

      if (java_stub) {
        // Associate XPCOM object w/ Java stub
        AddJavaXPCOMBinding(env, java_stub, inst);
      }
    }
  }

  if (java_stub == nsnull)
    ThrowXPCOMException(env, rv, "Failure in getComponentRegistrar");

  return java_stub;
}

extern "C" JNIEXPORT jobject JNICALL
XPCOM_NATIVE(getServiceManager) (JNIEnv *env, jclass)
{
  jobject java_stub = nsnull;

  // Call XPCOM method
  nsIServiceManager* sm = nsnull;
  nsresult rv = NS_GetServiceManager(&sm);

  if (NS_SUCCEEDED(rv)) {
    // wrap xpcom instance
    JavaXPCOMInstance* inst;
    inst = CreateJavaXPCOMInstance(sm, &NS_GET_IID(nsIServiceManager));
    NS_RELEASE(sm);   // JavaXPCOMInstance has owning ref

    if (inst) {
      // create java stub
      java_stub = CreateJavaWrapper(env, "nsIServiceManager");

      if (java_stub) {
        // Associate XPCOM object w/ Java stub
        AddJavaXPCOMBinding(env, java_stub, inst);
      }
    }
  }

  if (java_stub == nsnull)
    ThrowXPCOMException(env, rv, "Failure in getServiceManager");

  return java_stub;
}

extern "C" JNIEXPORT void JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodVoid) (JNIEnv *env, jclass that, jobject aJavaObject,
                                   jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
}

extern "C" JNIEXPORT jboolean JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodBool) (JNIEnv *env, jclass that, jobject aJavaObject,
                                   jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.z;
}

extern "C" JNIEXPORT jbooleanArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodBoolA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                    jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jbooleanArray) rc.l;
}

extern "C" JNIEXPORT jbyte JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodByte) (JNIEnv *env, jclass that, jobject aJavaObject,
                                   jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.b;
}

extern "C" JNIEXPORT jbyteArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodByteA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                    jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jbyteArray) rc.l;
}

extern "C" JNIEXPORT jchar JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodChar) (JNIEnv *env, jclass that, jobject aJavaObject,
                                   jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.c;
}

extern "C" JNIEXPORT jcharArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodCharA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                    jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jcharArray) rc.l;
}

extern "C" JNIEXPORT jshort JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodShort) (JNIEnv *env, jclass that, jobject aJavaObject,
                                    jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.s;
}

extern "C" JNIEXPORT jshortArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodShortA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                     jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jshortArray) rc.l;
}

extern "C" JNIEXPORT jint JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodInt) (JNIEnv *env, jclass that, jobject aJavaObject,
                                  jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.i;
}

extern "C" JNIEXPORT jintArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodIntA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                   jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jintArray) rc.l;
}

extern "C" JNIEXPORT jlong JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodLong) (JNIEnv *env, jclass that, jobject aJavaObject,
                                   jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.j;
}

extern "C" JNIEXPORT jlongArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodLongA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                    jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jlongArray) rc.l;
}

extern "C" JNIEXPORT jfloat JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodFloat) (JNIEnv *env, jclass that, jobject aJavaObject,
                                    jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.f;
}

extern "C" JNIEXPORT jfloatArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodFloatA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                     jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jfloatArray) rc.l;
}

extern "C" JNIEXPORT jdouble JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodDouble) (JNIEnv *env, jclass that, jobject aJavaObject,
                                     jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.d;
}

extern "C" JNIEXPORT jdoubleArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodDoubleA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                      jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jdoubleArray) rc.l;
}

extern "C" JNIEXPORT jobject JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodObj) (JNIEnv *env, jclass that, jobject aJavaObject,
                                  jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return rc.l;
}

extern "C" JNIEXPORT jobjectArray JNICALL
XPCOMPRIVATE_NATIVE(CallXPCOMMethodObjA) (JNIEnv *env, jclass that, jobject aJavaObject,
                                   jint aMethodIndex, jobjectArray aParams)
{
  jvalue rc;
  CallXPCOMMethod(env, that, aJavaObject, aMethodIndex, aParams, rc);
  return (jobjectArray) rc.l;
}

extern "C" JNIEXPORT void JNICALL
XPCOMPRIVATE_NATIVE(FinalizeStub) (JNIEnv *env, jclass that, jobject aJavaObject)
{
#ifdef DEBUG
  jboolean isCopy;
  jclass clazz = env->GetObjectClass(aJavaObject);
  jstring name = (jstring) env->CallObjectMethod(clazz, getNameMID);
  const char* javaObjectName = env->GetStringUTFChars(name, &isCopy);
  LOG(("*** Finalize(java_obj=%s)\n", javaObjectName));
  if (isCopy)
    env->ReleaseStringUTFChars(name, javaObjectName);
#endif

  void* obj = GetMatchingXPCOMObject(env, aJavaObject);
  NS_ASSERTION(!IsXPTCStub(obj), "Expecting JavaXPCOMInstance, got nsJavaXPTCStub");
  RemoveJavaXPCOMBinding(env, aJavaObject, nsnull);
  delete (JavaXPCOMInstance*) obj;
}

