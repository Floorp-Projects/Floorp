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

#define USE_PARAM_TEMPLATE

#ifdef __cplusplus
extern "C" {
#endif

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

    // Get method info
    tmpstr = env->GetStringUTFChars(methodName, NULL);
    res = GetMethodInfoByName(iidPtr, tmpstr, PR_FALSE, &mi, &offset);

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
		      (jlong)mi);

    env->SetIntField(self, 
		     XPCMethod_count_ID, 
		     (jint)mi->GetParamCount());

#ifdef USE_PARAM_TEMPLATE
    // Build parameter array
    nsXPTCVariant *variantPtr = new nsXPTCVariant[mi->GetParamCount()];
    BuildParamsForMethodInfo(mi, variantPtr);

    env->SetLongField(self, 
		      XPCMethod_frameptr_ID, 
		      (jlong)variantPtr);
#else
    env->SetLongField(self, 
		      XPCMethod_frameptr_ID, 
		      (jlong)0);
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

    nsXPTCVariant *variantPtr = (nsXPTCVariant *)peer;
    delete [] variantPtr;
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
	const nsXPTMethodInfo *mi =
	    (const nsXPTMethodInfo *)env->GetLongField(self, 
						       XPCMethod_infoptr_ID);
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
    jint paramcount = env->GetIntField(self, XPCMethod_count_ID);

    nsXPTCVariant variantArray[paramcount];

#ifdef USE_PARAM_TEMPLATE
    nsXPTCVariant *paramTemplate = 
	(nsXPTCVariant *)env->GetLongField(self, XPCMethod_frameptr_ID);

    memcpy(variantArray, paramTemplate, paramcount * sizeof(nsXPTCVariant));
    // Fix pointers
    for (int i = 0; i < paramcount; i++) {
	nsXPTCVariant *current = &(variantArray[i]);
	if (current->flags == nsXPTCVariant::PTR_IS_DATA) {
	    current->ptr = &current->val;
	}
    }
#else
    const nsXPTMethodInfo *mi =
	(const nsXPTMethodInfo *)env->GetLongField(self, 
						   XPCMethod_infoptr_ID);
    BuildParamsForMethodInfo(mi, variantArray);
#endif;

    res = JArrayToVariant(env, paramcount, variantArray, params);

    if (NS_FAILED(res)) {
	// PENDING: throw an exception
	cerr << "JArrayToVariant failed, status: " << res << endl;
	return;
    }

    nsISupports* true_target = This(env, target);

    res = XPTC_InvokeByIndex(true_target, 
			     (PRUint32)offset,
			     (PRUint32)paramcount, 
			     variantArray);

    if (NS_FAILED(res)) {
	// PENDING: throw an exception
	cerr << "Method failed, status: " << res << endl;
	return;
    }

    res = VariantToJArray(env, params, paramcount, variantArray);

    if (NS_FAILED(res)) {
	// PENDING: throw an exception
	cerr << "VariantToJArray failed, status: " << res << endl;
	return;
    }

}

#ifdef __cplusplus
}
#endif

