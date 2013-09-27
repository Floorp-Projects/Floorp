/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCollationMacUC.h"
#include "nsILocaleService.h"
#include "nsIServiceManager.h"
#include "prmem.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS1(nsCollationMacUC, nsICollation)

nsCollationMacUC::nsCollationMacUC() 
  : mInit(false)
  , mHasCollator(false)
  , mLocale(nullptr)
  , mLastStrength(-1)
  , mCollator(nullptr)
  , mBuffer(nullptr)
  , mBufferLen(1)
{
}

nsCollationMacUC::~nsCollationMacUC() 
{
  if (mHasCollator) {
#ifdef DEBUG
    OSStatus err =
#endif
      ::UCDisposeCollator(&mCollator);
    mHasCollator = false;
    NS_ASSERTION((err == noErr), "UCDisposeCollator failed");
  }
  PR_FREEIF(mBuffer);
}

nsresult nsCollationMacUC::StrengthToOptions(const int32_t aStrength,
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

nsresult nsCollationMacUC::ConvertLocale(nsILocale* aNSLocale, LocaleRef* aMacLocale) 
{
  NS_ENSURE_ARG_POINTER(aNSLocale);
  NS_ENSURE_ARG_POINTER(aMacLocale);

  nsAutoString localeString;
  nsresult res = aNSLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE"), localeString);
  NS_ENSURE_TRUE(NS_SUCCEEDED(res) && !localeString.IsEmpty(),
                 NS_ERROR_FAILURE);
  NS_LossyConvertUTF16toASCII tmp(localeString);
  tmp.ReplaceChar('-', '_');
  OSStatus err;
  err = ::LocaleRefFromLocaleString(tmp.get(), aMacLocale);
  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);

  return NS_OK;
}

nsresult nsCollationMacUC::EnsureCollator(const int32_t newStrength) 
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  if (mHasCollator && (mLastStrength == newStrength))
    return NS_OK;

  OSStatus err;
  if (mHasCollator) {
    err = ::UCDisposeCollator(&mCollator);
    mHasCollator = false;
    NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  }

  UCCollateOptions newOptions;
  nsresult res = StrengthToOptions(newStrength, &newOptions);
  NS_ENSURE_SUCCESS(res, res);
  
  LocaleOperationVariant opVariant = 0; // default variant for now
  err = ::UCCreateCollator(mLocale, opVariant, newOptions, &mCollator);
  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  mHasCollator = true;

  mLastStrength = newStrength;
  return NS_OK;
}

NS_IMETHODIMP nsCollationMacUC::Initialize(nsILocale* locale) 
{
  NS_ENSURE_TRUE((!mInit), NS_ERROR_ALREADY_INITIALIZED);
  nsCOMPtr<nsILocale> appLocale;

  nsresult rv;
  if (!locale) {
    nsCOMPtr<nsILocaleService> localeService = do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
    NS_ENSURE_SUCCESS(rv, rv);
    locale = appLocale;
  }

  rv = ConvertLocale(locale, &mLocale);
  NS_ENSURE_SUCCESS(rv, rv);

  mInit = true;
  return NS_OK;
}

NS_IMETHODIMP nsCollationMacUC::AllocateRawSortKey(int32_t strength, const nsAString& stringIn,
                                                   uint8_t** key, uint32_t* outLen)
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(key);
  NS_ENSURE_ARG_POINTER(outLen);

  nsresult res = EnsureCollator(strength);
  NS_ENSURE_SUCCESS(res, res);

  uint32_t stringInLen = stringIn.Length();
  uint32_t maxKeyLen = (1 + stringInLen) * kCollationValueSizeFactor * sizeof(UCCollationValue);
  if (maxKeyLen > mBufferLen) {
    uint32_t newBufferLen = mBufferLen;
    do {
      newBufferLen *= 2;
    } while (newBufferLen < maxKeyLen);
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
                                     &actual, (UCCollationValue *)mBuffer);
  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);

  uint32_t keyLength = actual * sizeof(UCCollationValue);
  void *newKey = PR_Malloc(keyLength);
  if (!newKey)
    return NS_ERROR_OUT_OF_MEMORY;

  memcpy(newKey, mBuffer, keyLength);
  *key = (uint8_t *)newKey;
  *outLen = keyLength;

  return NS_OK;
}

NS_IMETHODIMP nsCollationMacUC::CompareString(int32_t strength, const nsAString& string1,
                                              const nsAString& string2, int32_t* result) 
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
                        nullptr, (SInt32*) result);

  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);
  return NS_OK;
}

NS_IMETHODIMP nsCollationMacUC::CompareRawSortKey(const uint8_t* key1, uint32_t len1,
                                                  const uint8_t* key2, uint32_t len2,
                                                  int32_t* result)
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(key1);
  NS_ENSURE_ARG_POINTER(key2);
  NS_ENSURE_ARG_POINTER(result);
  *result = 0;

  OSStatus err;
  err = ::UCCompareCollationKeys((const UCCollationValue*) key1, (ItemCount) len1,
                                 (const UCCollationValue*) key2, (ItemCount) len2,
                                 nullptr, (SInt32*) result);

  NS_ENSURE_TRUE((err == noErr), NS_ERROR_FAILURE);

  return NS_OK;
}
