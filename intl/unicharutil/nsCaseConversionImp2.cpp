/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCaseConversionImp2.h"
#include "nsUnicharUtils.h"

NS_IMETHODIMP_(MozExternalRefCountType) nsCaseConversionImp2::AddRef(void)
{
  return (MozExternalRefCountType)1;
}

NS_IMETHODIMP_(MozExternalRefCountType) nsCaseConversionImp2::Release(void)
{
  return (MozExternalRefCountType)1;
}

NS_IMPL_QUERY_INTERFACE(nsCaseConversionImp2, nsICaseConversion)

NS_IMETHODIMP nsCaseConversionImp2::ToUpper(char16_t aChar, char16_t* aReturn)
{
  *aReturn = ToUpperCase(aChar);
  return NS_OK;
}

NS_IMETHODIMP nsCaseConversionImp2::ToLower(char16_t aChar, char16_t* aReturn)
{
  *aReturn = ToLowerCase(aChar);
  return NS_OK;
}

NS_IMETHODIMP nsCaseConversionImp2::ToTitle(char16_t aChar, char16_t* aReturn)
{
  *aReturn = ToTitleCase(aChar);
  return NS_OK;
}

NS_IMETHODIMP nsCaseConversionImp2::ToUpper(const char16_t* anArray,
                                         char16_t* aReturn,
                                         uint32_t aLen)
{
  ToUpperCase(anArray, aReturn, aLen);
  return NS_OK;
}

NS_IMETHODIMP nsCaseConversionImp2::ToLower(const char16_t* anArray,
                                         char16_t* aReturn,
                                         uint32_t aLen)
{
  ToLowerCase(anArray, aReturn, aLen);
  return NS_OK;
}

NS_IMETHODIMP
nsCaseConversionImp2::CaseInsensitiveCompare(const char16_t *aLeft,
                                             const char16_t *aRight,
                                             uint32_t aCount, int32_t* aResult)
{
  *aResult = ::CaseInsensitiveCompare(aLeft, aRight, aCount);
  return NS_OK;
}
