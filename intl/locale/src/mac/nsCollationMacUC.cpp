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

#if TARGET_CARBON
#include "nsCollationMacUC.h"
#include "nsILocaleService.h"
#include "nsIServiceManager.h"
#include "prmem.h"


////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsCollationMacUC, nsICollation)


nsCollationMacUC::nsCollationMacUC() 
  : mInit(PR_FALSE)
  , mHasCollator(PR_FALSE)
  , mBuffer(nsnull)
  , mBufferLen(1)
{
}

nsCollationMacUC::~nsCollationMacUC() 
{
  if (mHasCollator) 
  {
    OSStatus err = ::UCDisposeCollator(&mCollator);
    mHasCollator = PR_FALSE;
    NS_ASSERTION((err == noErr), "UCDisposeCollator failed");
  }
  PR_FREEIF(mBuffer);
}

nsresult nsCollationMacUC::StrengthToOptions(
  const PRInt32 aStrength,
  UCCollateOptions* aOptions)
{
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ENSURE_TRUE((aStrength < 4), NS_ERROR_FAILURE);
  // set our default collation options
  UCCollateOptions options = kUCCollateStandardOptions | kUCCollatePunctuationSignificantMask;
  if (aStrength & kCollationCaseInsensitiveAscii)
    options |= kUCCollateCaseInsensitiveMask;
  if (aStrength & kCollationAccentInsenstive)
    options |= kUCCollateDiacritInsensitiveMask;
  *aOptions = options;
  return NS_OK;
}

nsresult nsCollationMacUC::ConvertLocale(
  nsILocale* aNSLocale, LocaleRef* aMacLocale) 
{
  NS_ENSURE_ARG_POINTER(aNSLocale);
  NS_ENSURE_ARG_POINTER(aMacLocale);

  nsAutoString localeString;
  nsresult res = aNSLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE"), localeString);
  NS_ENSURE_TRUE(res == noErr && !localeString.IsEmpty(), NS_ERROR_FAILURE);
  NS_LossyConvertUTF16toASCII tmp(localeString);
  tmp.ReplaceChar('-', '_');
  OSStatus err;
  err = ::LocaleRefFromLocaleString( tmp.get(), aMacLocale);
  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult nsCollationMacUC::EnsureCollator(
  const PRInt32 newStrength) 
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  if (mHasCollator && (mLastStrength == newStrength))
    return NS_OK;

  OSStatus err;
  if (mHasCollator) 
  {
    err = ::UCDisposeCollator( &mCollator );
    mHasCollator = PR_FALSE;
    NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  }

  UCCollateOptions newOptions;
  nsresult res = StrengthToOptions(newStrength, &newOptions);
  NS_ENSURE_SUCCESS(res, res);
  
  LocaleOperationVariant opVariant = 0; // default variant for now
  err = ::UCCreateCollator(mLocale, opVariant, newOptions, &mCollator);
  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  mHasCollator = PR_TRUE;

  mLastStrength = newStrength;
  return NS_OK;
}

NS_IMETHODIMP nsCollationMacUC::Initialize(
  nsILocale* locale) 
{
  NS_ENSURE_TRUE((!mInit), NS_ERROR_ALREADY_INITIALIZED);
  nsCOMPtr<nsILocale> appLocale;

  nsresult res;
  if (locale == nsnull) 
  {
    nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &res);
    NS_ENSURE_SUCCESS(res, res);
    res = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
    NS_ENSURE_SUCCESS(res, res);
    locale = appLocale;
  }

  res = ConvertLocale(locale, &mLocale);
  NS_ENSURE_SUCCESS(res, res);

  mInit = PR_TRUE;
  return NS_OK;
};
 

NS_IMETHODIMP nsCollationMacUC::AllocateRawSortKey(
  PRInt32 strength,
  const nsAString& stringIn,
  PRUint8** key,
  PRUint32* outLen)
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(key);
  NS_ENSURE_ARG_POINTER(outLen);

  nsresult res = EnsureCollator(strength);
  NS_ENSURE_SUCCESS(res, res);

  PRUint32 stringInLen = stringIn.Length();
  PRUint32 maxKeyLen = (1 + stringInLen) * kCollationValueSizeFactor * sizeof(UCCollationValue);
  if (maxKeyLen > mBufferLen) {
    PRUint32 newBufferLen = mBufferLen;
    do newBufferLen *= 2;
    while (maxKeyLen > newBufferLen);
    void *newBuffer = PR_Malloc(newBufferLen);
    if (!newBuffer)
      return NS_ERROR_OUT_OF_MEMORY;

    PR_FREEIF(mBuffer);
    mBuffer = newBuffer;
    mBufferLen = newBufferLen;
  }

  ItemCount actual;
  OSStatus err = ::UCGetCollationKey(mCollator, (const UniChar*) PromiseFlatString(stringIn).get(),
                                     (UniCharCount) stringInLen,
                                     (ItemCount) (mBufferLen / sizeof(UCCollationValue)),
                                     &actual, (UCCollationValue *)key);
  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);

  PRUint32 keyLength = actual * sizeof(UCCollationValue);
  void *newKey = PR_Malloc(keyLength);
  if (!newKey)
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy(newKey, mBuffer, keyLength);
  *key = (PRUint8 *)newKey;
  *outLen = keyLength;

  return NS_OK;
}

    
NS_IMETHODIMP nsCollationMacUC::CompareString(
  PRInt32 strength, 
  const nsAString& string1, 
  const nsAString& string2, 
  PRInt32* result) 
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(result);
  *result = 0;

  nsresult res = EnsureCollator(strength);
  NS_ENSURE_SUCCESS(res, res);
  *result = 0;

  OSStatus err;
  err = ::UCCompareText(mCollator, 
                        (const UniChar *) PromiseFlatString(string1).get(), (UniCharCount) string1.Length(),
                        (const UniChar *) PromiseFlatString(string2).get(), (UniCharCount) string2.Length(),
                        NULL, result);

  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  return NS_OK;
}


NS_IMETHODIMP nsCollationMacUC::CompareRawSortKey(
  const PRUint8* key1, PRUint32 len1, 
  const PRUint8* key2, PRUint32 len2, 
  PRInt32* result)
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(key1);
  NS_ENSURE_ARG_POINTER(key2);
  NS_ENSURE_ARG_POINTER(result);
  *result = 0;
  
  OSStatus err;
  err = ::UCCompareCollationKeys((const UCCollationValue*) key1, (ItemCount) len1,
                                 (const UCCollationValue*) key2, (ItemCount) len2,
                                 NULL, result);

  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  return NS_OK;
}


#endif
