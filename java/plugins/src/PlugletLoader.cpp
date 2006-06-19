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
#include "string.h"
#include "iPlugletEngine.h"
#include "PlugletLoader.h"
#include "PlugletLog.h"
#include "nsCOMPtr.h"

jclass PlugletLoader::clazz = nsnull;
jmethodID PlugletLoader::getMIMEDescriptionMID = nsnull;
jmethodID PlugletLoader::getPlugletMID = nsnull;

//nb for debug only
static char *ToString(jobject obj,JNIEnv *env) {
	static jmethodID toStringID = nsnull;
	if (!toStringID) {
	    jclass clazz = env->FindClass("java/lang/Object");
	    toStringID = env->GetMethodID(clazz,"toString","()Ljava/lang/String;");
	}
	jstring jstr = (jstring) env->CallObjectMethod(obj,toStringID);
	//nb check for jni exception
	const char * str = env->GetStringUTFChars(jstr,nsnull);
	//nb check for jni exception
	char * res = new char[strlen(str)];
	strcpy(res,str);
	env->ReleaseStringUTFChars(jstr,str);
	return res;
}

void PlugletLoader::Initialize(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletLoader::Initialize\n"));
    //nb errors handling
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

    if (!env) {
      return;
    }
    clazz = env->FindClass("org/mozilla/pluglet/PlugletLoader");
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
        clazz = nsnull;
	return;
    }
    clazz = (jclass) env->NewGlobalRef(clazz);
    //nb check for jni exception
    getMIMEDescriptionMID = env->GetStaticMethodID(clazz,"getMIMEDescription","(Ljava/lang/String;)Ljava/lang/String;");
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
        clazz = nsnull;
	return;
    }
    getPlugletMID = env->GetStaticMethodID(clazz,"getPluglet","(Ljava/lang/String;)Lorg/mozilla/pluglet/PlugletFactory;");
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
        clazz = nsnull;
	return;
    }
}

void PlugletLoader::Destroy(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletLoader::destroy\n"));
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
    if (env) {
      env->DeleteGlobalRef(clazz);
    }
}

char * PlugletLoader::GetMIMEDescription(const char * path) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletLoader::GetMIMEDescription\n"));
    if (!clazz) {
	Initialize();
	if (!clazz) {
	    return nsnull;
	}
    }
    nsCOMPtr<iPlugletEngine> plugletEngine;
    nsresult rv = iPlugletEngine::GetInstance(getter_AddRefs(plugletEngine));
    if (NS_FAILED(rv)) {
	return nsnull;
    }
    JNIEnv *env = nsnull;
    rv = plugletEngine->GetJNIEnv(&env);
    if (NS_FAILED(rv)) {
	return nsnull;
    }
    jstring jpath =  env->NewStringUTF(path);
    //nb check for null
    jstring mimeDescription = (jstring)env->CallStaticObjectMethod(clazz,getMIMEDescriptionMID,jpath);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
	return nsnull;
    }
    if (!mimeDescription) {
	return nsnull;
    }
    
    const char* str = env->GetStringUTFChars(mimeDescription,nsnull);
    char *res = nsnull;
    if(str) {
	res = new char[strlen(str)+1];
	strcpy(res,str);
	env->ReleaseStringUTFChars(mimeDescription,str);
    }
    return res;
    //nb possible memory leak - talk to Mozilla
}

jobject PlugletLoader::GetPluglet(const char * path) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletLoader::GetPluglet\n"));
    if (!clazz) {
	Initialize();
	if (!clazz) {
	    return nsnull;
	}
    }    
    nsCOMPtr<iPlugletEngine> plugletEngine;
    nsresult rv = iPlugletEngine::GetInstance(getter_AddRefs(plugletEngine));
    if (NS_FAILED(rv)) {
	return nsnull;
    }
    JNIEnv *env = nsnull;
    rv = plugletEngine->GetJNIEnv(&env);
    if (NS_FAILED(rv)) {
	return nsnull;
    }

    jstring jpath =  env->NewStringUTF(path);
    jobject jpluglet = env->CallStaticObjectMethod(clazz,getPlugletMID,jpath);
    //nb check for jni exc
    if (jpluglet) {
	jpluglet = env->NewGlobalRef(jpluglet);
    }
    return jpluglet;
}
