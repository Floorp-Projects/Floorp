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
#include "PlugletStreamListener.h"
#include "PlugletEngine.h"
#include "PlugletStreamInfo.h"
#include "PlugletInputStream.h"
#include "PlugletLog.h"

jmethodID PlugletStreamListener::onStartBindingMID = NULL;
jmethodID PlugletStreamListener::onDataAvailableMID = NULL;
jmethodID PlugletStreamListener::onFileAvailableMID = NULL;
jmethodID PlugletStreamListener::onStopBindingMID = NULL;
jmethodID PlugletStreamListener::getStreamTypeMID = NULL;

void PlugletStreamListener::Initialize(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamListener::Initialize\n"));
    JNIEnv * env =  PlugletEngine::GetJNIEnv();
    jclass clazz = env->FindClass("org/mozilla/pluglet/PlugletStreamListener");
    onStartBindingMID = env->GetMethodID(clazz, "onStartBinding","(Lorg/mozilla/pluglet/mozilla/PlugletStreamInfo;)V");
    onDataAvailableMID = env->GetMethodID(clazz,"onDataAvailable","(Lorg/mozilla/pluglet/mozilla/PlugletStreamInfo;Ljava/io/InputStream;I)V");
    onFileAvailableMID = env->GetMethodID(clazz,"onFileAvailable","(Lorg/mozilla/pluglet/mozilla/PlugletStreamInfo;Ljava/lang/String;)V");
    getStreamTypeMID = env->GetMethodID(clazz,"getStreamType","()I");
    onStopBindingMID = env->GetMethodID(clazz,"onStopBinding","(Lorg/mozilla/pluglet/mozilla/PlugletStreamInfo;I)V");
}

PlugletStreamListener::PlugletStreamListener(jobject object) {
    jthis = PlugletEngine::GetJNIEnv()->NewGlobalRef(object);
    if (!onStopBindingMID) {
	Initialize();
    }
}

PlugletStreamListener::~PlugletStreamListener(void) {
    PlugletEngine::GetJNIEnv()->DeleteGlobalRef(jthis);
}

NS_METHOD  PlugletStreamListener::OnStartBinding(nsIPluginStreamInfo* pluginInfo) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamListener::OnStartBinding\n"));
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,onStartBindingMID,PlugletStreamInfo::GetJObject(pluginInfo));
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}
    
NS_METHOD PlugletStreamListener::OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamListener::OnDataAvailable\n"));
    JNIEnv * env = PlugletEngine::GetJNIEnv();   
    env->CallVoidMethod(jthis,onDataAvailableMID,PlugletStreamInfo::GetJObject(pluginInfo), 
			PlugletInputStream::GetJObject(input),(jint)length);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_METHOD PlugletStreamListener::OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamListener::OnFileAvailable\n"));
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,onFileAvailableMID,PlugletStreamInfo::GetJObject(pluginInfo),
	env->NewStringUTF(fileName));
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_METHOD PlugletStreamListener::OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamListener::OnStopBinding\n"));
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,onStopBindingMID,PlugletStreamInfo::GetJObject(pluginInfo),status);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

NS_METHOD PlugletStreamListener::GetStreamType(nsPluginStreamType *result) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("PlugletStreamListener::GetStreamType\n"));
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    *result = (nsPluginStreamType)env->CallIntMethod(jthis,getStreamTypeMID);
    if (env->ExceptionOccurred()) {
	env->ExceptionDescribe();
	return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

NS_IMPL_ISUPPORTS1(PlugletStreamListener, nsIPluginStreamListener);





