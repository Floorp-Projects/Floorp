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
#include "Pluglet.h"
#include "PlugletEngine.h"
#include "PlugletStreamListener.h"
#include "PlugletPeer.h"
#include "Registry.h"

jmethodID Pluglet::initializeMID = NULL;
jmethodID Pluglet::startMID = NULL;
jmethodID Pluglet::stopMID = NULL;
jmethodID Pluglet::destroyMID = NULL;
jmethodID Pluglet::newStreamMID = NULL;
jmethodID Pluglet::setWindowMID = NULL;
jmethodID Pluglet::printMID = NULL;

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);

NS_IMPL_ISUPPORTS(Pluglet, kIPluginInstanceIID);

Pluglet::Pluglet(jobject object) {
    NS_INIT_REFCNT();
    jthis = PlugletEngine::GetJNIEnv()->NewGlobalRef(object);
    //nb check for null
    peer = NULL;
    view = new PlugletView();
    Registry::SetPeer(jthis,(jlong)this);
}

Pluglet::~Pluglet() {
    Registry::Remove(jthis);
    PlugletEngine::GetJNIEnv()->DeleteGlobalRef(jthis);
    NS_RELEASE(peer);
}

NS_METHOD Pluglet::HandleEvent(nsPluginEvent* event, PRBool* handled) {
    //nb we do not need it under win32
    return NS_OK;
}

NS_METHOD Pluglet::Initialize(nsIPluginInstancePeer* _peer) {
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if (!printMID) {
	jclass clazz = env->FindClass("org/mozilla/pluglet/Pluglet");
	if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
        }
	initializeMID = env->GetMethodID(clazz,"initialize","(Lorg/mozilla/pluglet/mozilla/PlugletPeer;)V");
        if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
        }
	startMID = env->GetMethodID(clazz,"start","()V");
	if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
        }
	stopMID = env->GetMethodID(clazz,"stop","()V");
	if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
        }
	destroyMID = env->GetMethodID(clazz,"destroy","()V");
	if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
        }
	newStreamMID = env->GetMethodID(clazz,"newStream","()Lorg/mozilla/pluglet/PlugletStreamListener;");
	if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
        }
	setWindowMID = env->GetMethodID(clazz,"setWindow","(Ljava/awt/Frame;)V");
	if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
        }
	printMID = env->GetMethodID(clazz,"print","(Ljava/awt/print/PrinterJob;)V");
	if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
        }
    }
    peer = _peer;
    peer->AddRef();
    jobject obj = PlugletPeer::GetJObject(peer);
    if (!obj) {
	return NS_ERROR_FAILURE;
    }
    env->CallVoidMethod(jthis,initializeMID,obj);
    return NS_OK;
}

NS_METHOD Pluglet::GetPeer(nsIPluginInstancePeer* *result) {
    peer->AddRef();
    *result = peer;
    return NS_OK;
}

NS_METHOD Pluglet::Start(void) {
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,startMID);
    if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
     }
    return NS_OK;
}
NS_METHOD Pluglet::Stop(void) {
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,stopMID);
    if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}
NS_METHOD Pluglet::Destroy(void) {
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,destroyMID);
    if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
     }
    return NS_OK;
}

NS_METHOD Pluglet::NewStream(nsIPluginStreamListener** listener) {
    if(!listener) {
	return NS_ERROR_FAILURE;
    }
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    jobject obj = env->CallObjectMethod(jthis,newStreamMID);
    if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
    }
    if (obj) {
	*listener = new PlugletStreamListener(obj);
	(*listener)->AddRef();
    }
    return NS_OK;
}

NS_METHOD Pluglet::GetValue(nsPluginInstanceVariable variable, void *value) {
    return NS_ERROR_FAILURE;
}

NS_METHOD Pluglet::SetWindow(nsPluginWindow* window) {
    JNIEnv *env = PlugletEngine::GetJNIEnv();
    if (view->SetWindow(window)) {
      env->CallVoidMethod(jthis,setWindowMID,view->GetJObject());
    }
    return NS_OK;
}

NS_METHOD Pluglet::Print(nsPluginPrint* platformPrint) {
    //nb
    return NS_OK;
}










