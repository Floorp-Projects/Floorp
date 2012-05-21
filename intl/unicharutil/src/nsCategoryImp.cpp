/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nscore.h"
#include "pratom.h"
#include "nsUUDll.h"
#include "nsISupports.h"
#include "nsCategoryImp.h"
#include "nsUnicodeProperties.h"

static nsCategoryImp gCategoryImp;

NS_IMPL_THREADSAFE_QUERY_INTERFACE1(nsCategoryImp, nsIUGenCategory)

NS_IMETHODIMP_(nsrefcnt) nsCategoryImp::AddRef(void)
{
  return nsrefcnt(1);
}

NS_IMETHODIMP_(nsrefcnt) nsCategoryImp::Release(void)
{
  return nsrefcnt(1);
}

nsCategoryImp* nsCategoryImp::GetInstance()
{
  return &gCategoryImp;
}

nsIUGenCategory::nsUGenCategory nsCategoryImp::Get(PRUint32 aChar)
{
  return mozilla::unicode::GetGenCategory(aChar);
}
