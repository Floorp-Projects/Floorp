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
#ifndef __bcJavaStubsAndProxies_h
#define __bcJavaStubsAndProxies_h

#include "bcIJavaStubsAndProxies.h"
#include "bcIORB.h"
#include "bcIStub.h"
#include "bcJavaStubsAndProxiesCID.h"

class nsHashtable;

class bcJavaStubsAndProxies : public bcIJavaStubsAndProxies {
    NS_DECL_ISUPPORTS
    NS_IMETHOD GetStub(jobject obj, bcIStub **stub);
    NS_IMETHOD GetOID(char *location, bcOID *); //load component by location
    NS_IMETHOD GetOID(jobject object, bcIORB *orb, bcOID *oid); 
    NS_IMETHOD GetProxy(bcOID oid, const nsIID &iid, bcIORB *orb,  jobject *proxy);
    NS_IMETHOD GetInterface(const nsIID &iid,  jclass *clazz);
    bcJavaStubsAndProxies();
    virtual ~bcJavaStubsAndProxies();
 protected:
    void Init(void);
    static jclass componentLoader;
    static jmethodID loadComponentID;
    static jclass proxyFactory;
    static jmethodID getProxyID;
    static jmethodID getInterfaceID;

    static jclass java_lang_reflect_Proxy;
    static jmethodID getInvocationHandlerID;
    static jclass org_mozilla_xpcom_ProxyHandler;
    static jmethodID getOIDID;
    nsHashtable * oid2objectMap;

};

#endif  /*  __bcJavaStubsAndProxies_h */




