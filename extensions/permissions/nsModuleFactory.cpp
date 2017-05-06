/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIServiceManager.h"
#include "nsContentBlocker.h"
#include "nsXPIDLString.h"

// Define the constructor function for the objects
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsContentBlocker, Init)

NS_DEFINE_NAMED_CID(NS_CONTENTBLOCKER_CID);

static const mozilla::Module::CIDEntry kPermissionsCIDs[] = {
  { &kNS_CONTENTBLOCKER_CID, false, nullptr, nsContentBlockerConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kPermissionsContracts[] = {
  { NS_CONTENTBLOCKER_CONTRACTID, &kNS_CONTENTBLOCKER_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kPermissionsCategories[] = {
  { "content-policy", NS_CONTENTBLOCKER_CONTRACTID, NS_CONTENTBLOCKER_CONTRACTID },
  { nullptr }
};

static const mozilla::Module kPermissionsModule = {
  mozilla::Module::kVersion,
  kPermissionsCIDs,
  kPermissionsContracts,
  kPermissionsCategories
};

NSMODULE_DEFN(nsPermissionsModule) = &kPermissionsModule;
