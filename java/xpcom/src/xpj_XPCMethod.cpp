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
#include "nsIFactory.h" 
#include "nsIComponentManager.h" 
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"
#include "xptinfo.h"
#include "xptcall.h"
#include "xpt_struct.h"
#include "nsIAllocator.h"

#include "xpjava.h"

#ifdef INCLUDE_JNI_HEADER
#include "org_mozilla_xpcom_XPCMethod.h"
#endif

static jfieldID XPCMethod_infoptr_ID = NULL;
static jfieldID XPCMethod_frameptr_ID = NULL;
static jfieldID XPCMethod_offset_ID = NULL;
static jfieldID XPCMethod_count_ID = NULL;

jclass classXPCMethod = NULL;

#undef USE_PARAM_TEMPLATE

#ifdef __cplusplus
extern "C" {
#endif

/* Because not all platforms convert jlong to "long long" 
 *
 * NOTE: this code was cut&pasted to other places, with tweaks.
 *   Normally I wouldn't do this, but my reasons are:
 *
 *   1. My alternatives were to put it in xpjava.h or xpjava.cpp
 *      I'd like to take stuff *out* of xpjava.h, and putting it 
 *      in xpjava.cpp would preclude inlining.
 *
 *   2. How we represent methods in Java is an implementation 
 *      detail, which may change in the future; this entire class
 *      may disappear in favor of normal Java reflection, or change
 *      drastically.  Thus ToPtr/ToJLong is only of interest to those 
 *      objects that encode pointers as jlongs, which is kind of a 
 *      kludge to begin with.
 *
 *   -- frankm, 99.09.09
 */
static inline jlong ToJLong(const void *p) {
    jlong result;
    jlong_I2L(result, (int)p);
    return result;
}

static inline void* ToPtr(jlong l) {
    int result;
    jlong_L2I(result, l);
    return (void *)result;
}

/*
 * Class:     XPCMethod
 * Method:    init
 * Signature: (Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL 
    Java_org_mozilla_xpcom_XPCMethod_init(JNIEnv *env, jobject self, 
					  jobject iid, jstring methodName) {
    int offset;
    const nsXPTMethodInfo *mi;
    nsID *iidPtr = ID_GetNative(env, iid);
    const char *tmpstr;
    nsresult res;

    // Get interface info
    nsIInterfaceInfo *info = nsnull;

    res = (XPTI_GetInterfaceInfoManager())->GetInfoForIID(iidPtr, &info);

    if (NS_FAILED(res)) {
	cerr << "Failed to find info for " << iidPtr->ToString() << endl;
	return res;
    }

    tmpstr = env->GetStringUTFChars(methodName, NULL);
    res = xpj_GetMethodInfoByName(iidPtr, tmpstr, PR_FALSE, &mi, &offset);
    if (NS_FAILED(res)) {
	cerr << "Cannot find method for: " << (char *)tmpstr << endl;
	return -1;
    }
    env->ReleaseStringUTFChars(methodName, tmpstr);

    // Store argptr
    // PENDING: add iid as an argument

    if (XPCMethod_infoptr_ID == NULL) {
	classXPCMethod = env->GetObjectClass(self);

	classXPCMethod = (jclass)env->NewGlobalRef(classXPCMethod);
	if (classXPCMethod == NULL) return -1;

	XPCMethod_infoptr_ID = env->GetFieldID(classXPCMethod,
					       "infoptr",
					       "J");

	if (XPCMethod_infoptr_ID == NULL) {
	    cerr << "Field id for infoptr not found" << endl;
	    return -1;
	}

	XPCMethod_frameptr_ID = env->GetFieldID(classXPCMethod,
						"frameptr",
						"J");

	if (XPCMethod_frameptr_ID == NULL) {
	    cerr << "Field id for frameptr not found" << endl;
	    return -1;
	}

	XPCMethod_count_ID = env->GetFieldID(classXPCMethod,
						 "count",
						 "I");

	if (XPCMethod_count_ID == NULL) {
	    cerr << "Field id for count not found" << endl;
	    return -1;
	}

	XPCMethod_offset_ID = env->GetFieldID(classXPCMethod,
						  "offset",
						  "I");

	if (XPCMethod_offset_ID == NULL) {
	    cerr << "Field id for offset not found" << endl;
	    return -1;
	}
    }

    env->SetLongField(self, 
		      XPCMethod_infoptr_ID, 
		      ToJLong(info));

    env->SetIntField(self, 
		     XPCMethod_count_ID, 
		     (jint)mi->GetParamCount());

#ifdef USE_PARAM_TEMPLATE
    // Build parameter array
    nsXPTCVariant *variantPtr = NULL;
    xpjd_BuildParamsForOffset(info, offset, 0, &variantPtr);

    env->SetLongField(self, 
		      XPCMethod_frameptr_ID, 
		      ToJLong(variantPtr));
#else
    env->SetLongField(self, 
		      XPCMethod_frameptr_ID, 
		      ToJLong(NULL));
#endif

    // Return offset
    return offset;
}

/*
 * Class:     XPCMethod
 * Method:    destroyPeer
 * Signature: (J)V
 */
JNIEXPORT void JNICALL 
    Java_org_mozilla_xpcom_XPCMethod_destroyPeer(JNIEnv *env, 
						 jobject self, jlong peer) {

    nsXPTCVariant *variantPtr = (nsXPTCVariant *)ToPtr(peer);
    if (variantPtr != NULL) {
	nsAllocator::Free(variantPtr);
    }
}

/*
 * Class:     XPCMethod
 * Method:    getParameterType
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_xpcom_XPCMethod_getParameterType
    (JNIEnv *env, jobject self, jint index) {

    jint paramcount = env->GetIntField(self, XPCMethod_count_ID);

    if (index >= paramcount || index < 0) {
	cerr << "Out of range: " << index << endl;
	return -1;
    }
    else {
	jint  offset = env->GetIntField(self, XPCMethod_offset_ID);
	jlong ptrval = env->GetLongField(self, XPCMethod_infoptr_ID);

	const nsXPTMethodInfo *mi;
	nsIInterfaceInfo *info = (nsIInterfaceInfo *)ToPtr(ptrval);

	info->GetMethodInfo(offset, &mi);
	return mi->GetParam(index).GetType().TagPart();
    }
}

/*
 * Class:     XPCMethod
 * Method:    getParameterClass
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_xpcom_XPCMethod_getParameterClass
    (JNIEnv *env, jobject self, jint index) {
    cerr << "Unimplemented call" << endl;
    return 0;
}

/*
 * Class:     XPCMethod
 * Method:    invoke
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_XPCMethod_invoke
  (JNIEnv *env, jobject self, jobject target, jobjectArray params) {
    nsresult res;

    if (XPCMethod_infoptr_ID == NULL) {
	cerr << "Field id for infoptr not initialized" << endl;
	return;
    }

    jint offset = env->GetIntField(self, XPCMethod_offset_ID);

    jlong infoptr = env->GetLongField(self, XPCMethod_infoptr_ID);
    nsIInterfaceInfo *info = 
	(nsIInterfaceInfo *)ToPtr(infoptr);

#ifdef USE_PARAM_TEMPLATE
    jint paramcount = env->GetIntField(self, XPCMethod_count_ID);

    nsXPTCVariant *paramTemplate = 
	(nsXPTCVariant *)ToPtr(env->GetLongField(self, XPCMethod_frameptr_ID));

    nsXPTCVariant variantArray[paramcount];

    memcpy(variantArray, paramTemplate, paramcount * sizeof(nsXPTCVariant));
    // Fix pointers
    for (int i = 0; i < paramcount; i++) {
	nsXPTCVariant *current = &(variantArray[i]);
	if (current->flags == nsXPTCVariant::PTR_IS_DATA) {
	    current->ptr = &current->val;
	}
    }
#endif
    nsIID* iid = nsnull;
    nsISupports *true_target = nsnull;

    // XXX: check for success
    info->GetIID(&iid);

    // XXX: check for success
    xpjp_QueryInterfaceToXPCOM(env, target, *iid, (void**)&true_target);

    xpjd_InvokeMethod(env, true_target, info, offset, params);

    xpjp_SafeRelease(true_target);
    nsAllocator::Free(iid);
}

#ifdef __cplusplus
}
#endif

