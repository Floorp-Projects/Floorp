/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLanguageAtomService.h"
#include "nsUConvPropertySearch.h"
#include "nsUnicharUtils.h"
#include "nsAtom.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Encoding.h"
#include "mozilla/intl/OSPreferences.h"
#include "mozilla/ServoBindings.h"

using namespace mozilla;
using mozilla::intl::OSPreferences;

static constexpr nsUConvProp encodingsGroups[] = {
#include "encodingsgroups.properties.h"
};

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

nsAtom*
nsLanguageAtomService::LookupLanguage(const nsACString &aLanguage)
{
  nsAutoCString lowered(aLanguage);
  ToLowerCase(lowered);

  RefPtr<nsAtom> lang = NS_Atomize(lowered);
  return GetLanguageGroup(lang);
}

already_AddRefed<nsAtom>
nsLanguageAtomService::LookupCharSet(NotNull<const Encoding*> aEncoding)
{
  nsAutoCString charset;
  aEncoding->Name(charset);
  nsAutoCString group;
  if (NS_FAILED(nsUConvPropertySearch::SearchPropertyValue(
      encodingsGroups, ArrayLength(encodingsGroups), charset, group))) {
    return RefPtr<nsAtom>(nsGkAtoms::Unicode).forget();
  }
  return NS_Atomize(group);
}

nsAtom*
nsLanguageAtomService::GetLocaleLanguage()
{
  do {
    if (!mLocaleLanguage) {
      AutoTArray<nsCString, 10> regionalPrefsLocales;
      if (OSPreferences::GetInstance()->GetRegionalPrefsLocales(
                                          regionalPrefsLocales)) {
        // use lowercase for all language atoms
        ToLowerCase(regionalPrefsLocales[0]);
        mLocaleLanguage = NS_Atomize(regionalPrefsLocales[0]);
      } else {
        nsAutoCString locale;
        OSPreferences::GetInstance()->GetSystemLocale(locale);

        ToLowerCase(locale); // use lowercase for all language atoms
        mLocaleLanguage = NS_Atomize(locale);
      }
    }
  } while (0);

  return mLocaleLanguage;
}

nsAtom*
nsLanguageAtomService::GetLanguageGroup(nsAtom *aLanguage, bool* aNeedsToCache)
{
  nsAtom *retVal = mLangToGroup.GetWeak(aLanguage);

  if (!retVal) {
    if (aNeedsToCache) {
      *aNeedsToCache = true;
      return nullptr;
    }
    RefPtr<nsAtom> uncached = GetUncachedLanguageGroup(aLanguage);
    retVal = uncached.get();

    AssertIsMainThreadOrServoLangFontPrefsCacheLocked();
    // The hashtable will keep an owning reference to the atom
    mLangToGroup.Put(aLanguage, uncached);
  }

  return retVal;
}

already_AddRefed<nsAtom>
nsLanguageAtomService::GetUncachedLanguageGroup(nsAtom* aLanguage) const
{
  nsAutoCString langStr;
  aLanguage->ToUTF8String(langStr);
  ToLowerCase(langStr);

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

  RefPtr<nsAtom> langGroup = NS_Atomize(langGroupStr);

  return langGroup.forget();
}
