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
#include "nsXPIDLServiceManager.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsXPIDLServiceManager,nsIXPIDLServiceManager);
nsXPIDLServiceManager::nsXPIDLServiceManager() {
    NS_INIT_REFCNT();
}

nsXPIDLServiceManager::~nsXPIDLServiceManager() {
}

NS_IMETHODIMP nsXPIDLServiceManager::RegisterService(const nsCID & aClass, nsISupports *aService) {
    return nsServiceManager::RegisterService(aClass,aService);
}
NS_IMETHODIMP nsXPIDLServiceManager::UnregisterService(const nsCID & aClass) {
    return nsServiceManager::UnregisterService(aClass);
}
NS_IMETHODIMP nsXPIDLServiceManager::GetService(const nsCID & aClass, const nsIID & aIID, nsISupports **_retval) {
    return nsServiceManager::GetService(aClass,aIID,_retval);
}
NS_IMETHODIMP nsXPIDLServiceManager::ReleaseService(const nsCID & aClass, nsISupports *service) {
    return nsServiceManager::ReleaseService(aClass,service);
}



