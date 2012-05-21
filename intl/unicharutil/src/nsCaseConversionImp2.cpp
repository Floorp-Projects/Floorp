/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCaseConversionImp2.h"
#include "nsUnicharUtils.h"

NS_IMETHODIMP_(nsrefcnt) nsCaseConversionImp2::AddRef(void)
{
  return (nsrefcnt)1;
}

NS_IMETHODIMP_(nsrefcnt) nsCaseConversionImp2::Release(void)
{
  return (nsrefcnt)1;
}

NS_IMPL_THREADSAFE_QUERY_INTERFACE1(nsCaseConversionImp2, nsICaseConversion)

NS_IMETHODIMP nsCaseConversionImp2::ToUpper(PRUnichar aChar, PRUnichar* aReturn)
{
  *aReturn = ToUpperCase(aChar);
  return NS_OK;
}

NS_IMETHODIMP nsCaseConversionImp2::ToLower(PRUnichar aChar, PRUnichar* aReturn)
{
  *aReturn = ToLowerCase(aChar);
  return NS_OK;
}

NS_IMETHODIMP nsCaseConversionImp2::ToTitle(PRUnichar aChar, PRUnichar* aReturn)
{
  *aReturn = ToTitleCase(aChar);
  return NS_OK;
}

NS_IMETHODIMP nsCaseConversionImp2::ToUpper(const PRUnichar* anArray,
                                         PRUnichar* aReturn,
                                         PRUint32 aLen)
{
  ToUpperCase(anArray, aReturn, aLen);
  return NS_OK;
}

NS_IMETHODIMP nsCaseConversionImp2::ToLower(const PRUnichar* anArray,
                                         PRUnichar* aReturn,
                                         PRUint32 aLen)
{
  ToLowerCase(anArray, aReturn, aLen);
  return NS_OK;
}

NS_IMETHODIMP
nsCaseConversionImp2::CaseInsensitiveCompare(const PRUnichar *aLeft,
                                             const PRUnichar *aRight,
                                             PRUint32 aCount, PRInt32* aResult)
{
  *aResult = ::CaseInsensitiveCompare(aLeft, aRight, aCount);
  return NS_OK;
}
