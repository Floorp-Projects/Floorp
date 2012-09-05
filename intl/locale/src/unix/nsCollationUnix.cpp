/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include  <locale.h>
#include "prmem.h"
#include "nsCollationUnix.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsPosixLocale.h"
#include "nsCOMPtr.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
//#define DEBUG_UNIX_COLLATION

inline void nsCollationUnix::DoSetLocale()
{
  char *locale = setlocale(LC_COLLATE, nullptr);
  mSavedLocale.Assign(locale ? locale : "");
  if (!mSavedLocale.EqualsIgnoreCase(mLocale.get())) {
    (void) setlocale(LC_COLLATE, PromiseFlatCString(Substring(mLocale,0,MAX_LOCALE_LEN)).get());
  }
}

inline void nsCollationUnix::DoRestoreLocale()
{
  if (!mSavedLocale.EqualsIgnoreCase(mLocale.get())) { 
    (void) setlocale(LC_COLLATE, PromiseFlatCString(Substring(mSavedLocale,0,MAX_LOCALE_LEN)).get());
  }
}

nsCollationUnix::nsCollationUnix() : mCollation(nullptr)
{
}

nsCollationUnix::~nsCollationUnix() 
{
  if (mCollation)
    delete mCollation;
}

NS_IMPL_ISUPPORTS1(nsCollationUnix, nsICollation)

nsresult nsCollationUnix::Initialize(nsILocale* locale) 
{
#define kPlatformLocaleLength 64
  NS_ASSERTION(!mCollation, "Should only be initialized once");

  nsresult res;

  mCollation = new nsCollation;

  // default platform locale
  mLocale.Assign('C');

  nsAutoString localeStr;
  NS_NAMED_LITERAL_STRING(aCategory, "NSILOCALE_COLLATE##PLATFORM");

  // get locale string, use app default if no locale specified
  if (locale == nullptr) {
    nsCOMPtr<nsILocaleService> localeService = 
             do_GetService(NS_LOCALESERVICE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      nsCOMPtr<nsILocale> appLocale;
      res = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
      if (NS_SUCCEEDED(res)) {
        res = appLocale->GetCategory(aCategory, localeStr);
        NS_ASSERTION(NS_SUCCEEDED(res), "failed to get app locale info");
      }
    }
  }
  else {
    res = locale->GetCategory(aCategory, localeStr);
    NS_ASSERTION(NS_SUCCEEDED(res), "failed to get locale info");
  }

  // Get platform locale and charset name from locale, if available
  if (NS_SUCCEEDED(res)) {
    // keep the same behavior as 4.x as well as avoiding Linux collation key problem
    if (localeStr.LowerCaseEqualsLiteral("en_us")) { // note: locale is in platform format
      localeStr.AssignLiteral("C");
    }

    nsPosixLocale::GetPlatformLocale(localeStr, mLocale);

    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      nsAutoCString mappedCharset;
      res = platformCharset->GetDefaultCharsetForLocale(localeStr, mappedCharset);
      if (NS_SUCCEEDED(res)) {
        mCollation->SetCharset(mappedCharset.get());
      }
    }
  }

  return NS_OK;
}


nsresult nsCollationUnix::CompareString(int32_t strength,
                                        const nsAString& string1,
                                        const nsAString& string2,
                                        int32_t* result) 
{
  nsresult res = NS_OK;

  nsAutoString stringNormalized1, stringNormalized2;
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(string1, stringNormalized1);
    if (NS_FAILED(res)) {
      return res;
    }
    res = mCollation->NormalizeString(string2, stringNormalized2);
    if (NS_FAILED(res)) {
      return res;
    }
  } else {
    stringNormalized1 = string1;
    stringNormalized2 = string2;
  }

  // convert unicode to charset
  char *str1, *str2;

  res = mCollation->UnicodeToChar(stringNormalized1, &str1);
  if (NS_SUCCEEDED(res) && str1) {
    res = mCollation->UnicodeToChar(stringNormalized2, &str2);
    if (NS_SUCCEEDED(res) && str2) {
      DoSetLocale();
      *result = strcoll(str1, str2);
      DoRestoreLocale();
      PR_Free(str2);
    }
    PR_Free(str1);
  }

  return res;
}


nsresult nsCollationUnix::AllocateRawSortKey(int32_t strength, 
                                             const nsAString& stringIn,
                                             uint8_t** key, uint32_t* outLen)
{
  nsresult res = NS_OK;

  nsAutoString stringNormalized;
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringIn, stringNormalized);
    if (NS_FAILED(res))
      return res;
  } else {
    stringNormalized = stringIn;
  }
  // convert unicode to charset
  char *str;

  res = mCollation->UnicodeToChar(stringNormalized, &str);
  if (NS_SUCCEEDED(res) && str) {
    DoSetLocale();
    // call strxfrm to generate a key 
    size_t len = strxfrm(nullptr, str, 0) + 1;
    void *buffer = PR_Malloc(len);
    if (!buffer) {
      res = NS_ERROR_OUT_OF_MEMORY;
    } else if (strxfrm((char *)buffer, str, len) >= len) {
      PR_Free(buffer);
      res = NS_ERROR_FAILURE;
    } else {
      *key = (uint8_t *)buffer;
      *outLen = len;
    }
    DoRestoreLocale();
    PR_Free(str);
  }

  return res;
}

nsresult nsCollationUnix::CompareRawSortKey(const uint8_t* key1, uint32_t len1, 
                                            const uint8_t* key2, uint32_t len2, 
                                            int32_t* result)
{
  *result = PL_strcmp((const char *)key1, (const char *)key2);
  return NS_OK;
}
