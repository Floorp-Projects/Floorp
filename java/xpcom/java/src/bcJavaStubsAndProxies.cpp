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
#include "nsHashtable.h"

jclass bcJavaStubsAndProxies::componentLoader = 0;
jmethodID bcJavaStubsAndProxies::loadComponentID = 0;

jclass bcJavaStubsAndProxies::proxyFactory = 0;
jmethodID bcJavaStubsAndProxies::getProxyID = 0;
jmethodID bcJavaStubsAndProxies::getInterfaceID = 0;

jclass bcJavaStubsAndProxies::java_lang_reflect_Proxy = 0;
jmethodID bcJavaStubsAndProxies::getInvocationHandlerID = 0;
jclass bcJavaStubsAndProxies::org_mozilla_xpcom_ProxyHandler = 0;
jmethodID bcJavaStubsAndProxies::getOIDID = 0;

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


class bcOIDKey : public nsHashKey { 
protected:
    bcOID key;
public:
    bcOIDKey(bcOID oid) {
        key = oid;
    }
    virtual ~bcOIDKey() {
    }
    PRUint32 HashCode(void) const {                                               
        return (PRUint32)key;                                                      
    }                                                                             
                                                                                
    PRBool Equals(const nsHashKey *aKey) const {                                  
        return (key == ((const bcOIDKey *) aKey)->key);                          
    }  
    nsHashKey *Clone() const {                                                    
        return new bcOIDKey(key);                                                 
    }                                      
};

bcJavaStubsAndProxies::bcJavaStubsAndProxies() {
    NS_INIT_REFCNT();
    oid2objectMap = new nsHashtable(256,PR_TRUE);
}

bcJavaStubsAndProxies::~bcJavaStubsAndProxies() {
    delete oid2objectMap;
}

NS_IMETHODIMP bcJavaStubsAndProxies::GetStub(jobject obj, bcIStub **stub) {
    if (!stub) {
        return NS_ERROR_NULL_POINTER;
    }
    *stub = new bcJavaStub(obj);
    return NS_OK;
}

NS_IMETHODIMP bcJavaStubsAndProxies::GetProxy(bcOID oid, const nsIID &iid, bcIORB *orb,  jobject *proxy) {
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    PR_LOG(log,PR_LOG_DEBUG,("--[c++] bcJavaStubsAndProxies::GetProxy\n"));
    if (!componentLoader) {
        Init();
    }
    bcOIDKey *key = new bcOIDKey(oid);
    void *tmp = oid2objectMap->Get(key);
    delete key;
    if (tmp != NULL) { //we have shortcut
        *proxy = (jobject)tmp;
        PR_LOG(log, PR_LOG_DEBUG, ("\n--bcJavaStubsAndProxies::GetProxy we have shortcut for oid=%d\n",oid));
    } else {
        JNIEnv * env = bcJavaGlobal::GetJNIEnv();
        jobject jiid = bcIIDJava::GetObject((nsIID*)&iid);
        *proxy = env->CallStaticObjectMethod(proxyFactory,getProxyID, (jlong)oid, jiid, (jlong)orb);
        EXCEPTION_CHECKING(env);
    }
    return NS_OK;
}

NS_IMETHODIMP bcJavaStubsAndProxies::GetInterface(const nsIID &iid,  jclass *clazz) {
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    PR_LOG(log,PR_LOG_DEBUG,("--[c++] bcJavaStubsAndProxies::GetInterface\n"));
    if (!componentLoader) {
        Init();
    }

    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    jobject jiid = bcIIDJava::GetObject((nsIID*)&iid);
    *clazz = (jclass)env->CallStaticObjectMethod(proxyFactory,getInterfaceID, jiid);
    return NS_OK;
}

NS_IMETHODIMP  bcJavaStubsAndProxies::GetOID(jobject object, bcIORB *orb, bcOID *oid) {
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    nsresult rv = NS_OK;
    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    if (env->IsInstanceOf(object,java_lang_reflect_Proxy)) {
        EXCEPTION_CHECKING(env);
        jobject handler = env->CallStaticObjectMethod(java_lang_reflect_Proxy,getInvocationHandlerID,object);
        EXCEPTION_CHECKING(env);
        if (handler != NULL
            && env->IsInstanceOf(handler,org_mozilla_xpcom_ProxyHandler)) {
            EXCEPTION_CHECKING(env);
            *oid = env->CallLongMethod(handler,getOIDID);
            PR_LOG(log, PR_LOG_DEBUG, ("--bcJavaStubsAndProxies::GetOID we are using old oid %d\n",*oid));
            return rv;
        }
    }
    bcIStub *stub = new bcJavaStub(object);
    *oid = orb->RegisterStub(stub);
    oid2objectMap->Put(new bcOIDKey(*oid),object);
    return rv;

}

NS_IMETHODIMP bcJavaStubsAndProxies::GetOID(char *location, bcOID *oid) { 
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    PR_LOG(log,PR_LOG_DEBUG,("--bcJavaStubsAndProxies::GetOID %s\n",location));
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
        PR_LOG(log,PR_LOG_DEBUG,("--bcJavaStubsAndProxies::GetOID failed\n"));
        return result;
    }
    bcIORB *orb;
    _orb->GetORB(&orb);
    *oid = orb->RegisterStub(stub);
    return NS_OK;
}

void bcJavaStubsAndProxies::Init(void) {
    PRLogModuleInfo *log = bcJavaGlobal::GetLog();
    PR_LOG(log, PR_LOG_DEBUG,("--[c++]bcJavaStubsAndProxies::Init\n"));
    JNIEnv * env = bcJavaGlobal::GetJNIEnv();
    componentLoader = env->FindClass("org/mozilla/xpcom/ComponentLoader");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        PR_LOG(log,PR_LOG_ALWAYS,("--Did you set CLASSPATH correctly\n"));
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

    java_lang_reflect_Proxy  = env->FindClass("java/lang/reflect/Proxy");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }
    
    getInvocationHandlerID = 
        env->GetStaticMethodID(java_lang_reflect_Proxy, "getInvocationHandler",
                               "(Ljava/lang/Object;)Ljava/lang/reflect/InvocationHandler;");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }

    java_lang_reflect_Proxy = (jclass)env->NewGlobalRef(java_lang_reflect_Proxy);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }

    org_mozilla_xpcom_ProxyHandler  = env->FindClass("org/mozilla/xpcom/ProxyHandler");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }
    org_mozilla_xpcom_ProxyHandler = (jclass)env->NewGlobalRef(org_mozilla_xpcom_ProxyHandler);
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }

    getOIDID = env->GetMethodID(org_mozilla_xpcom_ProxyHandler, "getOID","()J");
    if (env->ExceptionOccurred()) {
        env->ExceptionDescribe();
        componentLoader = 0;
        return;
    }

}



