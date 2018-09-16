/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/ModuleUtils.h"
#include "mozHunspell.h"
#include "mozPersonalDictionary.h"
#include "nsIFile.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozHunspell, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozPersonalDictionary, Init)

NS_DEFINE_NAMED_CID(MOZ_HUNSPELL_CID);
NS_DEFINE_NAMED_CID(MOZ_PERSONALDICTIONARY_CID);

static const mozilla::Module::CIDEntry kSpellcheckCIDs[] = {
    { &kMOZ_HUNSPELL_CID, false, nullptr, mozHunspellConstructor },
    { &kMOZ_PERSONALDICTIONARY_CID, false, nullptr, mozPersonalDictionaryConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kSpellcheckContracts[] = {
    { MOZ_HUNSPELL_CONTRACTID, &kMOZ_HUNSPELL_CID },
    { MOZ_PERSONALDICTIONARY_CONTRACTID, &kMOZ_PERSONALDICTIONARY_CID },
    { nullptr }
};

const mozilla::Module kSpellcheckModule = {
    mozilla::Module::kVersion,
    kSpellcheckCIDs,
    kSpellcheckContracts,
    nullptr
};

NSMODULE_DEFN(mozSpellCheckerModule) = &kSpellcheckModule;
