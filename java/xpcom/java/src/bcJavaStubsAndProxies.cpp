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
 * Igor Kushnirskiy <idk@eng.sun.com>
 */
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "bcJavaStubsAndProxies.h"
#include "bcJavaStub.h"
#include "bcJavaGlobal.h"
#include "bcORB.h"
#include "bcIIDJava.h"

jclass bcJavaStubsAndProxies::componentLoader = 0;
jmethodID bcJavaStubsAndProxies::loadComponentID = 0;

jclass bcJavaStubsAndProxies::proxyFactory = 0;
jmethodID bcJavaStubsAndProxies::getProxyID = 0;
jmethodID bcJavaStubsAndProxies::getInterfaceID = 0;

NS_DEFINE_CID(kORBCIID,BC_ORB_CID);
NS_GENERIC_FACTORY_CONSTRUCTOR(bcJavaStubsAndProxies);

static  nsModuleComponentInfo components[] =
{
    {
        "Black Connect Java stubs and proxies",
	BC_JAVASTUBSANDPROXIES_CID,
        BC_JAVASTUBSANDPROXIES_PROGID,
        bcJavaStubsAndProxiesConstructor
    }
};

NS_IMPL_NSGETMODULE("BlackConnect Java stubs and proxies",components);

NS_IMPL_ISUPPORTS(bcJavaStubsAndProxies,NS_GET_IID(bcJavaStubsAndProxies));


bcJavaStubsAndProxies::bcJavaStubsAndProxies() {
    NS_INIT_REFCNT();
}

bcJavaStubsAndProxies::~bcJavaStubsAndProxies() {
}

NS_IMETHODIMP bcJavaStubsAndProxies::GetStub(jobject obj, bcIStub **stub) {
    if (!stub) {
        return NS_ERROR_NULL_POINTER;
    }
    *stub = new bcJavaStub(obj);
    return NS_OK;
}

NS_IMETHODIMP bcJavaStubsAndProxies::GetProxy(bcOID oid, const nsIID &iid, bcIORB *orb,  jobject *proxy) {
    printf("--[c++] bcJavaStubsAndProxies::GetProxy\n");
    if (!componentLoader) {
        Init();
    }

    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    jobject jiid = bcIIDJava::GetObject((nsIID*)&iid);
    *proxy = env->CallStaticObjectMethod(proxyFactory,getProxyID, (jlong)oid, jiid, (jlong)orb);
    return NS_OK;
}

NS_IMETHODIMP bcJavaStubsAndProxies::GetInterface(const nsIID &iid,  jclass *clazz) {
    printf("--[c++] bcJavaStubsAndProxies::GetInterface\n");
    if (!componentLoader) {
        Init();
    }

    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    jobject jiid = bcIIDJava::GetObject((nsIID*)&iid);
    *clazz = (jclass)env->CallStaticObjectMethod(proxyFactory,getInterfaceID, jiid);
    return NS_OK;
}

NS_IMETHODIMP  bcJavaStubsAndProxies::GetOID(jobject object, bcIORB *orb, bcOID *oid) {
    bcIStub *stub = new bcJavaStub(object);
    *oid = orb->RegisterStub(stub);
    return NS_OK;
}

NS_IMETHODIMP bcJavaStubsAndProxies::GetOID(char *location, bcOID *oid) { 
    printf("--bcJavaStubsAndProxies::GetOID %s\n",location);
    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    nsresult result;
    
    if (!componentLoader) {
        Init();
    }
    //location[strlen(location)-5] = 0; //nb dirty hack. location is xyz.jar.info
    strcpy(location + strlen(location)-4,"comp");
    jstring jstr = env->NewStringUTF(location);
    jobject object = env->CallStaticObjectMethod(componentLoader, loadComponentID, jstr);
    bcIStub *stub = new bcJavaStub(object);
    NS_WITH_SERVICE(bcORB,_orb,kORBCIID,&result);
    if (NS_FAILED(result)) {
        printf("--bcJavaStubsAndProxies::GetOID failed\n");
        return result;
    }
    bcIORB *orb;
    _orb->GetORB(&orb);
    *oid = orb->RegisterStub(stub);
    return NS_OK;
}

void bcJavaStubsAndProxies::Init(void) {
    printf("--[c++]bcJavaStubsAndProxies::Init\n");
    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    componentLoader = env->FindClass("org/mozilla/xpcom/ComponentLoader");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }
    componentLoader = (jclass)env->NewGlobalRef(componentLoader);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }
    loadComponentID = env->GetStaticMethodID(componentLoader,"loadComponent","(Ljava/lang/String;)Ljava/lang/Object;");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }
    proxyFactory = env->FindClass("org/mozilla/xpcom/ProxyFactory");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }
    proxyFactory = (jclass)env->NewGlobalRef(proxyFactory);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }
    getProxyID = env->GetStaticMethodID(proxyFactory, "getProxy","(JLorg/mozilla/xpcom/IID;J)Ljava/lang/Object;");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }
    getInterfaceID = env->GetStaticMethodID(proxyFactory, "getInterface","(Lorg/mozilla/xpcom/IID;)Ljava/lang/Class;");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }



}



