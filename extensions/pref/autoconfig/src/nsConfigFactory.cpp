/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsAutoConfig.h"
#include "nsReadConfig.h"
#include "nsIAppStartupNotifier.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAutoConfig, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsReadConfig, Init)

NS_DEFINE_NAMED_CID(NS_AUTOCONFIG_CID);
NS_DEFINE_NAMED_CID(NS_READCONFIG_CID);

static const mozilla::Module::CIDEntry kAutoConfigCIDs[] = {
  { &kNS_AUTOCONFIG_CID, false, nullptr, nsAutoConfigConstructor },
  { &kNS_READCONFIG_CID, false, nullptr, nsReadConfigConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kAutoConfigContracts[] = {
  { NS_AUTOCONFIG_CONTRACTID, &kNS_AUTOCONFIG_CID },
  { NS_READCONFIG_CONTRACTID, &kNS_READCONFIG_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kAutoConfigCategories[] = {
  { "pref-config-startup", "ReadConfig Module", NS_READCONFIG_CONTRACTID },
  { nullptr }
};

static const mozilla::Module kAutoConfigModule = {
  mozilla::Module::kVersion,
  kAutoConfigCIDs,
  kAutoConfigContracts,
  kAutoConfigCategories
};

NSMODULE_DEFN(nsAutoConfigModule) = &kAutoConfigModule;
