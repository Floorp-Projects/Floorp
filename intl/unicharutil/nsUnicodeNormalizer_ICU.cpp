/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUnicodeNormalizer.h"
#include "ICUUtils.h"
#include "unicode/unorm2.h"
#include "unicode/utext.h"

NS_IMPL_ISUPPORTS(nsUnicodeNormalizer, nsIUnicodeNormalizer)

nsUnicodeNormalizer::nsUnicodeNormalizer()
{
}

nsUnicodeNormalizer::~nsUnicodeNormalizer()
{
}

static nsresult
DoNormalization(const UNormalizer2* aNorm, const nsAString& aSrc,
                nsAString& aDest)
{
  UErrorCode errorCode = U_ZERO_ERROR;
  const int32_t length = aSrc.Length();
  const UChar* src = reinterpret_cast<const UChar*>(aSrc.BeginReading());
  // Initial guess for a capacity that is likely to be enough for most cases.
  int32_t capacity = length + (length >> 8) + 8;
  do {
    aDest.SetLength(capacity);
    UChar* dest = reinterpret_cast<UChar*>(aDest.BeginWriting());
    int32_t len = unorm2_normalize(aNorm, src, aSrc.Length(), dest, capacity,
                                   &errorCode);
    if (U_SUCCESS(errorCode)) {
      aDest.SetLength(len);
      break;
    }
    if (errorCode == U_BUFFER_OVERFLOW_ERROR) {
      // Buffer wasn't big enough; adjust to the reported size and try again.
      capacity = len;
      errorCode = U_ZERO_ERROR;
      continue;
    }
  } while (false);
  return ICUUtils::UErrorToNsResult(errorCode);
}

nsresult
nsUnicodeNormalizer::NormalizeUnicodeNFD(const nsAString& aSrc,
                                         nsAString& aDest)
{
  // The unorm2_getNF*Instance functions return static singletons that should
  // not be deleted, so we just get them once on first use.
  static UErrorCode errorCode = U_ZERO_ERROR;
  static const UNormalizer2* norm = unorm2_getNFDInstance(&errorCode);
  if (U_SUCCESS(errorCode)) {
    return DoNormalization(norm, aSrc, aDest);
  }
  return ICUUtils::UErrorToNsResult(errorCode);
}

nsresult
nsUnicodeNormalizer::NormalizeUnicodeNFC(const nsAString& aSrc,
                                         nsAString& aDest)
{
  static UErrorCode errorCode = U_ZERO_ERROR;
  static const UNormalizer2* norm = unorm2_getNFCInstance(&errorCode);
  if (U_SUCCESS(errorCode)) {
    return DoNormalization(norm, aSrc, aDest);
  }
  return ICUUtils::UErrorToNsResult(errorCode);
}

nsresult
nsUnicodeNormalizer::NormalizeUnicodeNFKD(const nsAString& aSrc,
                                          nsAString& aDest)
{
  static UErrorCode errorCode = U_ZERO_ERROR;
  static const UNormalizer2* norm = unorm2_getNFKDInstance(&errorCode);
  if (U_SUCCESS(errorCode)) {
    return DoNormalization(norm, aSrc, aDest);
  }
  return ICUUtils::UErrorToNsResult(errorCode);
}

nsresult
nsUnicodeNormalizer::NormalizeUnicodeNFKC(const nsAString& aSrc,
                                          nsAString& aDest)
{
  static UErrorCode errorCode = U_ZERO_ERROR;
  static const UNormalizer2* norm = unorm2_getNFKCInstance(&errorCode);
  if (U_SUCCESS(errorCode)) {
    return DoNormalization(norm, aSrc, aDest);
  }
  return ICUUtils::UErrorToNsResult(errorCode);
}
