/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsLanguageAtomService.h"
#include "nsICharsetConverterManager.h"
#include "nsILocaleService.h"
#include "nsUnicharUtils.h"
#include "nsIAtom.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"

NS_IMPL_ISUPPORTS1(nsLanguageAtomService, nsILanguageAtomService)

nsLanguageAtomService::nsLanguageAtomService()
{
}

nsresult
nsLanguageAtomService::InitLangGroupTable()
{
  if (mLangGroups)
    return NS_OK;

  nsCOMPtr<nsIStringBundleService> bundleService =
    mozilla::services::GetStringBundleService();
  if (!bundleService)
    return NS_ERROR_FAILURE;

  return bundleService->CreateBundle("resource://gre/res/langGroups.properties",
                                     getter_AddRefs(mLangGroups));
}

nsIAtom*
nsLanguageAtomService::LookupLanguage(const nsACString &aLanguage,
                                      nsresult *aError)
{
  nsAutoCString lowered(aLanguage);
  ToLowerCase(lowered);

  nsCOMPtr<nsIAtom> lang = do_GetAtom(lowered);
  return GetLanguageGroup(lang, aError);
}

already_AddRefed<nsIAtom>
nsLanguageAtomService::LookupCharSet(const char *aCharSet, nsresult *aError)
{
  if (!mCharSets) {
    mCharSets = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID);
    if (!mCharSets) {
      if (aError)
        *aError = NS_ERROR_FAILURE;

      return nullptr;
    }
  }

  nsCOMPtr<nsIAtom> langGroup;
  mCharSets->GetCharsetLangGroup(aCharSet, getter_AddRefs(langGroup));
  if (!langGroup) {
    if (aError)
      *aError = NS_ERROR_FAILURE;

    return nullptr;
  }

  if (aError)
    *aError = NS_OK;

  return langGroup.forget();
}

nsIAtom*
nsLanguageAtomService::GetLocaleLanguage(nsresult *aError)
{
  nsresult res = NS_OK;

  do {
    if (!mLocaleLanguage) {
      nsCOMPtr<nsILocaleService> localeService;
      localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID);
      if (!localeService) {
        res = NS_ERROR_FAILURE;
        break;
      }

      nsCOMPtr<nsILocale> locale;
      res = localeService->GetApplicationLocale(getter_AddRefs(locale));
      if (NS_FAILED(res))
        break;

      nsAutoString loc;
      res = locale->GetCategory(NS_LITERAL_STRING(NSILOCALE_MESSAGE), loc);
      if (NS_FAILED(res))
        break;

      ToLowerCase(loc); // use lowercase for all language atoms
      mLocaleLanguage = do_GetAtom(loc);
    }
  } while (0);

  if (aError)
    *aError = res;

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
    if (!mLangGroups) {
      if (NS_FAILED(InitLangGroupTable())) {
        if (aError) {
          *aError = NS_ERROR_FAILURE;
        }
        return nullptr;
      }
    }

    nsString langStr;
    aLanguage->ToString(langStr);

    nsXPIDLString langGroupStr;
    res = mLangGroups->GetStringFromName(langStr.get(),
                                         getter_Copies(langGroupStr));
    if (NS_FAILED(res)) {
      int32_t hyphen = langStr.FindChar('-');
      if (hyphen >= 0) {
        nsAutoString truncated(langStr);
        truncated.Truncate(hyphen);
        res = mLangGroups->GetStringFromName(truncated.get(),
                                             getter_Copies(langGroupStr));
        if (NS_FAILED(res)) {
          langGroupStr.AssignLiteral("x-unicode");
        }
      } else {
        langGroupStr.AssignLiteral("x-unicode");
      }
    }

    nsCOMPtr<nsIAtom> langGroup = do_GetAtom(langGroupStr);

    // The hashtable will keep an owning reference to the atom
    mLangToGroup.Put(aLanguage, langGroup);
    retVal = langGroup.get();
  }

  if (aError) {
    *aError = res;
  }

  return retVal;
}
