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
#include "nsISupports.h"
#include "PlugletInstancePeer.h"
#include "PlugletEngine.h"

jclass    PlugletInstancePeer::clazz = NULL;
jmethodID PlugletInstancePeer::initMID = NULL;

void PlugletInstancePeer::Initialize(void) {
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    clazz = env->FindClass("org/mozilla/pluglet/mozilla/PlugletInstancePeerImpl");
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
        clazz = NULL;
	return;
    }
    clazz = (jclass) env->NewGlobalRef(clazz);
    initMID =  env->GetMethodID(clazz,"<init>","(J)V");
    if (env->ExceptionOccurred()
	|| !initMID) {
	env->ExceptionDescribe();
        clazz = NULL;
	return;
    }
}

void PlugletInstancePeer::Destroy(void) {
    //nb  who gonna cal it?
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    if(clazz) {
	env->DeleteGlobalRef(clazz);
    }
}

jobject PlugletInstancePeer::GetJObject(const nsIPluginInstancePeer *stream) {
    jobject res = NULL;
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if(!clazz) {
	Initialize();
	if (! clazz) {
	    return NULL;
	}
    }
    res = env->NewObject(clazz,initMID,(jlong)stream);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	res = NULL;
    }
    return res;
}
