/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsPermissionManager.h"
#include "nsICategoryManager.h"
#include "nsCookiePermission.h"
#include "nsString.h"

// Define the constructor function for the objects
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsIPermissionManager,
  nsPermissionManager::GetXPCOMSingleton)

NS_DEFINE_NAMED_CID(NS_PERMISSIONMANAGER_CID);


static const mozilla::Module::CIDEntry kCookieCIDs[] = {
    { &kNS_PERMISSIONMANAGER_CID, false, nullptr, nsIPermissionManagerConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kCookieContracts[] = {
    { NS_PERMISSIONMANAGER_CONTRACTID, &kNS_PERMISSIONMANAGER_CID },
    { nullptr }
};

static void CookieModuleDtor()
{
  nsCookiePermission::Shutdown();
}

static const mozilla::Module kCookieModule = {
    mozilla::Module::kVersion,
    kCookieCIDs,
    kCookieContracts,
    nullptr,
    nullptr,
    nullptr,
    CookieModuleDtor
};

NSMODULE_DEFN(nsCookieModule) = &kCookieModule;
