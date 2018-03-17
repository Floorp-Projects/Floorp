/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "SpeechSynthesisService.h"

using namespace mozilla::dom;

#define ANDROIDSPEECHSERVICE_CID \
  {0x311b2dab, 0xf4d3, 0x4be4, {0x81, 0x23, 0x67, 0x32, 0x31, 0x3d, 0x95, 0xc2}}

#define ANDROIDSPEECHSERVICE_CONTRACTID "@mozilla.org/androidspeechsynth;1"

// Defines AndroidSpeechServiceConstructor
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(SpeechSynthesisService,
                                         SpeechSynthesisService::GetInstanceForService)

// Defines kANDROIDSPEECHSERVICE_CID
NS_DEFINE_NAMED_CID(ANDROIDSPEECHSERVICE_CID);

static const mozilla::Module::CIDEntry kCIDs[] = {
  { &kANDROIDSPEECHSERVICE_CID, true, nullptr, SpeechSynthesisServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kContracts[] = {
  { ANDROIDSPEECHSERVICE_CONTRACTID, &kANDROIDSPEECHSERVICE_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kCategories[] = {
  { "speech-synth-started", "Android Speech Synth", ANDROIDSPEECHSERVICE_CONTRACTID },
  { nullptr }
};

static const mozilla::Module kModule = {
  mozilla::Module::kVersion,
  kCIDs,
  kContracts,
  kCategories,
  nullptr,
  nullptr,
  nullptr,
};

NSMODULE_DEFN(androidspeechsynth) = &kModule;
