/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIServiceManager.h"
#include "nsImgManager.h"
#include "nsPermissionManager.h"
#include "nsPopupWindowManager.h"
#include "nsICategoryManager.h"
#include "nsCookiePromptService.h"
#include "nsCookiePermission.h"
#include "nsXPIDLString.h"

// Define the constructor function for the objects
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsImgManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPermissionManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPopupWindowManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCookiePermission, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCookiePromptService)

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
static const nsModuleComponentInfo components[] = {
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
    { "PopupWindowManager",
      NS_POPUPWINDOWMANAGER_CID,
      NS_POPUPWINDOWMANAGER_CONTRACTID,
      nsPopupWindowManagerConstructor
    },
    { "CookiePromptService",
      NS_COOKIEPROMPTSERVICE_CID,
      NS_COOKIEPROMPTSERVICE_CONTRACTID,
      nsCookiePromptServiceConstructor
    },
    { "CookiePermission",
      NS_COOKIEPERMISSION_CID,
      NS_COOKIEPERMISSION_CONTRACTID,
      nsCookiePermissionConstructor
    }
};

NS_IMPL_NSGETMODULE(nsCookieModule, components)
