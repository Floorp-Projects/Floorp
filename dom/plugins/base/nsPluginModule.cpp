/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsPluginHost.h"
#include "nsPluginsCID.h"

NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPluginHost, nsPluginHost::GetInst)
NS_DEFINE_NAMED_CID(NS_PLUGIN_HOST_CID);

static const mozilla::Module::CIDEntry kPluginCIDs[] = {
  { &kNS_PLUGIN_HOST_CID, false, nullptr, nsPluginHostConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kPluginContracts[] = {
  { MOZ_PLUGIN_HOST_CONTRACTID, &kNS_PLUGIN_HOST_CID },
  { nullptr }
};

static const mozilla::Module kPluginModule = {
  mozilla::Module::kVersion,
  kPluginCIDs,
  kPluginContracts
};

NSMODULE_DEFN(nsPluginModule) = &kPluginModule;
