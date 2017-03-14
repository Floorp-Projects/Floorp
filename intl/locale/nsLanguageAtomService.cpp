/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLanguageAtomService.h"
#include "nsUConvPropertySearch.h"
#include "nsUnicharUtils.h"
#include "nsIAtom.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Services.h"
#include "mozilla/intl/LocaleService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/EncodingUtils.h"

using namespace mozilla;
using mozilla::intl::LocaleService;

static constexpr nsUConvProp kLangGroups[] = {
#include "langGroups.properties.h"
};

NS_IMPL_ISUPPORTS(nsLanguageAtomService, nsILanguageAtomService)

nsLanguageAtomService::nsLanguageAtomService()
{
}

nsIAtom*
nsLanguageAtomService::LookupLanguage(const nsACString &aLanguage,
                                      nsresult *aError)
{
  nsAutoCString lowered(aLanguage);
  ToLowerCase(lowered);

  nsCOMPtr<nsIAtom> lang = NS_Atomize(lowered);
  return GetLanguageGroup(lang, aError);
}

already_AddRefed<nsIAtom>
nsLanguageAtomService::LookupCharSet(const nsACString& aCharSet)
{
  nsAutoCString group;
  mozilla::dom::EncodingUtils::LangGroupForEncoding(aCharSet, group);
  return NS_Atomize(group);
}

nsIAtom*
nsLanguageAtomService::GetLocaleLanguage()
{
  do {
    if (!mLocaleLanguage) {
      nsAutoCString locale;
      LocaleService::GetInstance()->GetAppLocaleAsLangTag(locale);

      ToLowerCase(locale); // use lowercase for all language atoms
      mLocaleLanguage = NS_Atomize(locale);
    }
  } while (0);

  return mLocaleLanguage;
}

nsIAtom*
nsLanguageAtomService::GetLanguageGroup(nsIAtom *aLanguage,
                                        nsresult *aError)
{
  nsIAtom *retVal;
  nsresult res = NS_OK;

  retVal = mLangToGroup.GetWeak(aLanguage);

  if (!retVal) {
    nsAutoCString langStr;
    aLanguage->ToUTF8String(langStr);

    nsAutoCString langGroupStr;
    res = nsUConvPropertySearch::SearchPropertyValue(kLangGroups,
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

    // The hashtable will keep an owning reference to the atom
    mLangToGroup.Put(aLanguage, langGroup);
    retVal = langGroup.get();
  }

  if (aError) {
    *aError = res;
  }

  return retVal;
}
