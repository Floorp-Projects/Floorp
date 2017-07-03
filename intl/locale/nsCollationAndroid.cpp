/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCollation.h"
#include "nsCOMPtr.h"
#include "nsUnicharUtils.h"

#include  <string.h>

nsCollation::nsCollation()
{
}

nsCollation::~nsCollation()
{
}

NS_IMPL_ISUPPORTS(nsCollation, nsICollation)

NS_IMETHODIMP
nsCollation::Initialize(const nsACString& locale)
{
  // Android doesn't have locale support
  return NS_OK;
}

NS_IMETHODIMP
nsCollation::CompareString(int32_t strength,
                           const nsAString& string1,
                           const nsAString& string2,
                           int32_t* result)
{
  NS_ENSURE_ARG_POINTER(result);

  nsAutoString stringNormalized1(string1);
  nsAutoString stringNormalized2(string2);
  if (strength != kCollationCaseSensitive) {
    ToLowerCase(stringNormalized1);
    ToLowerCase(stringNormalized2);
  }

  *result = strcmp(NS_ConvertUTF16toUTF8(stringNormalized1).get(),
                   NS_ConvertUTF16toUTF8(stringNormalized2).get());

  return NS_OK;
}

NS_IMETHODIMP
nsCollation::AllocateRawSortKey(int32_t strength,
                                const nsAString& stringIn,
                                uint8_t** key, uint32_t* outLen)
{
  NS_ENSURE_ARG_POINTER(key);
  NS_ENSURE_ARG_POINTER(outLen);

  nsAutoString stringNormalized(stringIn);
  if (strength != kCollationCaseSensitive) {
    ToLowerCase(stringNormalized);
  }

  NS_ConvertUTF16toUTF8 str(stringNormalized);
  size_t len = str.Length() + 1;
  void* buffer = malloc(len);
  memcpy(buffer, str.get(), len);
  *key = (uint8_t *)buffer;
  *outLen = len;

  return NS_OK;
}

NS_IMETHODIMP
nsCollation::CompareRawSortKey(const uint8_t* key1, uint32_t len1,
                               const uint8_t* key2, uint32_t len2,
                               int32_t* result)
{
  NS_ENSURE_ARG_POINTER(key1);
  NS_ENSURE_ARG_POINTER(key2);
  NS_ENSURE_ARG_POINTER(result);

  *result = strcmp((const char *)key1, (const char *)key2);
  return NS_OK;
}
