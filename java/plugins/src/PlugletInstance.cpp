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
#include "PlugletInstance.h"
#include "PlugletEngine.h"
#include "PlugletStreamListener.h"
#include "PlugletInstancePeer.h"

jmethodID PlugletInstance::initializeMID = NULL;
jmethodID PlugletInstance::startMID = NULL;
jmethodID PlugletInstance::stopMID = NULL;
jmethodID PlugletInstance::destroyMID = NULL;
jmethodID PlugletInstance::newStreamMID = NULL;
jmethodID PlugletInstance::setWindowMID = NULL;
jmethodID PlugletInstance::printMID = NULL;

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);

NS_IMPL_ISUPPORTS(PlugletInstance, kIPluginInstanceIID);

PlugletInstance::PlugletInstance(jobject object) {
    NS_INIT_REFCNT();
    jthis = PlugletEngine::GetJNIEnv()->NewGlobalRef(object);
    //nb check for null
    peer = NULL;
}

PlugletInstance::~PlugletInstance() {
    PlugletEngine::GetJNIEnv()->DeleteGlobalRef(jthis);
    NS_RELEASE(peer);
}

NS_METHOD PlugletInstance::HandleEvent(nsPluginEvent* event, PRBool* handled) {
    //nb
    return NS_OK;
}

NS_METHOD PlugletInstance::Initialize(nsIPluginInstancePeer* _peer) {
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if (!printMID) {
	//nb check for null after each and every JNI call
	jclass clazz = env->FindClass("org/mozilla/pluglet/PlugletInstance");
	initializeMID = env->GetMethodID(clazz,"initialize","(Lorg/mozilla/pluglet/mozilla/PlugletInstancePeer;)V");
	startMID = env->GetMethodID(clazz,"start","()V");
	stopMID = env->GetMethodID(clazz,"stop","()V");
	destroyMID = env->GetMethodID(clazz,"destroy","()V");
	newStreamMID = env->GetMethodID(clazz,"newStream","()Lorg/mozilla/pluglet/PlugletStreamListener;");
	setWindowMID = env->GetMethodID(clazz,"setWindow","(Ljava/awt/Panel;)V");
	printMID = env->GetMethodID(clazz,"print","(Ljava/awt/print/PrinterJob;)V");
    }
    peer = _peer;
    peer->AddRef();
    jobject obj = PlugletInstancePeer::GetJObject(peer);
    if (!obj) {
	return NS_ERROR_FAILURE;
    }
    env->CallVoidMethod(jthis,initializeMID,obj);
    return NS_OK;
}

NS_METHOD PlugletInstance::GetPeer(nsIPluginInstancePeer* *result) {
    peer->AddRef();
    *result = peer;
    return NS_OK;
}

NS_METHOD PlugletInstance::Start(void) {
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,startMID);
    //nb check for JNI exception
    return NS_OK;
}
NS_METHOD PlugletInstance::Stop(void) {
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,stopMID);
    //nb check for JNI exception
    return NS_OK;
}
NS_METHOD PlugletInstance::Destroy(void) {
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,destroyMID);
    //nb check for JNI exception
    return NS_OK;
}

NS_METHOD PlugletInstance::NewStream(nsIPluginStreamListener** listener) {
    if(!listener) {
	return NS_ERROR_FAILURE;
    }
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    jobject obj = env->CallObjectMethod(jthis,newStreamMID);
    //nb check for JNI exception
    if (obj) {
	*listener = new PlugletStreamListener(obj);
    }
    return NS_OK;
}

NS_METHOD PlugletInstance::GetValue(nsPluginInstanceVariable variable, void *value) {
    return NS_ERROR_FAILURE;
}

NS_METHOD PlugletInstance::SetWindow(nsPluginWindow* window) {
    //nb
    return NS_OK;
}

NS_METHOD PlugletInstance::Print(nsPluginPrint* platformPrint) {
    //nb
    return NS_OK;
}


