/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 07/05/2000       IBM Corp.      Reworked file.
 */

#include "unidef.h"
#include "prmem.h"
#include "nsCollationOS2.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsLocaleCID.h"
#include "nsILocaleService.h"
#include "nsIPlatformCharset.h"
#include "nsIOS2Locale.h"
#include "nsCOMPtr.h"
#include "nsFileSpec.h" /* for nsAutoString */
#include "nsIPref.h"


static NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);
static NS_DEFINE_CID(kOS2LocaleFactoryCID, NS_OS2LOCALEFACTORY_CID);
static NS_DEFINE_IID(kIOS2LocaleIID, NS_IOS2LOCALE_IID);
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);
static NS_DEFINE_CID(kPlatformCharsetCID, NS_PLATFORMCHARSET_CID);

NS_IMPL_ISUPPORTS(nsCollationOS2, kICollationIID)

nsCollationOS2::nsCollationOS2()
{
  NS_INIT_REFCNT();
  mCollation = NULL;
}

nsCollationOS2::~nsCollationOS2()
{
   if (mCollation != NULL)
     delete mCollation;
}

nsresult nsCollationOS2::Initialize(nsILocale *locale)
{
#define kPlatformLocaleLength 64
#define MAPPEDCHARSET_BUFFER_LEN 80
  NS_ASSERTION(mCollation == NULL, "Should only be initialized once");

  nsresult res;

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

    nsCOMPtr <nsIOS2Locale> os2Locale = do_GetService(kOS2LocaleFactoryCID, &res);
    if (NS_SUCCEEDED(res)) {
      PRUnichar platformLocale[kPlatformLocaleLength+1];
      res = os2Locale->GetPlatformLocale(platformLocale, kPlatformLocaleLength+1);
      if (NS_SUCCEEDED(res)) {
        mLocale.Assign(platformLocale, kPlatformLocaleLength+1);
      }
    }

    // Create a locale object and query for the code page
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      PRUnichar mappedCharset[MAPPEDCHARSET_BUFFER_LEN] = { 0 };
      LocaleObject locObj = NULL;
      UniChar      *pmCodepage;

      int  ret = UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)mLocale.GetUnicode(), &locObj);
      if (ret != ULS_SUCCESS) 
        return NS_ERROR_FAILURE;
        
      ret = UniQueryLocaleItem(locObj, LOCI_iCodepage, &pmCodepage);
      if (ret != ULS_SUCCESS)
        return NS_ERROR_FAILURE;

      int cpLen = UniStrlen((UniChar*)pmCodepage);
      UniStrncpy((UniChar *)mappedCharset, pmCodepage, cpLen);
      UniFreeMem(pmCodepage);
      UniFreeLocaleObject(locObj);
      PRUint32  mpLen = UniStrlen(mappedCharset);

      if (NS_SUCCEEDED(ret) && mappedCharset) {
        mCharset.Assign((PRUnichar*)mappedCharset, mpLen);
      }
    }
  }

  return NS_OK;
}

 
nsresult nsCollationOS2::GetSortKeyLen(const nsCollationStrength strength, 
                                       const nsString& stringIn, PRUint32* outLen)
{ 
  // this may not necessary because collation key length 
  // probably will not change by this normalization
  nsresult res = NS_OK;
  nsString stringNormalized = stringIn;
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringNormalized);
  }

  LocaleObject locObj = NULL;
  int ret = UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)L"", &locObj);
  if (ret != ULS_SUCCESS)
    return NS_ERROR_FAILURE;
  int uLen = UniStrxfrm(locObj, NULL, stringNormalized.GetUnicode(), 0);
  
  *outLen = (uLen < 1) ? 0 : (PRUint32)uLen;
  return res;
}


nsresult nsCollationOS2::CreateRawSortKey(const nsCollationStrength strength, 
                                          const nsString& stringIn, PRUint8* key, PRUint32* outLen)
{
  nsresult res = NS_OK;

  nsString stringNormalized = stringIn;
  if (strength != kCollationCaseSensitive) {
    res = mCollation->NormalizeString(stringNormalized);
  }

  LocaleObject locObj = NULL;
  int ret = UniCreateLocaleObject(UNI_UCS_STRING_POINTER, (UniChar *)L"", &locObj);
  if (ret != ULS_SUCCESS)
    return NS_ERROR_FAILURE;

  int length = UniStrlen(stringNormalized.GetUnicode());
  int uLen = UniStrxfrm(locObj, (UniChar*)key, stringNormalized.GetUnicode(), length);
  *outLen = (uLen < 1) ? 0 : (PRUint32)uLen;
  UniFreeMem(locObj);

  return res;
}

