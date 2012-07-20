/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing rectangle values in DOM computed style */

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsDOMCSSRect.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"

nsDOMCSSRect::nsDOMCSSRect(nsIDOMCSSPrimitiveValue* aTop,
                           nsIDOMCSSPrimitiveValue* aRight,
                           nsIDOMCSSPrimitiveValue* aBottom,
                           nsIDOMCSSPrimitiveValue* aLeft)
  : mTop(aTop), mRight(aRight), mBottom(aBottom), mLeft(aLeft)
{
}

nsDOMCSSRect::~nsDOMCSSRect(void)
{
}

DOMCI_DATA(CSSRect, nsDOMCSSRect)

// QueryInterface implementation for nsCSSRect
NS_INTERFACE_MAP_BEGIN(nsDOMCSSRect)
  NS_INTERFACE_MAP_ENTRY(nsIDOMRect)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSRect)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMCSSRect)
NS_IMPL_RELEASE(nsDOMCSSRect)

  
NS_IMETHODIMP
nsDOMCSSRect::GetTop(nsIDOMCSSPrimitiveValue** aTop)
{
  NS_ENSURE_TRUE(mTop, NS_ERROR_NOT_INITIALIZED);
  *aTop = mTop;
  NS_ADDREF(*aTop);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRect::GetRight(nsIDOMCSSPrimitiveValue** aRight)
{
  NS_ENSURE_TRUE(mRight, NS_ERROR_NOT_INITIALIZED);
  *aRight = mRight;
  NS_ADDREF(*aRight);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRect::GetBottom(nsIDOMCSSPrimitiveValue** aBottom)
{
  NS_ENSURE_TRUE(mBottom, NS_ERROR_NOT_INITIALIZED);
  *aBottom = mBottom;
  NS_ADDREF(*aBottom);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRect::GetLeft(nsIDOMCSSPrimitiveValue** aLeft)
{
  NS_ENSURE_TRUE(mLeft, NS_ERROR_NOT_INITIALIZED);
  *aLeft = mLeft;
  NS_ADDREF(*aLeft);
  return NS_OK;
}
