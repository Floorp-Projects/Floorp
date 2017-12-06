/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#ifdef MOZ_WEBRTC

#include "nsPicoService.h"

using namespace mozilla::dom;

#define PICOSERVICE_CID \
  {0x346c4fc8, 0x12fe, 0x459c, {0x81, 0x19, 0x9a, 0xa7, 0x73, 0x37, 0x7f, 0xf4}}

#define PICOSERVICE_CONTRACTID "@mozilla.org/synthpico;1"

// Defines nsPicoServiceConstructor
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsPicoService,
                                         nsPicoService::GetInstanceForService)

// Defines kPICOSERVICE_CID
NS_DEFINE_NAMED_CID(PICOSERVICE_CID);

static const mozilla::Module::CIDEntry kCIDs[] = {
  { &kPICOSERVICE_CID, true, nullptr, nsPicoServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kContracts[] = {
  { PICOSERVICE_CONTRACTID, &kPICOSERVICE_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kCategories[] = {
  { "profile-after-change", "Pico Speech Synth", PICOSERVICE_CONTRACTID },
  { nullptr }
};

static void
UnloadPicoModule()
{
  nsPicoService::Shutdown();
}

static const mozilla::Module kModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts,
  kCategories,
  nullptr,
  nullptr,
  UnloadPicoModule
};

NSMODULE_DEFN(synthpico) = &kModule;
#endif
