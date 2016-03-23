/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "SapiService.h"

using namespace mozilla::dom;

#define SAPISERVICE_CID \
  {0x21b4a45b, 0x9806, 0x4021, {0xa7, 0x06, 0xd7, 0x68, 0xab, 0x05, 0x48, 0xf9}}

#define SAPISERVICE_CONTRACTID "@mozilla.org/synthsapi;1"

// Defines SapiServiceConstructor
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(SapiService,
                                         SapiService::GetInstanceForService)

// Defines kSAPISERVICE_CID
NS_DEFINE_NAMED_CID(SAPISERVICE_CID);

static const mozilla::Module::CIDEntry kCIDs[] = {
  { &kSAPISERVICE_CID, true, nullptr, SapiServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kContracts[] = {
  { SAPISERVICE_CONTRACTID, &kSAPISERVICE_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kCategories[] = {
  { "speech-synth-started", "Sapi Speech Synth", SAPISERVICE_CONTRACTID },
  { nullptr }
};

static void
UnloadSapiModule()
{
  SapiService::Shutdown();
}

static const mozilla::Module kModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts,
  kCategories,
  nullptr,
  nullptr,
  UnloadSapiModule
};

NSMODULE_DEFN(synthsapi) = &kModule;
