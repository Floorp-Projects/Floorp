/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIPrefLocalizedString.h"
#include "nsUnicharUtils.h"
#include "nsCRT.h"
//#define DEBUG_UNIX_COLLATION

inline void nsCollationUnix::DoSetLocale()
{
  char *locale = setlocale(LC_COLLATE, NULL);
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

nsCollationUnix::nsCollationUnix() 
{
  mCollation = NULL;
  mUseCodePointOrder = PR_FALSE;
}

nsCollationUnix::~nsCollationUnix() 
{
  if (mCollation != NULL)
    delete mCollation;
}

NS_IMPL_ISUPPORTS1(nsCollationUnix, nsICollation)

nsresult nsCollationUnix::Initialize(nsILocale* locale) 
{
#define kPlatformLocaleLength 64
  NS_ASSERTION(mCollation == NULL, "Should only be initialized once");

  nsresult res;

  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    nsCOMPtr<nsIPrefLocalizedString> prefLocalString;
    res = prefBranch->GetComplexValue("intl.collationOption",
                                      NS_GET_IID(nsIPrefLocalizedString),
                                      getter_AddRefs(prefLocalString));
    if (NS_SUCCEEDED(res) && prefLocalString) {
      nsXPIDLString prefValue;
      prefLocalString->GetData(getter_Copies(prefValue));
      mUseCodePointOrder =
        prefValue.LowerCaseEqualsLiteral("usecodepointorder");
    }
  }

  mCollation = new nsCollation;
  if (mCollation == NULL) {
    NS_ASSERTION(0, "mCollation creation failed");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // default platform locale
  mLocale.Assign('C');

  nsAutoString localeStr;
  NS_NAMED_LITERAL_STRING(aCategory, "NSILOCALE_COLLATE");

  // get locale string, use app default if no locale specified
  if (locale == nsnull) {
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

    nsCOMPtr <nsIPosixLocale> posixLocale = do_GetService(NS_POSIXLOCALE_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      res = posixLocale->GetPlatformLocale(localeStr, mLocale);
    }

    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &res);
    if (NS_SUCCEEDED(res)) {
      nsCAutoString mappedCharset;
      res = platformCharset->GetDefaultCharsetForLocale(localeStr, mappedCharset);
      if (NS_SUCCEEDED(res)) {
        mCollation->SetCharset(mappedCharset.get());
      }
    }
  }

  return NS_OK;
}


nsresult nsCollationUnix::CompareString(PRInt32 strength,
                                        const nsAString& string1,
                                        const nsAString& string2,
                                        PRInt32* result) 
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
  if (NS_SUCCEEDED(res) && str1 != NULL) {
    res = mCollation->UnicodeToChar(stringNormalized2, &str2);
    if (NS_SUCCEEDED(res) && str2 != NULL) {
      if (mUseCodePointOrder) {
        *result = strcmp(str1, str2);
      }
      else {
        DoSetLocale();
        *result = strcoll(str1, str2);
        DoRestoreLocale();
      }
      PR_Free(str2);
    }
    PR_Free(str1);
  }

  return res;
}


nsresult nsCollationUnix::AllocateRawSortKey(PRInt32 strength, 
                                             const nsAString& stringIn,
                                             PRUint8** key, PRUint32* outLen)
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
  if (NS_SUCCEEDED(res) && str != NULL) {
    if (mUseCodePointOrder) {
      *key = (PRUint8 *)str;
      *outLen = strlen(str) + 1;
    } else {
      DoSetLocale();
      // call strxfrm to generate a key 
      size_t len = strxfrm(nsnull, str, 0) + 1;
      void *buffer = PR_Malloc(len);
      if (!buffer) {
        res = NS_ERROR_OUT_OF_MEMORY;
      } else if (strxfrm((char *)buffer, str, len) >= len) {
        PR_Free(buffer);
        res = NS_ERROR_FAILURE;
      } else {
        *key = (PRUint8 *)buffer;
        *outLen = len;
      }
      DoRestoreLocale();
      PR_Free(str);
    }
  }

  return res;
}

nsresult nsCollationUnix::CompareRawSortKey(const PRUint8* key1, PRUint32 len1, 
                                            const PRUint8* key2, PRUint32 len2, 
                                            PRInt32* result)
{
  *result = PL_strcmp((const char *)key1, (const char *)key2);
  return NS_OK;
}
