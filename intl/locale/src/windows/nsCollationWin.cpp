/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */


#include "nsCollationWin.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsIWin32Locale.h"
#include "nsCOMPtr.h"
#include "prmem.h"
#include "plstr.h"
#include <windows.h>


NS_IMPL_ISUPPORTS1(nsCollationWin, nsICollation);


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
  mCharset.AssignWithConversion("ISO-8859-1");
  
  // default LCID (en-US)
  mLCID = 1033;

  PRUnichar *aLocaleUnichar = NULL;
  nsString aCategory; aCategory.AssignWithConversion("NSILOCALE_COLLATE");

  // get locale string, use app default if no locale specified
  if (locale == nsnull) {
    nsCOMPtr<nsILocaleService> localeService = 
             do_GetService(NS_LOCALESERVICE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      nsILocale *appLocale;
      res = localeService->GetApplicationLocale(&appLocale);
      if (NS_SUCCEEDED(res)) {
        res = appLocale->GetCategory(aCategory.get(), &aLocaleUnichar);
        appLocale->Release();
      }
    }
  }
  else {
    res = locale->GetCategory(aCategory.get(), &aLocaleUnichar);
  }

  // Get LCID and charset name from locale, if available
  if (NS_SUCCEEDED(res)) {
    nsString aLocale;
    aLocale.Assign(aLocaleUnichar);
    if (NULL != aLocaleUnichar) {
      nsMemory::Free(aLocaleUnichar);
    }

    nsCOMPtr <nsIWin32Locale> win32Locale = do_GetService(NS_WIN32LOCALE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      LCID lcid;
  	  res = win32Locale->GetPlatformLocale(&aLocale, &lcid);
      if (NS_SUCCEEDED(res)) {
        mLCID = lcid;
      }
    }

    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      PRUnichar* mappedCharset = NULL;
      res = platformCharset->GetDefaultCharsetForLocale(aLocale.get(), &mappedCharset);
      if (NS_SUCCEEDED(res) && mappedCharset) {
        mCharset.Assign(mappedCharset);
        nsMemory::Free(mappedCharset);
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
                                (LPCWSTR) stringIn.get(), (int) stringIn.Length(), NULL, 0);
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
  nsAutoString stringNormalized; stringNormalized.Assign(stringIn);

  if (mCollation != NULL && strength == kCollationCaseInSensitive) {
    mCollation->NormalizeString(stringNormalized);
  }

  if (mW_API) {
    byteLen = LCMapStringW(mLCID, LCMAP_SORTKEY, 
                          (LPCWSTR) stringNormalized.get(), (int) stringNormalized.Length(), (LPWSTR) key, *outLen);
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
