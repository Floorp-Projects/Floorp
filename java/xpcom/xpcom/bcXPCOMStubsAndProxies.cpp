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
#include "nsHashtable.h"
#include "bcXPCOMStubsAndProxies.h"
#include "bcXPCOMStub.h"
#include "bcXPCOMProxy.h"
#include "bcXPCOMLog.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(bcXPCOMStubsAndProxies);

static  nsModuleComponentInfo components[] =
{
    {
        "Black Connect XPCOM stubs and proxies",
	    BC_XPCOMSTUBSANDPROXIES_CID,
        BC_XPCOMSTUBSANDPROXIES_PROGID,
        bcXPCOMStubsAndProxiesConstructor
    }
};

NS_IMPL_NSGETMODULE("BlackConnect XPCOM stubs and proxies",components);

NS_IMPL_ISUPPORTS(bcXPCOMStubsAndProxies,NS_GET_IID(bcXPCOMStubsAndProxies));



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


bcXPCOMStubsAndProxies::bcXPCOMStubsAndProxies() {
    NS_INIT_REFCNT();
    oid2objectMap = new nsSupportsHashtable(256, PR_TRUE);
}

bcXPCOMStubsAndProxies::~bcXPCOMStubsAndProxies() {
    delete oid2objectMap;
}

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetStub(nsISupports *obj, bcIStub **stub) {
    if (!stub) {
	return NS_ERROR_NULL_POINTER;
    }
    *stub = new bcXPCOMStub(obj);
    return NS_OK;
}

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetOID(nsISupports *obj, bcIORB *orb, bcOID *oid) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    bcOID *tmp;
    if (NS_SUCCEEDED(obj->QueryInterface(NS_GET_IID(bcXPCOMProxy),(void**)&tmp))) {
        *oid = *tmp;
        PR_LOG(log, PR_LOG_DEBUG,("--[c++] bcXPCOMProxy::GetOID obj %p is a proxy to oid %d\n",obj,*oid));
    } else {
        bcIStub *stub = NULL;
        GetStub(obj, &stub);
        *oid = orb->RegisterStub(stub);
        oid2objectMap->Put(new bcOIDKey(*oid),obj);
    }
    return NS_OK;
}

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetProxy(bcOID oid, const nsIID &iid, bcIORB *orb, nsISupports **proxy) {
    PRLogModuleInfo *log = bcXPCOMLog::GetLog();
    nsresult rv = NS_OK;
    PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::GetProxy iid=%s\n",iid.ToString()));
    if (!proxy) {
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::GetProxy failed\n"));
        return NS_ERROR_NULL_POINTER;
    }
    *proxy = NULL;
    nsHashKey * key =  new bcOIDKey(oid);
    *proxy = oid2objectMap->Get(key); //doing shortcut
    delete key;
    if (*proxy != NULL) {
        rv = (*proxy)->QueryInterface(iid,(void **)proxy);
        PR_LOG(log, PR_LOG_DEBUG, ("--bcXPCOMStubsAndProxies::GetProxy we have shortcut for oid=%d\n",oid));
    } else {
        *proxy = new bcXPCOMProxy(oid,iid,orb);
        NS_IF_ADDREF(*proxy);
    }
    return rv;
}




