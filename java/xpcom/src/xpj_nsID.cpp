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
#include "nscore.h" 
#include "nsID.h" 
#include "nsIAllocator.h"

#include "xpjava.h"

#ifdef INCLUDE_JNI_HEADER
#include "org_mozilla_xpcom_nsID.h"
#endif

#define ID_CLASS_NAME "org/mozilla/xpcom/nsID"
#define ID_FIELD_NAME "nsidptr"
#define ID_FIELD_TYPE "J"

/********************** ID **************************/

jobject ID_NewJavaID(JNIEnv *env, const nsIID* iid) {
    jclass clazz = env->FindClass(ID_CLASS_NAME);
    jmethodID initID = env->GetMethodID(clazz, "<init>", "()V");

    jobject result = env->NewObject(clazz, initID);
    nsID *idptr = (nsID *)nsAllocator::Alloc(sizeof(nsID));

    memcpy(idptr, iid, sizeof(nsID));
    ID_SetNative(env, result, idptr);

    return result; 
}

nsID *ID_GetNative(JNIEnv *env, jobject self) {
    jclass clazz = env->FindClass(ID_CLASS_NAME);
    jfieldID nsidptrID = env->GetFieldID(clazz, ID_FIELD_NAME, ID_FIELD_TYPE);

    assert(env->IsInstanceOf(self, clazz));

    jlong nsidptr = env->GetLongField(self, nsidptrID);

    return (nsID *)nsidptr;
}

void ID_SetNative(JNIEnv *env, jobject self, nsID *id) {
    jclass clazz = env->FindClass(ID_CLASS_NAME);
    jfieldID nsidptrID = env->GetFieldID(clazz, ID_FIELD_NAME, ID_FIELD_TYPE);

    assert(env->IsInstanceOf(self, clazz));

    jlong nsidptr = (jlong)id;

    env->SetLongField(self, nsidptrID, nsidptr);
}

jboolean ID_IsEqual(JNIEnv *env, jobject self, jobject other) {
    jboolean result = JNI_FALSE;
    jclass clazz = env->FindClass(ID_CLASS_NAME);
    jfieldID nsidptrID = env->GetFieldID(clazz, ID_FIELD_NAME, ID_FIELD_TYPE);

    assert(env->IsInstanceOf(self, clazz));

    if (other != NULL && env->IsInstanceOf(other, clazz)) {
	nsID *selfid = (nsID *)env->GetLongField(self, nsidptrID);
	nsID *otherid = (nsID *)env->GetLongField(other, nsidptrID);

	if (selfid != NULL && otherid != NULL) {
	    result = selfid->Equals(*otherid);
	}
    }
    return result;
}

/*
 * Class:     ID
 * Method:    NewIDPtr
 * Signature: (ISSBBBBBBBB)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_nsID_NewIDPtr__ISSBBBBBBBB
  (JNIEnv *env, jobject self, jint m0, jshort m1, jshort m2, 
   jbyte m30, jbyte m31, jbyte m32, jbyte m33, 
   jbyte m34, jbyte m35, jbyte m36, jbyte m37) {

    nsID *idptr = (nsID *)nsAllocator::Alloc(sizeof(nsID));
    idptr->m0 = m0;
    idptr->m1 = m1;
    idptr->m2 = m2;
    idptr->m3[0] = m30;
    idptr->m3[1] = m31;
    idptr->m3[2] = m32;
    idptr->m3[3] = m33;
    idptr->m3[4] = m34;
    idptr->m3[5] = m35;
    idptr->m3[6] = m36;
    idptr->m3[7] = m37;

    ID_SetNative(env, self, idptr);
}

/*
 * Class:     ID
 * Method:    NewIDPtr
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_nsID_NewIDPtr__Ljava_lang_String_2
    (JNIEnv *env, jobject self, jstring string) {

    nsID *idptr = (nsID *)nsAllocator::Alloc(sizeof(nsID));

    jboolean isCopy;
    const jbyte *utf = env->GetStringUTFChars(string, &isCopy);
    char *aIDStr;

    if (isCopy) {
	aIDStr = (char *)utf;
    }
    else {
	jsize len = env->GetStringUTFLength(string);
	aIDStr = (char *)nsAllocator::Alloc(len * sizeof(char));
	strncpy(aIDStr, utf, len);
	aIDStr[len - 1] = 0;
    }

    if (!(idptr->Parse(aIDStr))) {
	nsAllocator::Free(idptr);
	idptr = NULL;
    }

    ID_SetNative(env, self, idptr);

    if (!isCopy) {
	nsAllocator::Free(aIDStr);
    }

    env->ReleaseStringUTFChars(string, utf);
}

/*
 * Class:     ID
 * Method:    finalize
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_mozilla_xpcom_nsID_finalize(JNIEnv *env, jobject self) {
    nsID *idptr = ID_GetNative(env, self);

    nsAllocator::Free(idptr);
}

/*
 * Class:     ID
 * Method:    equals
 * Signature: (Ljava/lang/Object;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_mozilla_xpcom_nsID_equals(JNIEnv *env, jobject self, jobject other) {
    return ID_IsEqual(env, self, other);
}

/*
 * Class:     ID
 * Method:    toString
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_org_mozilla_xpcom_nsID_toString(JNIEnv *env, jobject self) {

    nsID *idptr = ID_GetNative(env, self);

    char *idstr = idptr->ToString();
    
    jstring result = env->NewStringUTF(idstr);

    delete [] idstr;

    return result;
}

/*
 * Class:     ID
 * Method:    hashCode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_org_mozilla_xpcom_nsID_hashCode(JNIEnv *env, jobject self) {
    jint result;

    PRUint32 *intptr = (PRUint32 *)ID_GetNative(env, self);

    result = intptr[0] ^ intptr[1] ^ intptr[2] ^ intptr[3];

    return result;
}

