/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/ModuleUtils.h"
#include "mozHunspell.h"
#include "mozSpellChecker.h"
#include "mozPersonalDictionary.h"
#include "mozSpellI18NManager.h"
#include "nsIFile.h"

#define NS_SPELLCHECKER_CID         \
{ /* 8227f019-afc7-461e-b030-9f185d7a0e29 */    \
0x8227F019, 0xAFC7, 0x461e,                     \
{ 0xB0, 0x30, 0x9F, 0x18, 0x5D, 0x7A, 0x0E, 0x29} }

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozHunspell, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozSpellChecker, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozPersonalDictionary, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(mozSpellI18NManager)

NS_DEFINE_NAMED_CID(MOZ_HUNSPELL_CID);
NS_DEFINE_NAMED_CID(NS_SPELLCHECKER_CID);
NS_DEFINE_NAMED_CID(MOZ_PERSONALDICTIONARY_CID);
NS_DEFINE_NAMED_CID(MOZ_SPELLI18NMANAGER_CID);

static const mozilla::Module::CIDEntry kSpellcheckCIDs[] = {
    { &kMOZ_HUNSPELL_CID, false, nullptr, mozHunspellConstructor },
    { &kNS_SPELLCHECKER_CID, false, nullptr, mozSpellCheckerConstructor },
    { &kMOZ_PERSONALDICTIONARY_CID, false, nullptr, mozPersonalDictionaryConstructor },
    { &kMOZ_SPELLI18NMANAGER_CID, false, nullptr, mozSpellI18NManagerConstructor },
    { nullptr }
};

static const mozilla::Module::ContractIDEntry kSpellcheckContracts[] = {
    { MOZ_HUNSPELL_CONTRACTID, &kMOZ_HUNSPELL_CID },
    { NS_SPELLCHECKER_CONTRACTID, &kNS_SPELLCHECKER_CID },
    { MOZ_PERSONALDICTIONARY_CONTRACTID, &kMOZ_PERSONALDICTIONARY_CID },
    { MOZ_SPELLI18NMANAGER_CONTRACTID, &kMOZ_SPELLI18NMANAGER_CID },
    { nullptr }
};

const mozilla::Module kSpellcheckModule = {
    mozilla::Module::kVersion,
    kSpellcheckCIDs,
    kSpellcheckContracts,
    nullptr
};

NSMODULE_DEFN(mozSpellCheckerModule) = &kSpellcheckModule;
