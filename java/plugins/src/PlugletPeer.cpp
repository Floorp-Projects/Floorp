/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsISupports.h"
#include "PlugletPeer.h"
#include "iPlugletEngine.h"
#include "nsCOMPtr.h"

jclass    PlugletPeer::clazz = nsnull;
jmethodID PlugletPeer::initMID = nsnull;

void PlugletPeer::Initialize(void) {
    nsCOMPtr<iPlugletEngine> plugletEngine;
    nsresult rv = iPlugletEngine::GetInstance(getter_AddRefs(plugletEngine));
    if (NS_FAILED(rv)) {
	return;
    }
    JNIEnv *env = nsnull;
    rv = plugletEngine->GetJNIEnv(&env);
    if (NS_FAILED(rv)) {
	return;
    }

    clazz = env->FindClass("org/mozilla/pluglet/mozilla/PlugletPeerImpl");
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
        clazz = nsnull;
	return;
    }
    clazz = (jclass) env->NewGlobalRef(clazz);
    initMID =  env->GetMethodID(clazz,"<init>","(J)V");
    if (env->ExceptionOccurred()
	|| !initMID) {
	env->ExceptionDescribe();
        clazz = nsnull;
	return;
    }
}

void PlugletPeer::Destroy(void) {
    //nb  who gonna cal it?
    nsCOMPtr<iPlugletEngine> plugletEngine;
    nsresult rv = iPlugletEngine::GetInstance(getter_AddRefs(plugletEngine));
    if (NS_FAILED(rv)) {
	return;
    }
    JNIEnv *env = nsnull;
    rv = plugletEngine->GetJNIEnv(&env);
    if (NS_FAILED(rv)) {
	return;
    }
    if(clazz) {
	env->DeleteGlobalRef(clazz);
    }
}

jobject PlugletPeer::GetJObject(const nsIPluginInstancePeer *peer) {
    jobject res = nsnull;
    nsCOMPtr<iPlugletEngine> plugletEngine;
    nsresult rv = iPlugletEngine::GetInstance(getter_AddRefs(plugletEngine));
    if (NS_FAILED(rv)) {
	return res;
    }
    JNIEnv *env = nsnull;
    rv = plugletEngine->GetJNIEnv(&env);
    if (NS_FAILED(rv)) {
	return res;
    }
    if(!clazz) {
	Initialize();
	if (! clazz) {
	    return nsnull;
	}
    }
    res = env->NewObject(clazz,initMID,(jlong)peer);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	res = nsnull;
    }
    return res;
}
