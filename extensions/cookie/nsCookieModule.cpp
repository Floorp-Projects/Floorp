/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsPermissionManager.h"
#include "nsPopupWindowManager.h"
#include "nsICategoryManager.h"
#include "nsCookiePromptService.h"
#include "nsCookiePermission.h"
#include "nsXPIDLString.h"

// Define the constructor function for the objects
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIPermissionManager,
  nsPermissionManager::GetXPCOMSingleton)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPopupWindowManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCookiePermission)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCookiePromptService)

NS_DEFINE_NAMED_CID(NS_PERMISSIONMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_POPUPWINDOWMANAGER_CID);
NS_DEFINE_NAMED_CID(NS_COOKIEPROMPTSERVICE_CID);
NS_DEFINE_NAMED_CID(NS_COOKIEPERMISSION_CID);


static const mozilla::Module::CIDEntry kCookieCIDs[] = {
    { &kNS_PERMISSIONMANAGER_CID, false, NULL, nsIPermissionManagerConstructor },
    { &kNS_POPUPWINDOWMANAGER_CID, false, NULL, nsPopupWindowManagerConstructor },
    { &kNS_COOKIEPROMPTSERVICE_CID, false, NULL, nsCookiePromptServiceConstructor },
    { &kNS_COOKIEPERMISSION_CID, false, NULL, nsCookiePermissionConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kCookieContracts[] = {
    { NS_PERMISSIONMANAGER_CONTRACTID, &kNS_PERMISSIONMANAGER_CID },
    { NS_POPUPWINDOWMANAGER_CONTRACTID, &kNS_POPUPWINDOWMANAGER_CID },
    { NS_COOKIEPROMPTSERVICE_CONTRACTID, &kNS_COOKIEPROMPTSERVICE_CID },
    { NS_COOKIEPERMISSION_CONTRACTID, &kNS_COOKIEPERMISSION_CID },
    { NULL }
};

static const mozilla::Module kCookieModule = {
    mozilla::Module::kVersion,
    kCookieCIDs,
    kCookieContracts
};

NSMODULE_DEFN(nsCookieModule) = &kCookieModule;
