/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Spellchecker Component.
 *
 * The Initial Developer of the Original Code is David Einstein.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): David Einstein <Deinst@world.std.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "mozilla/ModuleUtils.h"
#include "mozHunspell.h"
#include "mozHunspellDirProvider.h"
#include "mozSpellChecker.h"
#include "mozInlineSpellChecker.h"
#include "nsTextServicesCID.h"
#include "mozPersonalDictionary.h"
#include "mozSpellI18NManager.h"

#define NS_SPELLCHECKER_CID         \
{ /* 8227f019-afc7-461e-b030-9f185d7a0e29 */    \
0x8227F019, 0xAFC7, 0x461e,                     \
{ 0xB0, 0x30, 0x9F, 0x18, 0x5D, 0x7A, 0x0E, 0x29} }

#define MOZ_INLINESPELLCHECKER_CID         \
{ /* 9FE5D975-09BD-44aa-A01A-66402EA28657 */    \
0x9fe5d975, 0x9bd, 0x44aa,                      \
{ 0xa0, 0x1a, 0x66, 0x40, 0x2e, 0xa2, 0x86, 0x57} }

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozHunspell, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(mozHunspellDirProvider)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozSpellChecker, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(mozPersonalDictionary, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(mozSpellI18NManager)

// This special constructor for the inline spell checker asks the inline
// spell checker if we can create spell checking objects at all (ie, if there
// are any dictionaries loaded) before trying to create one. The static
// CanEnableInlineSpellChecking caches the value so this will be faster (we
// have to run this code for every edit box we create, as well as for every
// right click in those edit boxes).
static nsresult
mozInlineSpellCheckerConstructor(nsISupports *aOuter, REFNSIID aIID,
                                 void **aResult)
{
  if (! mozInlineSpellChecker::CanEnableInlineSpellChecking())
    return NS_ERROR_FAILURE;

  nsresult rv;

  *aResult = NULL;
  if (NULL != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }

  mozInlineSpellChecker* inst = new mozInlineSpellChecker();
  if (NULL == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    return rv;
  }
  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

NS_DEFINE_NAMED_CID(MOZ_HUNSPELL_CID);
NS_DEFINE_NAMED_CID(HUNSPELLDIRPROVIDER_CID);
NS_DEFINE_NAMED_CID(NS_SPELLCHECKER_CID);
NS_DEFINE_NAMED_CID(MOZ_PERSONALDICTIONARY_CID);
NS_DEFINE_NAMED_CID(MOZ_SPELLI18NMANAGER_CID);
NS_DEFINE_NAMED_CID(MOZ_INLINESPELLCHECKER_CID);

static const mozilla::Module::CIDEntry kSpellcheckCIDs[] = {
    { &kMOZ_HUNSPELL_CID, false, NULL, mozHunspellConstructor },
    { &kHUNSPELLDIRPROVIDER_CID, false, NULL, mozHunspellDirProviderConstructor },
    { &kNS_SPELLCHECKER_CID, false, NULL, mozSpellCheckerConstructor },
    { &kMOZ_PERSONALDICTIONARY_CID, false, NULL, mozPersonalDictionaryConstructor },
    { &kMOZ_SPELLI18NMANAGER_CID, false, NULL, mozSpellI18NManagerConstructor },
    { &kMOZ_INLINESPELLCHECKER_CID, false, NULL, mozInlineSpellCheckerConstructor },
    { NULL }
};

static const mozilla::Module::ContractIDEntry kSpellcheckContracts[] = {
    { MOZ_HUNSPELL_CONTRACTID, &kMOZ_HUNSPELL_CID },
    { mozHunspellDirProvider::kContractID, &kHUNSPELLDIRPROVIDER_CID },
    { NS_SPELLCHECKER_CONTRACTID, &kNS_SPELLCHECKER_CID },
    { MOZ_PERSONALDICTIONARY_CONTRACTID, &kMOZ_PERSONALDICTIONARY_CID },
    { MOZ_SPELLI18NMANAGER_CONTRACTID, &kMOZ_SPELLI18NMANAGER_CID },
    { MOZ_INLINESPELLCHECKER_CONTRACTID, &kMOZ_INLINESPELLCHECKER_CID },
    { NULL }
};

static const mozilla::Module::CategoryEntry kSpellcheckCategories[] = {
    { XPCOM_DIRECTORY_PROVIDER_CATEGORY, "spellcheck-directory-provider", mozHunspellDirProvider::kContractID },
    { NULL }
};

const mozilla::Module kSpellcheckModule = {
    mozilla::Module::kVersion,
    kSpellcheckCIDs,
    kSpellcheckContracts,
    kSpellcheckCategories
};

NSMODULE_DEFN(mozSpellCheckerModule) = &kSpellcheckModule;
