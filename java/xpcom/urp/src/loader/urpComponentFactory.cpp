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
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "urpComponentFactory.h"
#include "bcIXPCOMStubsAndProxies.h"
#include "bcXPCOMStubsAndProxiesCID.h"
#include "bcIORBComponent.h"
#include "bcORBComponentCID.h"

#include "urpStub.h"

static NS_DEFINE_CID(kXPCOMStubsAndProxies,BC_XPCOMSTUBSANDPROXIES_CID);
static NS_DEFINE_CID(kORBComponent,BC_ORBCOMPONENT_CID);

NS_IMPL_THREADSAFE_ISUPPORTS1(urpComponentFactory, nsIFactory)

static nsISupports* compM = nsnull;

urpComponentFactory::urpComponentFactory(const char *_location, const nsCID &aCID) {
    NS_INIT_ISUPPORTS();
    location = nsCRT::strdup(_location);
    aClass = aCID;
}

urpComponentFactory::~urpComponentFactory() {
    nsCRT::free((char*)location);
}

/* void CreateInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); 
*/
NS_IMETHODIMP urpComponentFactory::CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *result) {
    printf("--urpComponentFactory::CreateInstance\n");
    nsresult r;
    if(!compM) {
       NS_WITH_SERVICE(bcIXPCOMStubsAndProxies, xpcomStubsAndProxies, kXPCOMStubsAndProxies, &r);
       if (NS_FAILED(r)) {
           printf("--urpComponentFactory::CreateInstance xpcomStubsAndProxies failed \n");
           return r;
       }
       NS_WITH_SERVICE(bcIORBComponent, _orb, kORBComponent, &r);
       if (NS_FAILED(r)) {
           printf("--urpComponentFactory::CreateInstance bcORB failed \n");
           return r;
       }
       bcIORB *orb;
       _orb->GetORB(&orb);
       bcOID oid;
       urpTransport* transport = new urpConnector();
       PRStatus status = transport->Open("socket,host=indra,port=20009");
       if(status != PR_SUCCESS) {
           printf("Error during opening connection\n");
           exit(-1);
       }
       bcIStub *stub = new urpStub(transport->GetConnection());
       oid = orb->RegisterStub(stub);
       printf("--urpComponentFactory::CreateInstance after GetOID\n");
       nsISupports *proxy;
       printf("--[c++]urpComponentFactory::CreateInstance iid:%s cid:%s\n",iid.ToString(), aClass.ToString());
       xpcomStubsAndProxies->GetProxy(oid, NS_GET_IID(nsIComponentManager), orb, &proxy);
       compM = proxy;
    }
    ((nsIComponentManager*)(compM))->CreateInstance(aClass, nsnull, iid, result);
    printf("--urpComponentFactory::CreateInstance end\n");
    return NS_OK;
}

/* void LockFactory (in PRBool lock); */
NS_IMETHODIMP urpComponentFactory::LockFactory(PRBool lock)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}








