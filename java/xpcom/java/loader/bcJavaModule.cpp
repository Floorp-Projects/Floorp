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
#include <fstream.h>
#include "nsCRT.h"
#include "nsIAllocator.h"
#include "nsXPIDLString.h"
#include "nsCOMPtr.h"
#include "bcJavaModule.h"
#include "bcJavaComponentFactory.h"

NS_IMPL_ISUPPORTS(bcJavaModule,NS_GET_IID(nsIModule));
bcJavaModule::bcJavaModule(const char *registryLocation, nsIFile *component) 
    : location(NULL) {
    NS_INIT_REFCNT();    
    nsXPIDLCString str;
    component->GetPath(getter_Copies(str));
    location = nsCRT::strdup(str);
    printf("--JavaModule::JavaModule %s\n",(const char*)str);
}

bcJavaModule::~bcJavaModule() {
    if (location) {
	nsCRT::free((char*)location);
    }
}

/* void getClassObject (in nsIComponentManager aCompMgr, in nsCIDRef aClass, in nsIIDRef aIID, [iid_is (aIID), retval] out nsQIResult result); */
NS_IMETHODIMP bcJavaModule::GetClassObject(nsIComponentManager *aCompMgr, const nsCID & aClass, const nsIID & aIID, void * *result) {
    printf("--JavaModule::GetClassObject\n");
    nsIFactory *f;
    f = new bcJavaComponentFactory(location);
    NS_ADDREF(f);
    *result = f;
    return NS_OK;
}

/* void registerSelf (in nsIComponentManager aCompMgr, in nsIFile location, in string registryLocation, in string componentType); */
NS_IMETHODIMP bcJavaModule::RegisterSelf(nsIComponentManager *aCompMgr, nsIFile *_location, const char *registryLocation, const char *componentType) {
    nsresult result = NS_OK;
    printf("--JavaModule::RegisterSelf\n");
    ifstream in(location);
    char cidStr[500], progid[1000], desc[1000];
    in.getline(cidStr,1000);
    in.getline(progid,1000);
    in.getline(desc,1000);
    printf("%s %s %s", cidStr, progid, desc);
    nsCID  cid;
    cid.Parse((const char *)cidStr);
    aCompMgr->RegisterComponentWithType(cid, desc, progid, _location, registryLocation, PR_TRUE, PR_TRUE, componentType);
    return result;
}

/* void unregisterSelf (in nsIComponentManager aCompMgr, in nsIFile location, in string registryLocation); */
NS_IMETHODIMP bcJavaModule::UnregisterSelf(nsIComponentManager *aCompMgr, nsIFile *_location, const char *registryLocation) { //nb 
    return NS_OK;
}

/* boolean canUnload (in nsIComponentManager aCompMgr); */
NS_IMETHODIMP bcJavaModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *_retval) {
    if (!_retval) {
	return NS_ERROR_NULL_POINTER;
    }
    *_retval = PR_TRUE;
    return NS_OK;
}




