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

#include "PlugletTagInfo2.h"


jclass    PlugletTagInfo2::clazz = NULL;
jmethodID PlugletTagInfo2::initMID = NULL;

void PlugletTagInfo2::Initialize(JNIEnv *env) {
    clazz = env->FindClass("org/mozilla/pluglet/mozilla/PlugletTagInfo2Impl");
    if (!clazz) {
	env->ExceptionDescribe();
	return;
    }
    clazz = (jclass) env->NewGlobalRef(clazz);
    initMID =  env->GetMethodID(clazz,"<init>","(J)V");
    if (!initMID) {
	env->ExceptionDescribe();
        clazz = NULL;
	return;
    }
}

void PlugletTagInfo2::Destroy(JNIEnv *env) {
    //nb  who gonna cal it?
    if(clazz) {
	env->DeleteGlobalRef(clazz);
    }
}

jobject PlugletTagInfo2::GetJObject(JNIEnv *env, const nsIPluginTagInfo2 *info) {
    jobject res = NULL;
    if(!clazz) {
	Initialize(env);
	if (! clazz) {
	    return NULL;
	}
    }
    res = env->NewObject(clazz,initMID,(jlong)info);
    if (!res) {
	env->ExceptionDescribe();
    }
    return res;
}















