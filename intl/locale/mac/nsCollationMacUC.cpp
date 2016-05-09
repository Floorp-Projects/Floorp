/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCollationMacUC.h"
#include "nsILocaleService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsIServiceManager.h"
#include "prmem.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS(nsCollationMacUC, nsICollation)

nsCollationMacUC::nsCollationMacUC()
  : mInit(false)
  , mHasCollator(false)
  , mLocaleICU(nullptr)
  , mLastStrength(-1)
  , mCollatorICU(nullptr)
{ }

nsCollationMacUC::~nsCollationMacUC()
{
#ifdef DEBUG
  nsresult res =
#endif
    CleanUpCollator();
  NS_ASSERTION(NS_SUCCEEDED(res), "CleanUpCollator failed");
  if (mLocaleICU) {
    free(mLocaleICU);
    mLocaleICU = nullptr;
  }
}

nsresult nsCollationMacUC::ConvertStrength(const int32_t aNSStrength,
                                           UCollationStrength* aICUStrength,
                                           UColAttributeValue* aCaseLevelOut)
{
  NS_ENSURE_ARG_POINTER(aICUStrength);
  NS_ENSURE_TRUE((aNSStrength < 4), NS_ERROR_FAILURE);

  UCollationStrength strength = UCOL_DEFAULT;
  UColAttributeValue caseLevel = UCOL_OFF;
  switch (aNSStrength) {
    case kCollationCaseInSensitive:
      strength = UCOL_PRIMARY;
      break;
    case kCollationCaseInsensitiveAscii:
      strength = UCOL_SECONDARY;
      break;
    case kCollationAccentInsenstive:
      caseLevel = UCOL_ON;
      strength = UCOL_PRIMARY;
      break;
    case kCollationCaseSensitive:
      strength = UCOL_TERTIARY;
      break;
    default:
      NS_WARNING("Bad aNSStrength passed to ConvertStrength.");
      return NS_ERROR_FAILURE;
  }

  *aICUStrength = strength;
  *aCaseLevelOut = caseLevel;

  return NS_OK;
}

nsresult nsCollationMacUC::ConvertLocaleICU(nsILocale* aNSLocale, char** aICULocale)
{
  NS_ENSURE_ARG_POINTER(aNSLocale);
  NS_ENSURE_ARG_POINTER(aICULocale);

  nsAutoString localeString;
  nsresult res = aNSLocale->GetCategory(NS_LITERAL_STRING("NSILOCALE_COLLATE"), localeString);
  NS_ENSURE_TRUE(NS_SUCCEEDED(res) && !localeString.IsEmpty(),
                 NS_ERROR_FAILURE);
  NS_LossyConvertUTF16toASCII tmp(localeString);
  tmp.ReplaceChar('-', '_');
  char* locale = (char*)malloc(tmp.Length() + 1);
  if (!locale) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  strcpy(locale, tmp.get());

  *aICULocale = locale;

  return NS_OK;
}

nsresult nsCollationMacUC::EnsureCollator(const int32_t newStrength)
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  if (mHasCollator && (mLastStrength == newStrength))
    return NS_OK;

  nsresult res;
  res = CleanUpCollator();
  NS_ENSURE_SUCCESS(res, res);

  NS_ENSURE_TRUE(mLocaleICU, NS_ERROR_NOT_INITIALIZED);

  UErrorCode status;
  status = U_ZERO_ERROR;
  mCollatorICU = ucol_open(mLocaleICU, &status);
  NS_ENSURE_TRUE(U_SUCCESS(status), NS_ERROR_FAILURE);

  UCollationStrength strength;
  UColAttributeValue caseLevel;
  res = ConvertStrength(newStrength, &strength, &caseLevel);
  NS_ENSURE_SUCCESS(res, res);

  status = U_ZERO_ERROR;
  ucol_setAttribute(mCollatorICU, UCOL_STRENGTH, strength, &status);
  NS_ENSURE_TRUE(U_SUCCESS(status), NS_ERROR_FAILURE);
  ucol_setAttribute(mCollatorICU, UCOL_CASE_LEVEL, caseLevel, &status);
  NS_ENSURE_TRUE(U_SUCCESS(status), NS_ERROR_FAILURE);
  ucol_setAttribute(mCollatorICU, UCOL_ALTERNATE_HANDLING, UCOL_DEFAULT, &status);
  NS_ENSURE_TRUE(U_SUCCESS(status), NS_ERROR_FAILURE);
  ucol_setAttribute(mCollatorICU, UCOL_NUMERIC_COLLATION, UCOL_OFF, &status);
  NS_ENSURE_TRUE(U_SUCCESS(status), NS_ERROR_FAILURE);
  ucol_setAttribute(mCollatorICU, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
  NS_ENSURE_TRUE(U_SUCCESS(status), NS_ERROR_FAILURE);
  ucol_setAttribute(mCollatorICU, UCOL_CASE_FIRST, UCOL_DEFAULT, &status);
  NS_ENSURE_TRUE(U_SUCCESS(status), NS_ERROR_FAILURE);

  mHasCollator = true;

  mLastStrength = newStrength;
  return NS_OK;
}

nsresult nsCollationMacUC::CleanUpCollator(void)
{
  if (mHasCollator) {
    ucol_close(mCollatorICU);
    mHasCollator = false;
  }

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

  rv = ConvertLocaleICU(locale, &mLocaleICU);
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

  const UChar* str = (const UChar*)stringIn.BeginReading();

  int32_t keyLength = ucol_getSortKey(mCollatorICU, str, stringInLen, nullptr, 0);
  NS_ENSURE_TRUE((stringInLen == 0 || keyLength > 0), NS_ERROR_FAILURE);

  // Since key is freed elsewhere with PR_Free, allocate with PR_Malloc.
  uint8_t* newKey = (uint8_t*)PR_Malloc(keyLength + 1);
  if (!newKey) {
      return NS_ERROR_OUT_OF_MEMORY;
  }

  keyLength = ucol_getSortKey(mCollatorICU, str, stringInLen, newKey, keyLength + 1);
  NS_ENSURE_TRUE((stringInLen == 0 || keyLength > 0), NS_ERROR_FAILURE);

  *key = newKey;
  *outLen = keyLength;

  return NS_OK;
}

NS_IMETHODIMP nsCollationMacUC::CompareString(int32_t strength, const nsAString& string1,
                                              const nsAString& string2, int32_t* result)
{
  NS_ENSURE_TRUE(mInit, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(result);
  *result = 0;

  nsresult rv = EnsureCollator(strength);
  NS_ENSURE_SUCCESS(rv, rv);

  UCollationResult uresult;
  uresult = ucol_strcoll(mCollatorICU,
                         (const UChar*)string1.BeginReading(),
                         string1.Length(),
                         (const UChar*)string2.BeginReading(),
                         string2.Length());
  int32_t res;
  switch (uresult) {
    case UCOL_LESS:
      res = -1;
      break;
    case UCOL_EQUAL:
      res = 0;
      break;
    case UCOL_GREATER:
      res = 1;
      break;
    default:
      MOZ_CRASH("ucol_strcoll returned bad UCollationResult");
  }
  *result = res;
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

  int32_t tmpResult = strcmp((const char*)key1, (const char*)key2);
  int32_t res;
  if (tmpResult < 0) {
      res = -1;
  } else if (tmpResult > 0) {
      res = 1;
  } else {
      res = 0;
  }
  *result = res;
  return NS_OK;
}
