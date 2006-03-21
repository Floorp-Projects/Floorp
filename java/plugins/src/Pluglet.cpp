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
#include "PlugletViewFactory.h"
#include "PlugletLog.h"

jmethodID Pluglet::initializeMID = NULL;
jmethodID Pluglet::startMID = NULL;
jmethodID Pluglet::stopMID = NULL;
jmethodID Pluglet::destroyMID = NULL;
jmethodID Pluglet::newStreamMID = NULL;
jmethodID Pluglet::setWindowMID = NULL;
jmethodID Pluglet::printMID = NULL;

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);

NS_IMPL_ISUPPORTS1(Pluglet,nsIPluginInstance);

Pluglet::Pluglet(jobject object) {
    jthis = PlugletEngine::GetJNIEnv()->NewGlobalRef(object);
    //nb check for null
    peer = NULL;
    view = PlugletViewFactory::GetPlugletView();
    Registry::SetPeer(jthis,(jlong)this);
}

Pluglet::~Pluglet() {
    Registry::Remove(jthis);
    PlugletEngine::GetJNIEnv()->DeleteGlobalRef(jthis);
    NS_RELEASE(peer);
}

NS_METHOD Pluglet::HandleEvent(nsPluginEvent* event, PRBool* handled) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::HandleEvent; stub\n"));
    //nb we do not need it under win32
    return NS_OK;
}

NS_METHOD Pluglet::Initialize(nsIPluginInstancePeer* _peer) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::Initialize\n"));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::GetPeer\n"));
    peer->AddRef();
    *result = peer;
    return NS_OK;
}

NS_METHOD Pluglet::Start(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::Start\n"));
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,startMID);
    if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
     }
    return NS_OK;
}
NS_METHOD Pluglet::Stop(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::Stop\n"));
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,stopMID);
    if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
    }
    return NS_OK;
}
NS_METHOD Pluglet::Destroy(void) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::Destroy\n"));
    JNIEnv * env = PlugletEngine::GetJNIEnv();
    env->CallVoidMethod(jthis,destroyMID);
    if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
     }
    return NS_OK;
}

NS_METHOD Pluglet::NewStream(nsIPluginStreamListener** listener) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::NewStream\n"));
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
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::GetValue; stub\n"));
    return NS_ERROR_FAILURE;
}

NS_METHOD Pluglet::SetWindow(nsPluginWindow* window) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::SetWindow\n"));
    if (view->SetWindow(window) == PR_TRUE) {
	JNIEnv *env = PlugletEngine::GetJNIEnv();
	env->CallVoidMethod(jthis,setWindowMID,view->GetJObject());
	if (env->ExceptionOccurred()) {
            env->ExceptionDescribe();
            return NS_ERROR_FAILURE;
	}
    }
    return NS_OK;
}

NS_METHOD Pluglet::Print(nsPluginPrint* platformPrint) {
    PR_LOG(PlugletLog::log, PR_LOG_DEBUG,
	    ("Pluglet::Print; stub\n"));
    //nb
    return NS_OK;
}










