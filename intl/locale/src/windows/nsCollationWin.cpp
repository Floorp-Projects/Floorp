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

  OSVERSIONINFO os;
  os.dwOSVersionInfoSize = sizeof(os);
  ::GetVersionEx(&os);
  if (VER_PLATFORM_WIN32_NT == os.dwPlatformId && os.dwMajorVersion >= 4) {
    mW_API = PR_TRUE;
  }
  else {
    mW_API = PR_FALSE;
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
  nsCOMPtr <nsIWin32Locale> win32Locale = 
      do_GetService(NS_WIN32LOCALE_CONTRACTID);
  if (win32Locale) {
    LCID lcid;
    res = win32Locale->GetPlatformLocale(localeStr, &lcid);
    if (NS_SUCCEEDED(res)) {
      mLCID = lcid;
    }
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


nsresult nsCollationWin::CompareString(PRInt32 strength,
                                       const nsAString& string1, const nsAString& string2, PRInt32* result)
{
  int retval;
  nsresult res;
  DWORD dwMapFlags = 0;

  if (strength == kCollationCaseInSensitive)
    dwMapFlags |= NORM_IGNORECASE;

  if (mW_API) {
    retval = CompareStringW(mLCID, dwMapFlags,
                            (LPCWSTR) PromiseFlatString(string1).get(), -1,
                            (LPCWSTR) PromiseFlatString(string2).get(), -1);
    if (retval) {
      res = NS_OK;
      *result = retval - 2;
    } else {
      res = NS_ERROR_FAILURE;
    }
  } else {
    char *Cstr1 = nsnull, *Cstr2 = nsnull;
    res = mCollation->UnicodeToChar(string1, &Cstr1);
    if (NS_SUCCEEDED(res) && Cstr1 != nsnull) {
      res = mCollation->UnicodeToChar(string2, &Cstr2);
      if (NS_SUCCEEDED(res) && Cstr2 != nsnull) {
        retval = CompareStringA(mLCID, dwMapFlags, Cstr1, -1, Cstr2, -1);
        if (retval)
          *result = retval - 2;
        else
          res = NS_ERROR_FAILURE;
        PR_Free(Cstr2);
      }
      PR_Free(Cstr1);
    }
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

  if (mW_API) {
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
  }
  else {
    char *Cstr = nsnull;
    res = mCollation->UnicodeToChar(stringIn, &Cstr);
    if (NS_SUCCEEDED(res) && Cstr != nsnull) {
      byteLen = LCMapStringA(mLCID, dwMapFlags, Cstr, -1, NULL, 0);
      buffer = PR_Malloc(byteLen);
      if (!buffer) {
        res = NS_ERROR_OUT_OF_MEMORY;
      } else {
        *key = (PRUint8 *)buffer;
        *outLen = LCMapStringA(mLCID, dwMapFlags, Cstr, -1, (char *) buffer, byteLen);
      }
      PR_Free(Cstr);
    }
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
