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
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "urpComponentFactory.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(urpComponentFactory, nsIFactory)

//static nsISupports* compM = nsnull;
//nsCOMPtr<nsIComponentManager> compM = nsnull;

urpComponentFactory::urpComponentFactory(const char *_location, const nsCID &aCID, nsISupports* m) {
    NS_INIT_ISUPPORTS();
    location = nsCRT::strdup(_location);
    aClass = aCID;
    compM = (nsIComponentManager*)m;
}

urpComponentFactory::~urpComponentFactory() {
    printf("destructor or urpComponentFactory\n");
    NS_RELEASE(compM);
    nsCRT::free((char*)location);
}

/* void CreateInstance (in nsISupports aOuter, in nsIIDRef iid, [iid_is (iid), retval] out nsQIResult result); 
*/
NS_IMETHODIMP urpComponentFactory::CreateInstance(nsISupports *aOuter, const nsIID & iid, void * *result) {
    printf("--urpComponentFactory::CreateInstance\n");
    nsresult r;
    nsIFactory* factory;
    ((nsIComponentManager*)(compM))->FindFactory(aClass, &factory);
    factory->CreateInstance(aOuter, iid, result);
    NS_RELEASE(factory);
    printf("--urpComponentFactory::CreateInstance end\n");
    return NS_OK;
}

/* void LockFactory (in PRBool lock); */
NS_IMETHODIMP urpComponentFactory::LockFactory(PRBool lock)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}








