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
 * Sergey Lunegov <lsv@sparc.spb.su>
 */
#include "urpConnectComponent.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"

#include "bcIXPCOMStubsAndProxies.h"
#include "bcXPCOMStubsAndProxiesCID.h"
#include "bcIORBComponent.h"
#include "bcORBComponentCID.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(urpConnectComponent);

static NS_DEFINE_CID(kXPCOMStubsAndProxies,BC_XPCOMSTUBSANDPROXIES_CID);
static NS_DEFINE_CID(kORBComponent,BC_ORBCOMPONENT_CID);

static  nsModuleComponentInfo components[] =
{
    {
        "URP Connect Component",
        URP_CONNECTCOMPONENT_CID,
        URP_CONNECTCOMPONENT_ContractID,
        urpConnectComponentConstructor
    }
};

NS_IMPL_NSGETMODULE("URPConnect component",components);



NS_IMPL_THREADSAFE_ISUPPORTS(urpConnectComponent,NS_GET_IID(urpConnectComponent));

urpConnectComponent::urpConnectComponent() :
    transport(0), connection(0), compM(0), man(0), stub(0), orb(0)
{
    NS_INIT_REFCNT();
}

urpConnectComponent::~urpConnectComponent() {
}

NS_IMETHODIMP urpConnectComponent::GetCompMan(char* cntStr,
	nsISupports** cm) {
    if (!cm) {
        printf("--urpConnectComponent::GetCompMan NS_ERROR_NULL_POINTER\n");
        return NS_ERROR_NULL_POINTER;
    }
    if (!compM) {
	transport = new urpConnector();
	transport->Open(cntStr);
	connection = transport->GetConnection(); 
	man = new urpManager(PR_TRUE, nsnull, connection);
        stub = new urpStub(man, connection);
	nsresult r;
        NS_WITH_SERVICE(bcIXPCOMStubsAndProxies, xpcomStubsAndProxies, kXPCOMStubsAndProxies, &r);
        if (NS_FAILED(r)) {
            printf("--urpConnectComponent::GetCompMan xpcomStubsAndProxies failed \n");
            return NS_ERROR_FAILURE;
        }
        NS_WITH_SERVICE(bcIORBComponent, _orb, kORBComponent, &r);
        if (NS_FAILED(r)) {
            printf("--urpConnectComponent::GetCompMan bcORB failed \n");
            return NS_ERROR_FAILURE;
        }
        _orb->GetORB(&orb);
	bcOID oid = 441450497;
	orb->RegisterStubWithOID(stub, &oid);
	xpcomStubsAndProxies->GetProxy(oid, NS_GET_IID(nsIComponentManager), orb, (nsISupports**)&compM);
    }
    *cm = compM;
    return NS_OK;
}

NS_IMETHODIMP urpConnectComponent::GetTransport(char* cntStr, urpTransport** trans) {
    if(!trans) {
	printf("--urpConnectComponent::GetTransport NS_ERROR_NULL_POINTER\n");
        return NS_ERROR_NULL_POINTER;
    }
    if(!transport) {
	transport = new urpAcceptor();
	transport->Open(cntStr);
    }
    *trans = transport;
    return NS_OK;
}
