/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if TARGET_CARBON
#include "nsCollationMacUC.h"
#include "nsILocaleService.h"
#include "nsIServiceManager.h"


////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(nsCollationMacUC, nsICollation);


nsCollationMacUC::nsCollationMacUC() 
  : mInit(PR_FALSE)
  , mHasCollator(PR_FALSE)
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
}

nsresult nsCollationMacUC::StrengthToOptions(
  const nsCollationStrength aStrength,
  UCCollateOptions* aOptions)
{
  NS_ENSURE_ARG_POINTER(aOptions);
  NS_ENSURE_TRUE((aStrength < 4), NS_ERROR_FAILURE);
  // set our default collaion option
  UCCollateOptions defaultOption = kUCCollateStandardOptions | kUCCollatePunctuationSignificantMask;
  switch (aStrength) {
    case kCollationCaseInsensitiveAscii:
      *aOptions = defaultOption 
                  | kUCCollateCaseInsensitiveMask;
      break;
    case kCollationAccentInsenstive:
      *aOptions = defaultOption 
                  | kUCCollateDiacritInsensitiveMask;
      break;
    case kCollationCaseInSensitive:
      *aOptions = defaultOption 
                  | kUCCollateCaseInsensitiveMask
                  | kUCCollateDiacritInsensitiveMask;
      break;
    case kCollationCaseSensitive:
    default:
      *aOptions =  defaultOption;
      break;
  }
  return NS_OK;
}

nsresult nsCollationMacUC::ConvertLocale(
  nsILocale* aNSLocale, LocaleRef* aMacLocale) 
{
  NS_ENSURE_ARG_POINTER(aNSLocale);
  NS_ENSURE_ARG_POINTER(aMacLocale);

  nsXPIDLString aLocaleString;
  nsresult res = aNSLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE").get(), getter_Copies(aLocaleString));
  if (res != noErr)
    return NS_ERROR_FAILURE;
  NS_ENSURE_ARG_POINTER(aLocaleString.get());
  nsCAutoString tmp;
  tmp.AssignWithConversion(aLocaleString);
  tmp.ReplaceChar('-', '_');
  OSStatus err;
  err = ::LocaleRefFromLocaleString( tmp.get(), aMacLocale);
  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult nsCollationMacUC::EnsureCollator(
  const nsCollationStrength newStrength) 
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
 

NS_IMETHODIMP nsCollationMacUC::GetSortKeyLen(
  const nsCollationStrength strength, 
  const nsAString& stringIn, 
  PRUint32* outLen)
{
  NS_ENSURE_ARG_POINTER(outLen);

  // If possible, a length is estimated by actually creating a key (and cache the result).
  // If the string is too big then return the length by the formula.

  mCachedStringLen = stringIn.Length();
  // Set the default value for an output length.
  *outLen = (1 + mCachedStringLen) * kCollationValueSizeFactor * sizeof(UCCollationValue);
    
  if (mCachedStringLen > kCacheSize)
  {
    mCachedStringLen = mCachedKeyLen = 0;  // indicate key is not cached
    return NS_OK;
  }

  nsresult res = EnsureCollator(strength);
  NS_ENSURE_SUCCESS(res, res);

  ItemCount actual;
  memcpy((void *) mCachedString, (void *) PromiseFlatString(stringIn).get(), 
         mCachedStringLen * sizeof(UniChar));
  OSErr err = ::UCGetCollationKey(mCollator, (const UniChar *) mCachedString,
                           (ItemCount) mCachedStringLen,
                           kCacheSize * kCollationValueSizeFactor,
                           &actual, (UCCollationValue *) mCachedKey);
  if (err == noErr)
    *outLen = mCachedKeyLen = actual * sizeof(UCCollationValue);
  else
    mCachedStringLen = mCachedKeyLen = 0;

  return NS_OK;
}

NS_IMETHODIMP nsCollationMacUC::CreateRawSortKey(
  const nsCollationStrength strength, 
  const nsAString& stringIn, 
  PRUint8* key, 
  PRUint32* outLen)
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(key);
  NS_ENSURE_ARG_POINTER(outLen);
  *outLen = 0;

  nsresult res = EnsureCollator(strength);
  NS_ENSURE_SUCCESS(res, res);

  OSStatus err;
  
  // If we already get a key at the estimation then just copy the cached data.
  PRUint32 stringInLen = stringIn.Length();
  if (mCachedKeyLen && 
      mCachedStringLen == stringInLen &&
      !memcmp((void *) mCachedString, 
              (void *) (const UniChar*) PromiseFlatString(stringIn).get(), stringInLen*sizeof(UniChar)))
  {
    memcpy((void *) key, (void *) mCachedKey, mCachedKeyLen);
    *outLen = mCachedKeyLen;
    return NS_OK;
  }

  // If not cached then call the API.
  PRUint32 keyLength = (1 + stringInLen) * kCollationValueSizeFactor * sizeof(UCCollationValue);
  ItemCount actual;
  
  err = ::UCGetCollationKey(mCollator, (const UniChar*) PromiseFlatString(stringIn).get(),
                           (UniCharCount) stringInLen,
                           (ItemCount) (keyLength / sizeof(UCCollationValue)),
                           &actual, (UCCollationValue *)key);
  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);

  *outLen = actual * sizeof(UCCollationValue);

  return NS_OK;
}


    
NS_IMETHODIMP nsCollationMacUC::CompareString(
  const nsCollationStrength strength, 
  const nsAString& string1, 
  const nsAString& string2, 
  PRInt32* result) 
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(result);
  *result = 0;

  nsresult res = EnsureCollator(strength);
  NS_ENSURE_SUCCESS(res, res);

  SInt32 order;

  OSStatus err;
  err = ::UCCompareText(mCollator, 
                        (const UniChar *) PromiseFlatString(string1).get(), (UniCharCount) string1.Length(),
                        (const UniChar *) PromiseFlatString(string2).get(), (UniCharCount) string2.Length(),
                        NULL, &order);

  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  // order could be -2, -1, 0, 1, or 2, 
  // *result can only be -1, 0 or 1
  *result = (order == 0) ? 0 : ((order > 0) ? 1 : -1);
  return NS_OK;
}


NS_IMETHODIMP nsCollationMacUC::CompareRawSortKey(
  const PRUint8* key1, const PRUint32 len1, 
  const PRUint8* key2, const PRUint32 len2, 
  PRInt32* result)
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(key1);
  NS_ENSURE_ARG_POINTER(key2);
  NS_ENSURE_ARG_POINTER(result);
  *result = 0;
  
  SInt32 order;

  OSStatus err;
  err = ::UCCompareCollationKeys((const UCCollationValue*) key1, (ItemCount) len1,
                                 (const UCCollationValue*) key2, (ItemCount) len2,
                                 NULL, &order);

  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  // order could be -2, -1, 0, 1, or 2, 
  // *result can only be -1, 0 or 1
  *result = (order == 0) ? 0 : ((order > 0) ? 1 : -1);
  return NS_OK;
}


#endif
