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

#include  <locale.h>
#include "prmem.h"
#include "nsCollationUnix.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsIPosixLocale.h"
#include "nsCOMPtr.h"


static NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);
static NS_DEFINE_CID(kPosixLocaleFactoryCID, NS_POSIXLOCALEFACTORY_CID);
static NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);

NS_IMPL_ISUPPORTS(nsCollationUnix, kICollationIID);


nsCollationUnix::nsCollationUnix() 
{
  NS_INIT_REFCNT(); 
  mCollation = NULL;
}

nsCollationUnix::~nsCollationUnix() 
{
  if (mCollation != NULL)
    delete mCollation;
}

nsresult nsCollationUnix::Initialize(nsILocale* locale) 
{
#define kPlatformLocaleLength 64
  NS_ASSERTION(mCollation == NULL, "Should only be initialized once");

  nsresult res;

  mCollation = new nsCollation;
  if (mCollation == NULL) {
    NS_ASSERTION(0, "mCollation creation failed");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // default local charset name
  mCharset.SetString("ISO-8859-1");

  // default platform locale
  mLocale.SetString("C");

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

  // Get platform locale and charset name from locale, if available
  if (NS_SUCCEEDED(res)) {
    nsString aLocale;
    aLocale.SetString(aLocaleUnichar);
    if (NULL != aLocaleUnichar) {
      nsAllocator::Free(aLocaleUnichar);
    }

    // keep the same behavior as 4.x as well as avoiding Linux collation key problem
    if (aLocale.EqualsIgnoreCase("en-US")) {
      aLocale.SetString("C");
    }

    nsCOMPtr <nsIPosixLocale> posixLocale;
    res = nsComponentManager::CreateInstance(kPosixLocaleFactoryCID, NULL, kIPosixLocaleIID, getter_AddRefs(posixLocale));
    if (NS_SUCCEEDED(res)) {
      char platformLocale[kPlatformLocaleLength+1];
      res = posixLocale->GetPlatformLocale(&aLocale, platformLocale, kPlatformLocaleLength+1);
      if (NS_SUCCEEDED(res)) {
        mLocale.SetString(platformLocale);
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
 

nsresult nsCollationUnix::GetSortKeyLen(const nsCollationStrength strength, 
                           const nsString& stringIn, PRUint32* outLen)
{
  nsresult res = NS_OK;

  // this may not necessary because collation key length 
  // probably will not change by this normalization
  nsAutoString stringNormalized(stringIn);
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringNormalized);
  }

  // convert unicode to charset
  char *str;

  res = mCollation->UnicodeToChar(stringNormalized, &str, mCharset);
  if (NS_SUCCEEDED(res) && str != NULL) {
    char *cstr = mLocale.ToNewCString();
    char *old_locale =  setlocale(LC_COLLATE, NULL);
    (void) setlocale(LC_COLLATE, cstr);
    // call strxfrm to calculate a key length 
    int len = strxfrm(NULL, str, 0) + 1;
    (void) setlocale(LC_COLLATE, old_locale);
    delete [] cstr;
    *outLen = (len == -1) ? 0 : (PRUint32)len;
    PR_Free(str);
  }

  return res;
}

nsresult nsCollationUnix::CreateRawSortKey(const nsCollationStrength strength, 
                           const nsString& stringIn, PRUint8* key, PRUint32* outLen)
{
  nsresult res = NS_OK;

  nsAutoString stringNormalized(stringIn);
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringNormalized);
  }
  // convert unicode to charset
  char *str;

  res = mCollation->UnicodeToChar(stringNormalized, &str, mCharset);
  if (NS_SUCCEEDED(res) && str != NULL) {
    char *cstr = mLocale.ToNewCString();
    char *old_locale =  setlocale(LC_COLLATE, NULL);
    (void) setlocale(LC_COLLATE, cstr);
    // call strxfrm to generate a key 
    int len = strxfrm((char *) key, str, strlen(str));
    (void) setlocale(LC_COLLATE, old_locale);
    delete [] cstr;
    *outLen = (len == -1) ? 0 : (PRUint32)len;
    PR_Free(str);
  }

  return res;
}

