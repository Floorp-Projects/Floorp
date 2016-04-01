/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "OSXSpeechSynthesizerService.h"

using namespace mozilla::dom;

#define OSXSPEECHSYNTHESIZERSERVICE_CID \
  {0x914e73b4, 0x6337, 0x4bef, {0x97, 0xf3, 0x4d, 0x06, 0x9e, 0x05, 0x3a, 0x12}}

#define OSXSPEECHSYNTHESIZERSERVICE_CONTRACTID "@mozilla.org/synthsystem;1"

// Defines OSXSpeechSynthesizerServiceConstructor
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(OSXSpeechSynthesizerService,
                                         OSXSpeechSynthesizerService::GetInstanceForService)

// Defines kOSXSERVICE_CID
NS_DEFINE_NAMED_CID(OSXSPEECHSYNTHESIZERSERVICE_CID);

static const mozilla::Module::CIDEntry kCIDs[] = {
  { &kOSXSPEECHSYNTHESIZERSERVICE_CID, true, nullptr, OSXSpeechSynthesizerServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kContracts[] = {
  { OSXSPEECHSYNTHESIZERSERVICE_CONTRACTID, &kOSXSPEECHSYNTHESIZERSERVICE_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kCategories[] = {
  { "speech-synth-started", "OSX Speech Synth", OSXSPEECHSYNTHESIZERSERVICE_CONTRACTID },
  { nullptr }
};

static void
UnloadOSXSpeechSynthesizerModule()
{
  OSXSpeechSynthesizerService::Shutdown();
}

static const mozilla::Module kModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts,
  kCategories,
  nullptr,
  nullptr,
  UnloadOSXSpeechSynthesizerModule
};

NSMODULE_DEFN(osxsynth) = &kModule;
