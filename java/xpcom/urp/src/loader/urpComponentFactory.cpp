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

#include "urpIConnectComponent.h"
#include "urpConnectComponentCID.h"

#include "../urpLog.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(urpComponentFactory, nsIFactory)

static NS_DEFINE_CID(kConnectComponent,URP_CONNECTCOMPONENT_CID);


urpComponentFactory::urpComponentFactory(const char *_location, const nsCID &aCID) {
    NS_INIT_ISUPPORTS();
    location = nsCRT::strdup(_location);
    aClass = aCID;
}

urpComponentFactory::~urpComponentFactory() {
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("destructor or urpComponentFactory\n"));
    nsCRT::free((char*)location);
}

/* void CreateInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); 
*/
NS_IMETHODIMP urpComponentFactory::CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *result) {
    PRLogModuleInfo *log = urpLog::GetLog();
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentFactory::CreateInstance\n"));
    nsresult r;
    nsIFactory* factory;
    NS_WITH_SERVICE(urpIConnectComponent, conn, kConnectComponent, &r);
    if(NS_FAILED(r)) {
	printf("Error in loading urpIConnectComponent\n");
//	exit(-1);
    }
    nsIComponentManager* compM;
    conn->GetCompMan("socket,host=indra,port=20009", (nsISupports**)&compM);
    compM->FindFactory(aClass, &factory);
    factory->CreateInstance(aOuter, iid, result);
    NS_RELEASE(factory);
    PR_LOG(log, PR_LOG_DEBUG, ("--urpComponentFactory::CreateInstance end\n"));
    return NS_OK;
}

/* void LockFactory (in PRBool lock); */
NS_IMETHODIMP urpComponentFactory::LockFactory(PRBool lock)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}








