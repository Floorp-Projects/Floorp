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

#include  <locale.h>
#include "prmem.h"
#include "nsCollationUnix.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsIPosixLocale.h"
#include "nsCOMPtr.h"
#include "nsFileSpec.h" /* for nsAutoString */
#include "nsIPref.h"
//#define DEBUG_UNIX_COLLATION

static NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);
static NS_DEFINE_CID(kPosixLocaleFactoryCID, NS_POSIXLOCALEFACTORY_CID);
static NS_DEFINE_IID(kIPosixLocaleIID, NS_IPOSIXLOCALE_IID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);
static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);

NS_IMPL_ISUPPORTS(nsCollationUnix, kICollationIID);


inline void nsCollationUnix::DoSetLocale()
{
  char *locale = setlocale(LC_COLLATE, NULL);
  mSavedLocale.AssignWithConversion(locale ? locale : "");
  if (!mSavedLocale.EqualsIgnoreCase(mLocale)) {
    char newLocale[128];
    (void) setlocale(LC_COLLATE, mLocale.ToCString(newLocale, 128));
  }
}

inline void nsCollationUnix::DoRestoreLocale()
{
  if (!mSavedLocale.EqualsIgnoreCase(mLocale)) { 
    char oldLocale[128];
    (void) setlocale(LC_COLLATE, mSavedLocale.ToCString(oldLocale, 128));
  }
}

nsCollationUnix::nsCollationUnix() 
{
  NS_INIT_REFCNT(); 
  mCollation = NULL;
  mKeyAsCodePoint = PR_FALSE;
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

  nsCOMPtr<nsIPref> prefs = do_GetService(NS_PREF_CONTRACTID);
  if (prefs) {
    res = prefs->GetBoolPref("intl.collationKeyAsCodePoint", &mKeyAsCodePoint);
  }

  mCollation = new nsCollation;
  if (mCollation == NULL) {
    NS_ASSERTION(0, "mCollation creation failed");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // default local charset name
  mCharset.AssignWithConversion("ISO-8859-1");

  // default platform locale
  mLocale.AssignWithConversion("C");

  PRUnichar *aLocaleUnichar = NULL;
  nsString aCategory;
  aCategory.AssignWithConversion("NSILOCALE_COLLATE");

  // get locale string, use app default if no locale specified
  if (locale == nsnull) {
    NS_WITH_SERVICE(nsILocaleService, localeService, kLocaleServiceCID, &res);
    if (NS_SUCCEEDED(res)) {
      nsILocale *appLocale;
      res = localeService->GetApplicationLocale(&appLocale);
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
    aLocale = aLocaleUnichar;
    if (NULL != aLocaleUnichar) {
      nsMemory::Free(aLocaleUnichar);
    }

    // keep the same behavior as 4.x as well as avoiding Linux collation key problem
    if (aLocale.EqualsIgnoreCase("en-US")) {
      aLocale.AssignWithConversion("C");
    }

    nsCOMPtr <nsIPosixLocale> posixLocale = do_GetService(kPosixLocaleFactoryCID, &res);
    if (NS_SUCCEEDED(res)) {
      char platformLocale[kPlatformLocaleLength+1];
      res = posixLocale->GetPlatformLocale(&aLocale, platformLocale, kPlatformLocaleLength+1);
      if (NS_SUCCEEDED(res)) {
        mLocale.AssignWithConversion(platformLocale);
      }
    }

    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      PRUnichar* mappedCharset = NULL;
      res = platformCharset->GetDefaultCharsetForLocale(aLocale.GetUnicode(), &mappedCharset);
      if (NS_SUCCEEDED(res) && mappedCharset) {
        mCharset = mappedCharset;
        nsMemory::Free(mappedCharset);
      }
    }
  }

#if defined(DEBUG_UNIX_COLLATION)
  nsAutoCString tmp(mLocale);
  if (NULL != (const char *)tmp) {
    printf("nsCollationUnix::Initialize mLocale = %s\n", (const char *)tmp);
  }
  nsAutoCString tmp2(mCharset);
  if (NULL != (const char *)tmp2) {
    printf("nsCollationUnix::Initialize mCharset = %s\n", (const char *)tmp2);
  }
#endif

  return NS_OK;
};
 

nsresult nsCollationUnix::GetSortKeyLen(const nsCollationStrength strength, 
                           const nsString& stringIn, PRUint32* outLen)
{
  nsresult res = NS_OK;

  // this may not necessary because collation key length 
  // probably will not change by this normalization
  nsString stringNormalized = stringIn;
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringNormalized);
  }

  // convert unicode to charset
  char *str;

  res = mCollation->UnicodeToChar(stringNormalized, &str, mCharset);
  if (NS_SUCCEEDED(res) && str != NULL) {
    if (mKeyAsCodePoint) {
      *outLen = nsCRT::strlen(str);
    }
    else {
      DoSetLocale();
      // call strxfrm to calculate a key length 
      int len = strxfrm(NULL, str, 0) + 1;
      DoRestoreLocale();
      *outLen = (len == -1) ? 0 : (PRUint32)len;
    }
    PR_Free(str);
  }

  return res;
}

nsresult nsCollationUnix::CreateRawSortKey(const nsCollationStrength strength, 
                           const nsString& stringIn, PRUint8* key, PRUint32* outLen)
{
  nsresult res = NS_OK;

  nsString stringNormalized = stringIn;
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringNormalized);
  }
  // convert unicode to charset
  char *str;

  res = mCollation->UnicodeToChar(stringNormalized, &str, mCharset);
  if (NS_SUCCEEDED(res) && str != NULL) {
    if (mKeyAsCodePoint) {
      *outLen = nsCRT::strlen(str);
      nsCRT::memcpy(key, str, *outLen);
    }
    else {
      DoSetLocale();
      // call strxfrm to generate a key 
      int len = strxfrm((char *) key, str, strlen(str));
      DoRestoreLocale();
      *outLen = (len == -1) ? 0 : (PRUint32)len;
    }
    PR_Free(str);
  }

  return res;
}

