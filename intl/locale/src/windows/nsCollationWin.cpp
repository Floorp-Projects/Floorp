/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsCollationWin.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsWin32Locale.h"
#include "nsCOMPtr.h"
#include "prmem.h"
#include "plstr.h"
#include <windows.h>

#undef CompareString

NS_IMPL_ISUPPORTS1(nsCollationWin, nsICollation)


nsCollationWin::nsCollationWin() 
{
  mCollation = NULL;
}

nsCollationWin::~nsCollationWin() 
{
  if (mCollation != NULL)
    delete mCollation;
}

nsresult nsCollationWin::Initialize(nsILocale* locale) 
{
  NS_ASSERTION(mCollation == NULL, "Should only be initialized once.");

  nsresult res;

  mCollation = new nsCollation;
  if (!mCollation) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // default LCID (en-US)
  mLCID = 1033;

  nsAutoString localeStr;

  // get locale string, use app default if no locale specified
  if (!locale) {
    nsCOMPtr<nsILocaleService> localeService = 
             do_GetService(NS_LOCALESERVICE_CONTRACTID);
    if (localeService) {
      nsCOMPtr<nsILocale> appLocale;
      res = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
      if (NS_SUCCEEDED(res)) {
        res = appLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE"), 
                                     localeStr);
      }
    }
  }
  else {
    res = locale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE"), 
                              localeStr);
  }

  // Get LCID and charset name from locale, if available
  LCID lcid;
  res = nsWin32Locale::GetPlatformLocale(localeStr, &lcid);
  if (NS_SUCCEEDED(res)) {
    mLCID = lcid;
  }

  nsCOMPtr <nsIPlatformCharset> platformCharset = 
      do_GetService(NS_PLATFORMCHARSET_CONTRACTID);
  if (platformCharset) {
    nsCAutoString mappedCharset;
    res = platformCharset->GetDefaultCharsetForLocale(localeStr, mappedCharset);
    if (NS_SUCCEEDED(res)) {
      mCollation->SetCharset(mappedCharset.get());
    }
  }

  return NS_OK;
}


NS_IMETHODIMP nsCollationWin::CompareString(PRInt32 strength, 
                                            const nsAString & string1, 
                                            const nsAString & string2, 
                                            PRInt32 *result)
{
  int retval;
  nsresult res;
  DWORD dwMapFlags = 0;

  if (strength == kCollationCaseInSensitive)
    dwMapFlags |= NORM_IGNORECASE;

  retval = ::CompareStringW(mLCID, 
                            dwMapFlags,
                            (LPCWSTR) PromiseFlatString(string1).get(), 
                            -1,
                            (LPCWSTR) PromiseFlatString(string2).get(), 
                            -1);
  if (retval) {
    res = NS_OK;
    *result = retval - 2;
  } else {
    res = NS_ERROR_FAILURE;
  }

  return res;
}
 

nsresult nsCollationWin::AllocateRawSortKey(PRInt32 strength, 
                                            const nsAString& stringIn, PRUint8** key, PRUint32* outLen)
{
  int byteLen;
  void *buffer;
  nsresult res = NS_OK;
  DWORD dwMapFlags = LCMAP_SORTKEY;

  if (strength == kCollationCaseInSensitive)
    dwMapFlags |= NORM_IGNORECASE;

  byteLen = LCMapStringW(mLCID, dwMapFlags, 
                         (LPCWSTR) PromiseFlatString(stringIn).get(),
                         -1, NULL, 0);
  buffer = PR_Malloc(byteLen);
  if (!buffer) {
    res = NS_ERROR_OUT_OF_MEMORY;
  } else {
    *key = (PRUint8 *)buffer;
    *outLen = LCMapStringW(mLCID, dwMapFlags, 
                           (LPCWSTR) PromiseFlatString(stringIn).get(),
                           -1, (LPWSTR) buffer, byteLen);
  }
  return res;
}

nsresult nsCollationWin::CompareRawSortKey(const PRUint8* key1, PRUint32 len1, 
                                           const PRUint8* key2, PRUint32 len2, 
                                           PRInt32* result)
{
  *result = PL_strcmp((const char *)key1, (const char *)key2);
  return NS_OK;
}
