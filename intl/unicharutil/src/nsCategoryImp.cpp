/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "nsISupports.h"
#include "nsCategoryImp.h"
#include "nsUnicodeProperties.h"

NS_IMPL_QUERY_INTERFACE1(nsCategoryImp, nsIUGenCategory)

NS_IMETHODIMP_(MozExternalRefCountType) nsCategoryImp::AddRef(void)
{
  return MozExternalRefCountType(1);
}

NS_IMETHODIMP_(MozExternalRefCountType) nsCategoryImp::Release(void)
{
  return MozExternalRefCountType(1);
}

nsCategoryImp* nsCategoryImp::GetInstance()
{
  static nsCategoryImp categoryImp;
  return &categoryImp;
}

nsIUGenCategory::nsUGenCategory nsCategoryImp::Get(uint32_t aChar)
{
  return mozilla::unicode::GetGenCategory(aChar);
}
