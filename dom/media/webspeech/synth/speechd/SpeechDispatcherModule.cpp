/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"
#include "SpeechDispatcherService.h"

using namespace mozilla::dom;

#define SPEECHDISPATCHERSERVICE_CID \
  {0x8817b1cf, 0x5ada, 0x43bf, {0xbd, 0x73, 0x60, 0x76, 0x57, 0x70, 0x3d, 0x0d}}

#define SPEECHDISPATCHERSERVICE_CONTRACTID "@mozilla.org/synthspeechdispatcher;1"

// Defines SpeechDispatcherServiceConstructor
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(SpeechDispatcherService,
                                         SpeechDispatcherService::GetInstanceForService)

// Defines kSPEECHDISPATCHERSERVICE_CID
NS_DEFINE_NAMED_CID(SPEECHDISPATCHERSERVICE_CID);

static const mozilla::Module::CIDEntry kCIDs[] = {
  { &kSPEECHDISPATCHERSERVICE_CID, true, nullptr, SpeechDispatcherServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kContracts[] = {
  { SPEECHDISPATCHERSERVICE_CONTRACTID, &kSPEECHDISPATCHERSERVICE_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kCategories[] = {
  { "profile-after-change", "SpeechDispatcher Speech Synth", SPEECHDISPATCHERSERVICE_CONTRACTID },
  { nullptr }
};

static void
UnloadSpeechDispatcherModule()
{
  SpeechDispatcherService::Shutdown();
}

static const mozilla::Module kModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts,
  kCategories,
  nullptr,
  nullptr,
  UnloadSpeechDispatcherModule
};

NSMODULE_DEFN(synthspeechdispatcher) = &kModule;
