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
#include "PlugletInputStream.h"
#include "nsCOMPtr.h"
#include "iPlugletEngine.h"

jclass    PlugletInputStream::clazz = NULL;
jmethodID PlugletInputStream::initMID = NULL;

void PlugletInputStream::Initialize(void) {
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

    clazz = env->FindClass("org/mozilla/pluglet/mozilla/PlugletInputStream");
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

void PlugletInputStream::Destroy(void) {
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

jobject PlugletInputStream::GetJObject(const nsIInputStream *stream) {
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
    res = env->NewObject(clazz,initMID,(jlong)stream);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	res = nsnull;
    }
    return res;
}
