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
#include "nsISupports.h"
#include "jni.h"
#include "bcDefs.h"
#include "bcIStub.h"
#include "bcIORB.h"

/* 58034ea6-1dd2-11b2-9b58-8630abb8af47 */

#define BC_JAVASTUBSANDPROXIES_IID \
 {0x58034ea6, 0x1dd2, 0x11b2,  \
 {0x9b, 0x58, 0x86, 0x30, 0xab, 0xb8, 0xaf,0x47}} 

#define BC_JAVASTUBSANDPROXIES_PROGID "component://netscape/blackwood/blackconnect/java-stubs-and-proxies"

/* 7cadf6e8-1dd2-11b2-9a6e-b1c37844e004 */
#define BC_JAVASTUBSANDPROXIES_CID \
 {0x7cadf6e8, 0x1dd2, 0x11b2,  \
 {0x9a, 0x6e, 0xb1, 0xc3, 0x78,0x44, 0xe0, 0x04}}

class bcJavaStubsAndProxies : public nsISupports {
    NS_DECL_ISUPPORTS
    NS_DEFINE_STATIC_IID_ACCESSOR(BC_JAVASTUBSANDPROXIES_IID)  
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
};

#endif  /*  __bcJavaStubsAndProxies_h */




