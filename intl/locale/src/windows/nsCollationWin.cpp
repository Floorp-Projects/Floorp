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
#include "nsIWin32Locale.h"
#include <windows.h>


static NS_DEFINE_CID(kWin32LocaleFactoryCID, NS_WIN32LOCALEFACTORY_CID);
static NS_DEFINE_IID(kIWin32LocaleIID, NS_IWIN32LOCALE_IID);
static NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);

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
  mCollation = new nsCollation;
  if (mCollation == NULL) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // locale -> LCID 
  mLCID = 1033; // initialize to en-US
  if (locale != nsnull) {
    nsString aLocale;
    nsString aCategory("NSILOCALE_COLLATE");
    nsresult res = locale->GetCatagory(&aCategory, &aLocale);
    if (NS_FAILED(res)) {
      return res;
    }
  
	  nsIWin32Locale* win32Locale;
	  res = nsComponentManager::CreateInstance(kWin32LocaleFactoryCID, NULL, kIWin32LocaleIID, (void**)&win32Locale);
    if (NS_FAILED(res)) {
      return res;
    }
    res = win32Locale->GetPlatformLocale(&aLocale, (LCID *) &mLCID);
	  win32Locale->Release();
  }

  return NS_OK;
};
 

nsresult nsCollationWin::GetSortKeyLen(const nsCollationStrength strength, 
                                       const nsString& stringIn, PRUint32* outLen)
{
  // Currently, no length change by the normalization.
  // API returns number of bytes when LCMAP_SORTKEY is specified 
	*outLen = LCMapStringW(mLCID, LCMAP_SORTKEY, 
                              (LPCWSTR) stringIn.GetUnicode(), (int) stringIn.Length(), NULL, 0);

  return NS_OK;
}

nsresult nsCollationWin::CreateRawSortKey(const nsCollationStrength strength, 
                                          const nsString& stringIn, PRUint8* key, PRUint32* outLen)
{
  int byteLen;
  nsAutoString stringNormalized(stringIn);

  if (mCollation != NULL && strength == kCollationCaseInSensitive) {
    mCollation->NormalizeString(stringNormalized);
  }
  byteLen = LCMapStringW(mLCID, LCMAP_SORTKEY, 
                            (LPCWSTR) stringNormalized.GetUnicode(), (int) stringNormalized.Length(), (LPWSTR) key, *outLen);
  *outLen = (PRUint32) byteLen;

  return NS_OK;
}

