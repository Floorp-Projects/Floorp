/* 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1999 Sun Microsystems,
 * Inc. All Rights Reserved. 
 */

#include "ByteRanges.h"

jfieldID ByteRanges::offsetFID = NULL;
jfieldID ByteRanges::lengthFID = NULL;
jfieldID ByteRanges::currentFID = NULL;

nsByteRange*  ByteRanges::GetByteRanges(JNIEnv *env, jobject object) {
    if (!currentFID) {
	Initialize(env);
	if (!currentFID) {
	    return NULL;
	}
    }
    jint current = 0; //curent is the last one, sorry for this name :)
    current = env->GetIntField(object,currentFID);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	return NULL;
    }
    if (!current) {             //empty ranges
	return NULL;
    }
    jintArray joffset = NULL;
    joffset = (jintArray)env->GetObjectField(object,offsetFID);
    if (!joffset) {
	env->ExceptionDescribe();
	return NULL;
    }
    jintArray jlength = NULL;
    jlength = (jintArray)env->GetObjectField(object,lengthFID);
    if (!jlength) {
	env->ExceptionDescribe();
	return NULL;
    }
    jint *offset = NULL;
    jint *length = NULL;
    offset = env->GetIntArrayElements(joffset,NULL);
    if(!offset) {
	env->ExceptionDescribe();
	return NULL;
    }
    length = env->GetIntArrayElements(joffset,NULL);
    if(!length) {
	env->ExceptionDescribe();
	return NULL;
    }
    nsByteRange  *res = (nsByteRange*)malloc(sizeof(nsByteRange)*current);
    if(!res) {
	env->ReleaseIntArrayElements(joffset,offset,0);
	env->ReleaseIntArrayElements(jlength,length,0);
	return NULL;
    }
    for (int i = 0;i<current;i++) {
	res[i].offset = offset[i];
	res[i].length = length[i];
	res[i].next   = &res[i+1];
    }
    res[i].next = NULL;
    env->ReleaseIntArrayElements(joffset,offset,0);
    env->ReleaseIntArrayElements(jlength,length,0);
    return res;
}

void ByteRanges::FreeByteRanges(nsByteRange * ranges) {
    free(ranges);
}

void ByteRanges::Initialize(JNIEnv *env) {
    jclass clazz = env->FindClass("org/mozilla/pluglet/mozilla/ByteRangesImpl");
    if(!clazz) {
	env->ExceptionDescribe();
	return;
    }
    offsetFID = env->GetFieldID(clazz,"offset","[I");
    if(!offsetFID) {
	env->ExceptionDescribe();
	return;
    }
    lengthFID = env->GetFieldID(clazz,"length","[I");
    if(!lengthFID) {
	env->ExceptionDescribe();
	return;
    }
    currentFID = env->GetFieldID(clazz,"current","I");
    if(!currentFID) {
	env->ExceptionDescribe();
	return;
    }

}
