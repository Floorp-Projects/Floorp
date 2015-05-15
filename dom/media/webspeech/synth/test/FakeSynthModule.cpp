/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "nsFakeSynthServices.h"

using namespace mozilla::dom;

#define FAKESYNTHSERVICE_CID \
  {0xe7d52d9e, 0xc148, 0x47d8, {0xab, 0x2a, 0x95, 0xd7, 0xf4, 0x0e, 0xa5, 0x3d}}

#define FAKESYNTHSERVICE_CONTRACTID "@mozilla.org/fakesynth;1"

// Defines nsFakeSynthServicesConstructor
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsFakeSynthServices,
                                         nsFakeSynthServices::GetInstanceForService)

// Defines kFAKESYNTHSERVICE_CID
NS_DEFINE_NAMED_CID(FAKESYNTHSERVICE_CID);

static const mozilla::Module::CIDEntry kCIDs[] = {
  { &kFAKESYNTHSERVICE_CID, true, nullptr, nsFakeSynthServicesConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kContracts[] = {
  { FAKESYNTHSERVICE_CONTRACTID, &kFAKESYNTHSERVICE_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kCategories[] = {
  { "profile-after-change", "Fake Speech Synth", FAKESYNTHSERVICE_CONTRACTID },
  { nullptr }
};

static void
UnloadFakeSynthmodule()
{
  nsFakeSynthServices::Shutdown();
}

static const mozilla::Module kModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts,
  kCategories,
  nullptr,
  nullptr,
  UnloadFakeSynthmodule
};

NSMODULE_DEFN(fakesynth) = &kModule;
