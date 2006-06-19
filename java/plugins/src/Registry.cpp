/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- 
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

#include "iPlugletEngine.h"
#include "Registry.h"
#include "nsCOMPtr.h"

jclass Registry::clazz = NULL;
jmethodID Registry::setPeerMID = NULL;
jmethodID Registry::removeMID = NULL;

void Registry::SetPeer(jobject key, jlong peer) {
    if (!clazz) {
        Initialize();
        if(!clazz) {
            return;
        }
    }
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

    env->CallStaticVoidMethod(clazz,setPeerMID,key,peer);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        return;
    }
}

void Registry::Remove(jobject key) {
    if (!clazz) {   // it is impossible
        Initialize();
        if(!clazz) {
            return;
        }
    }
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

    env->CallStaticVoidMethod(clazz,removeMID,key);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        return;
    }
}

void Registry::Initialize() {
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

    if(!env) {
        return;
    }
    clazz = env->FindClass("org/mozilla/pluglet/Registry");
    if(!clazz) {
        env->ExceptionDescribe();
	    return;
	}
    setPeerMID = env->GetStaticMethodID(clazz,"setPeer","(Ljava/lang/Object;J)V");
    if (!setPeerMID) {
        env->ExceptionDescribe();
        clazz = NULL;
        return;
    }
    removeMID = env->GetStaticMethodID(clazz,"remove","(Ljava/lang/Object;)V");
    if (!removeMID) {
        env->ExceptionDescribe();
        clazz = NULL;
        return;
    }
}











