/*
 * The contents of this file are subject to the Mozilla Public License 
 * Version 1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for 
 * the specific language governing rights and limitations under the License.
 *
 * Contributors:
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
#include <iostream.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <jni.h>
#include "nscore.h" 
#include "nsID.h" 
#include "nsIFactory.h" 
#include "nsIComponentManager.h" 
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "nsIAllocator.h"

#include "xpjava.h"

#ifdef INCLUDE_JNI_HEADER
#include "org_mozilla_xpcom_XPComUtilities.h"
#endif

/* ---------- CLASS METHODS ------------ */

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL 
    Java_org_mozilla_xpcom_XPComUtilities_initialize(JNIEnv *env, jclass cls)
{
    nsresult res = xpj_InitXPCOM();
    if (NS_FAILED(res)) return JNI_FALSE;

    return xpjd_InitJavaCaches(env); // XXX: remove; do in dispatch
}

/*
 * Class:     XPComUtilities
 * Method:    CreateInstance
 * Signature: (LID;LnsISupports;LID;)Ljava/lang/Object;
 */
JNIEXPORT jobject JNICALL Java_org_mozilla_xpcom_XPComUtilities_CreateInstance
  (JNIEnv *env, jclass clazz, 
   jobject jcid, jobject jdelegate, jobject jiid)
{
    jobject result;
    nsISupports *xpcobj = NULL; 
    nsresult res;
    nsID *classID = ID_GetNative(env, jcid);
    nsID *interfaceID = ID_GetNative(env, jiid);
    nsISupports *delegate = NULL;

    if (jdelegate != NULL) {
	xpjp_QueryInterfaceToXPCOM(env, 
				   jdelegate, 
				   NS_GET_IID(nsISupports),
				   (void **)&delegate);
    }

    assert(classID != NULL && interfaceID != NULL);

    // Create Instance
    //cerr << "Creating Sample Instance" << endl;

    nsIComponentManager *manager;

    res = NS_GetGlobalComponentManager(&manager);

    if (NS_FAILED(res)) {
      cerr << "Failed to get component manager" << endl;
      return NULL;
    }

    res = manager->CreateInstance(*classID,
				  delegate, 
				  *interfaceID,
				  (void **) &xpcobj); 

    if (NS_FAILED(res)) {
	cerr << "Failed to create instance" << endl;
	return NULL;
    }
    // cerr << "Wrapping " << sample << " in new ComObject" << endl;

    // Wrap it in an ComObject
    result = xpjp_QueryInterfaceToJava(env, xpcobj, *interfaceID);

    return result;    
}

/*
 * Class:     XPComUtilities
 * Method:    RegisterComponent
 * Signature: (LID;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;ZZ)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_XPComUtilities_RegisterComponent
  (JNIEnv *env, jclass clazz, 
   jobject cid, jstring className, jstring progID, jstring libraryName, 
   jboolean replace, jboolean persist)
{
    nsresult res;
    const nsID *aClass = ID_GetNative(env, cid);

    const char *c_className = 
	(const char *)env->GetStringUTFChars(className, NULL);

    const char *c_progID = 
	(const char *)env->GetStringUTFChars(progID, NULL);

    // PENDING: convert from library-independent to library-dependent?
    const char *c_libraryName = 
	(const char *)env->GetStringUTFChars(libraryName, NULL);

    res = nsComponentManager::RegisterComponent(*aClass,
						c_className,
						c_progID,
						c_libraryName,
						(PRBool)replace,
						(PRBool)persist);

    env->ReleaseStringUTFChars(className, c_className);

    env->ReleaseStringUTFChars(progID, c_progID);

    env->ReleaseStringUTFChars(libraryName, c_libraryName);

    if (NS_FAILED(res)) {
	cerr << "Failed to register factory" << endl;
    }
}

/*
 * Class:     XPComUtilities
 * Method:    AddRef
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_XPComUtilities_AddRef
    (JNIEnv *env, jclass cls, jlong ref)
{
    xpjp_SafeAddRefProxyID(ref);
}

/*
 * Class:     XPComUtilities
 * Method:    Release
 * Signature: (J)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_XPComUtilities_Release
    (JNIEnv *env, jclass cls, jlong ref)
{
    xpjp_SafeReleaseProxyID(ref);
}

/*
 * Class:     XPComUtilities
 * Method:    InvokeMethodByIndex
 * Signature: (LID;IJ[Ljava/lang/Object;)V
 */
JNIEXPORT void JNICALL 
    Java_org_mozilla_xpcom_XPComUtilities_InvokeMethodByIndex(JNIEnv *env, 
					    jclass cls, 
					    jobject iid, 
					    jlong ref, 
					    jint index, 
					    jobjectArray args) {
    nsIInterfaceInfo *info = nsnull;
    nsISupports *target;

    // XXX: check for success
    xpjd_GetInterfaceInfo(env, iid, &info);

    // XXX: s.b. just |xpjp_UnwrapProxy(env, self);|
    nsIID *nativeIID = ID_GetNative(env, iid);
    xpjp_QueryInterfaceForProxyID(ref, *nativeIID, (void**)&target);

    xpjd_InvokeMethod(env, target, info, index, args);

    // XXX: remove when ref not QueryInterface'd
    xpjp_SafeReleaseProxyID(ref);
}

#ifdef __cplusplus
}
#endif
