/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.	Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsCookie.h"
#include "nsPermission.h"
#include "nsCookieManager.h"
#include "nsCookieService.h"
#include "nsImgManager.h"
#include "nsPermissionManager.h"
#include "nsCookieHTTPNotify.h"
#include "nsICategoryManager.h"
#include "nsXPIDLString.h"

// Define the constructor function for the objects
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCookie)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPermission)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookieManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookieService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsImgManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPermissionManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookieHTTPNotify, Init)

static NS_METHOD
RegisterContentPolicy(nsIComponentManager *aCompMgr, nsIFile *aPath,
                      const char *registryLocation, const char *componentType,
                      const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsXPIDLCString previous;
    return catman->AddCategoryEntry("content-policy",
                                    NS_IMGMANAGER_CONTRACTID,
                                    NS_IMGMANAGER_CONTRACTID,
                                    PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_METHOD
UnregisterContentPolicy(nsIComponentManager *aCompMgr, nsIFile *aPath,
                        const char *registryLocation,
                        const nsModuleComponentInfo *info)
{
    nsresult rv;
    nsCOMPtr<nsICategoryManager> catman =
        do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    return catman->DeleteCategoryEntry("content-policy",
                                       NS_IMGMANAGER_CONTRACTID,
                                       PR_TRUE);
}


// The list of components we register
static nsModuleComponentInfo components[] = {
    { "Cookie",
      NS_COOKIE_CID,
      NS_COOKIE_CONTRACTID,
      nsCookieConstructor
    },
    { "Permission",
      NS_PERMISSION_CID,
      NS_PERMISSION_CONTRACTID,
      nsPermissionConstructor
    },
    { "CookieManager",
      NS_COOKIEMANAGER_CID,
      NS_COOKIEMANAGER_CONTRACTID,
      nsCookieManagerConstructor
    },
    { "CookieService",
      NS_COOKIESERVICE_CID,
      NS_COOKIESERVICE_CONTRACTID,
      nsCookieServiceConstructor
    },
    { "ImgManager",
      NS_IMGMANAGER_CID,
      NS_IMGMANAGER_CONTRACTID,
      nsImgManagerConstructor,
      RegisterContentPolicy, UnregisterContentPolicy
    },
    { "PermissionManager",
      NS_PERMISSIONMANAGER_CID,
      NS_PERMISSIONMANAGER_CONTRACTID,
      nsPermissionManagerConstructor
    },
    { NS_COOKIEHTTPNOTIFY_CLASSNAME,
      NS_COOKIEHTTPNOTIFY_CID,
      NS_COOKIEHTTPNOTIFY_CONTRACTID,
      nsCookieHTTPNotifyConstructor,
      nsCookieHTTPNotify::RegisterProc,
      nsCookieHTTPNotify::UnregisterProc
    },
};

NS_IMPL_NSGETMODULE(nsCookieModule, components)
