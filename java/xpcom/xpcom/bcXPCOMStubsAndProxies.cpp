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
#include "bcXPCOMStubsAndProxies.h"
#include "bcXPCOMStub.h"
#include "bcXPCOMProxy.h"

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


bcXPCOMStubsAndProxies::bcXPCOMStubsAndProxies() {
    NS_INIT_REFCNT();
}

bcXPCOMStubsAndProxies::~bcXPCOMStubsAndProxies() {
}

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetStub(nsISupports *obj, bcIStub **stub) {
    if (!stub) {
	return NS_ERROR_NULL_POINTER;
    }
    *stub = new bcXPCOMStub(obj);
    return NS_OK;
}

NS_IMETHODIMP bcXPCOMStubsAndProxies::GetProxy(bcOID oid, const nsIID &iid, bcIORB *orb, nsISupports **proxy) {
    printf("--bcXPCOMStubsAndProxies::GetProxy iid=%s\n",iid.ToString());
    if (!proxy) {
        printf("--bcXPCOMStubsAndProxies::GetProxy failed\n");
        return NS_ERROR_NULL_POINTER;
    }
    *proxy = new bcXPCOMProxy(oid,iid,orb);
    NS_IF_ADDREF(*proxy);
    return NS_OK;
}




