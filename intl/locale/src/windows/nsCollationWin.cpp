/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nsCollationWin.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsIWin32Locale.h"
#include "nsCOMPtr.h"
#include "prmem.h"
#include "plstr.h"
#include <windows.h>


static NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
static NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);
static NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID); 

NS_IMPL_ISUPPORTS(nsCollationWin, kICollationIID);


nsCollationWin::nsCollationWin() 
{
  NS_INIT_REFCNT(); 
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
  if (mCollation == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  OSVERSIONINFO os;
  os.dwOSVersionInfoSize = sizeof(os);
  ::GetVersionEx(&os);
  if (VER_PLATFORM_WIN32_NT == os.dwPlatformId &&
      os.dwMajorVersion >= 4) {
    mW_API = PR_TRUE;
  }
  else {
    mW_API = PR_FALSE;
  }

  // default charset name
  mCharset.SetString("ISO-8859-1");
  
  // default LCID (en-US)
  mLCID = 1033;

  PRUnichar *aLocaleUnichar = NULL;
  nsString aCategory("NSILOCALE_COLLATE");

  // get locale string, use app default if no locale specified
  if (locale == nsnull) {
    nsILocaleService *localeService;

    res = nsComponentManager::CreateInstance(kLocaleServiceCID, NULL, 
                                             nsILocaleService::GetIID(), (void**)&localeService);
    if (NS_SUCCEEDED(res)) {
      nsILocale *appLocale;
      res = localeService->GetApplicationLocale(&appLocale);
	    localeService->Release();
      if (NS_SUCCEEDED(res)) {
        res = appLocale->GetCategory(aCategory.GetUnicode(), &aLocaleUnichar);
        appLocale->Release();
      }
    }
  }
  else {
    res = locale->GetCategory(aCategory.GetUnicode(), &aLocaleUnichar);
  }

  // Get LCID and charset name from locale, if available
  if (NS_SUCCEEDED(res)) {
    nsString aLocale;
    aLocale.SetString(aLocaleUnichar);
    if (NULL != aLocaleUnichar) {
      nsAllocator::Free(aLocaleUnichar);
    }

    nsCOMPtr <nsIWin32Locale> win32Locale;
    res = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID, NULL, kIWin32LocaleIID, getter_AddRefs(win32Locale));
    if (NS_SUCCEEDED(res)) {
      LCID lcid;
  	  res = win32Locale->GetPlatformLocale(&aLocale, &lcid);
      if (NS_SUCCEEDED(res)) {
        mLCID = lcid;
      }
    }

    nsCOMPtr <nsIPlatformCharset> platformCharset;
    res = nsComponentManager::CreateInstance(kPlatformCharsetCID, NULL, 
                                             nsIPlatformCharset::GetIID(), getter_AddRefs(platformCharset));
    if (NS_SUCCEEDED(res)) {
      PRUnichar* mappedCharset = NULL;
      res = platformCharset->GetDefaultCharsetForLocale(aLocale.GetUnicode(), &mappedCharset);
      if (NS_SUCCEEDED(res) && mappedCharset) {
        mCharset.SetString(mappedCharset);
        nsAllocator::Free(mappedCharset);
      }
    }
  }

  return NS_OK;
};
 

nsresult nsCollationWin::GetSortKeyLen(const nsCollationStrength strength, 
                                       const nsString& stringIn, PRUint32* outLen)
{
  nsresult res = NS_OK;
  // Currently, no length change by the normalization.
  // API returns number of bytes when LCMAP_SORTKEY is specified 
  if (mW_API) {
    *outLen = LCMapStringW(mLCID, LCMAP_SORTKEY, 
                                (LPCWSTR) stringIn.GetUnicode(), (int) stringIn.Length(), NULL, 0);
  }
  else {
    char *Cstr = nsnull;
    res = mCollation->UnicodeToChar(stringIn, &Cstr, mCharset);
    if (NS_SUCCEEDED(res) && Cstr != nsnull) {
      *outLen = LCMapStringA(mLCID, LCMAP_SORTKEY, Cstr, PL_strlen(Cstr), NULL, 0);
      PR_Free(Cstr);
    }
  }

  return res;
}

nsresult nsCollationWin::CreateRawSortKey(const nsCollationStrength strength, 
                                          const nsString& stringIn, PRUint8* key, PRUint32* outLen)
{
  int byteLen;
  nsresult res = NS_OK;
  nsAutoString stringNormalized(stringIn);

  if (mCollation != NULL && strength == kCollationCaseInSensitive) {
    mCollation->NormalizeString(stringNormalized);
  }

  if (mW_API) {
    byteLen = LCMapStringW(mLCID, LCMAP_SORTKEY, 
                          (LPCWSTR) stringNormalized.GetUnicode(), (int) stringNormalized.Length(), (LPWSTR) key, *outLen);
  }
  else {
    char *Cstr = nsnull;
    res = mCollation->UnicodeToChar(stringNormalized, &Cstr, mCharset);
    if (NS_SUCCEEDED(res) && Cstr != nsnull) {
      byteLen = LCMapStringA(mLCID, LCMAP_SORTKEY, Cstr, PL_strlen(Cstr), (char *) key, (int) *outLen);
      PR_Free(Cstr);
    }
  }
  *outLen = (PRUint32) byteLen;

  return res;
}
