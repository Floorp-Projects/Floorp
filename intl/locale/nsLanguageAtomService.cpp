/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLanguageAtomService.h"
#include "nsUConvPropertySearch.h"
#include "nsUnicharUtils.h"
#include "nsIAtom.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/intl/OSPreferences.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ServoBindings.h"

using namespace mozilla;
using mozilla::intl::OSPreferences;

static constexpr nsUConvProp kLangGroups[] = {
#include "langGroups.properties.h"
};

// static
nsLanguageAtomService*
nsLanguageAtomService::GetService()
{
  static UniquePtr<nsLanguageAtomService> gLangAtomService;
  if (!gLangAtomService) {
    gLangAtomService = MakeUnique<nsLanguageAtomService>();
    ClearOnShutdown(&gLangAtomService);
  }
  return gLangAtomService.get();
}

nsIAtom*
nsLanguageAtomService::LookupLanguage(const nsACString &aLanguage)
{
  nsAutoCString lowered(aLanguage);
  ToLowerCase(lowered);

  nsCOMPtr<nsIAtom> lang = NS_Atomize(lowered);
  return GetLanguageGroup(lang);
}

already_AddRefed<nsIAtom>
nsLanguageAtomService::LookupCharSet(const nsACString& aCharSet)
{
  nsAutoCString group;
  dom::EncodingUtils::LangGroupForEncoding(aCharSet, group);
  return NS_Atomize(group);
}

nsIAtom*
nsLanguageAtomService::GetLocaleLanguage()
{
  do {
    if (!mLocaleLanguage) {
      nsAutoCString locale;
      OSPreferences::GetInstance()->GetSystemLocale(locale);

      ToLowerCase(locale); // use lowercase for all language atoms
      mLocaleLanguage = NS_Atomize(locale);
    }
  } while (0);

  return mLocaleLanguage;
}

nsIAtom*
nsLanguageAtomService::GetLanguageGroup(nsIAtom *aLanguage, bool* aNeedsToCache)
{
  nsIAtom *retVal = mLangToGroup.GetWeak(aLanguage);

  if (!retVal) {
    if (aNeedsToCache) {
      *aNeedsToCache = true;
      return nullptr;
    }
    nsCOMPtr<nsIAtom> uncached = GetUncachedLanguageGroup(aLanguage);
    retVal = uncached.get();

    AssertIsMainThreadOrServoLangFontPrefsCacheLocked();
    // The hashtable will keep an owning reference to the atom
    mLangToGroup.Put(aLanguage, uncached);
  }

  return retVal;
}

already_AddRefed<nsIAtom>
nsLanguageAtomService::GetUncachedLanguageGroup(nsIAtom* aLanguage) const
{
  nsAutoCString langStr;
  aLanguage->ToUTF8String(langStr);

  nsAutoCString langGroupStr;
  nsresult res =
    nsUConvPropertySearch::SearchPropertyValue(kLangGroups,
                                               ArrayLength(kLangGroups),
                                               langStr, langGroupStr);
  while (NS_FAILED(res)) {
    int32_t hyphen = langStr.RFindChar('-');
    if (hyphen <= 0) {
      langGroupStr.AssignLiteral("x-unicode");
      break;
    }
    langStr.Truncate(hyphen);
    res = nsUConvPropertySearch::SearchPropertyValue(kLangGroups,
                                                     ArrayLength(kLangGroups),
                                                     langStr, langGroupStr);
  }

  nsCOMPtr<nsIAtom> langGroup = NS_Atomize(langGroupStr);

  return langGroup.forget();
}
