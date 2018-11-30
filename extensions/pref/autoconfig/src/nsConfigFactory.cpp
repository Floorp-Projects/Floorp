/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsReadConfig.h"
#include "nsIAppStartupNotifier.h"

#define NS_READCONFIG_CID                            \
  {                                                  \
    0xba5bc4c6, 0x1dd1, 0x11b2, {                    \
      0xbb, 0x89, 0xb8, 0x44, 0xc6, 0xec, 0x03, 0x39 \
    }                                                \
  }

#define NS_READCONFIG_CONTRACTID "@mozilla.org/readconfig;1"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsReadConfig, Init)

NS_DEFINE_NAMED_CID(NS_READCONFIG_CID);

static const mozilla::Module::CIDEntry kAutoConfigCIDs[] = {
    {&kNS_READCONFIG_CID, false, nullptr, nsReadConfigConstructor}, {nullptr}};

static const mozilla::Module::ContractIDEntry kAutoConfigContracts[] = {
    {NS_READCONFIG_CONTRACTID, &kNS_READCONFIG_CID}, {nullptr}};

static const mozilla::Module::CategoryEntry kAutoConfigCategories[] = {
    {"pref-config-startup", "ReadConfig Module", NS_READCONFIG_CONTRACTID},
    {nullptr}};

static const mozilla::Module kAutoConfigModule = {
    mozilla::Module::kVersion, kAutoConfigCIDs, kAutoConfigContracts,
    kAutoConfigCategories};

NSMODULE_DEFN(nsAutoConfigModule) = &kAutoConfigModule;
