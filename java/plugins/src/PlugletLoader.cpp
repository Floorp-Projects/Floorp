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
#include "PlugletLoader.h"
#include "PlugletEngine.h"
#include "string.h"

jclass PlugletLoader::clazz = NULL;
jmethodID PlugletLoader::getMIMEDescriptionMID = NULL;
jmethodID PlugletLoader::getPlugletMID = NULL;

//nb for debug only
static char *ToString(jobject obj,JNIEnv *env) {
	static jmethodID toStringID = NULL;
	if (!toStringID) {
	    jclass clazz = env->FindClass("java/lang/Object");
	    toStringID = env->GetMethodID(clazz,"toString","()Ljava/lang/String;");
	}
	jstring jstr = (jstring) env->CallObjectMethod(obj,toStringID);
	//nb check for jni exception
	const char * str = env->GetStringUTFChars(jstr,NULL);
	//nb check for jni exception
	char * res = new char[strlen(str)];
	strcpy(res,str);
	env->ReleaseStringUTFChars(jstr,str);
	return res;
}

void PlugletLoader::Initialize(void) {
    //nb erors handling
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    clazz = env->FindClass("org/mozilla/pluglet/PlugletLoader");
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
        clazz = NULL;
	return;
    }
    clazz = (jclass) env->NewGlobalRef(clazz);
    //nb check for jni exception
    getMIMEDescriptionMID = env->GetStaticMethodID(clazz,"getMIMEDescription","(Ljava/lang/String;)Ljava/lang/String;");
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
        clazz = NULL;
	return;
    }
    getPlugletMID = env->GetStaticMethodID(clazz,"getPluglet","(Ljava/lang/String;)Lorg/mozilla/pluglet/Pluglet;");
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
        clazz = NULL;
	return;
    }
}

void PlugletLoader::Destroy(void) {
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->DeleteGlobalRef(clazz);
}

char * PlugletLoader::GetMIMEDescription(const char * path) {
    if (!clazz) {
	Initialize();
	if (!clazz) {
	    return NULL;
	}
    }
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    jstring jpath =  env->NewStringUTF(path);
    //nb check for null
    jstring mimeDescription = (jstring)env->CallStaticObjectMethod(clazz,getMIMEDescriptionMID,jpath);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
	return NULL;
    }
    if (!mimeDescription) {
	return NULL;
    }
    
    const char* str = env->GetStringUTFChars(mimeDescription,NULL);
    char *res = NULL;
    if(str) {
	res = new char[strlen(str)+1];
	strcpy(res,str);
	env->ReleaseStringUTFChars(mimeDescription,str);
    }
    return res;
    //nb possible memory leak - talk to Mozilla
}

jobject PlugletLoader::GetPluglet(const char * path) {
    if (!clazz) {
	Initialize();
	if (!clazz) {
	    return NULL;
	}
    }    
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    jstring jpath =  env->NewStringUTF(path);
    jobject jpluglet = env->CallStaticObjectMethod(clazz,getPlugletMID,jpath);
    //nb check for jni exc
    if (jpluglet) {
	jpluglet = env->NewGlobalRef(jpluglet);
    }
    return jpluglet;
}








