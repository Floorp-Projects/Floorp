/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing rectangle values in DOM computed style */

#include "mozilla/dom/RectBinding.h"
#include "nsROCSSPrimitiveValue.h"
#include "nsContentUtils.h"
#include "nsDOMCSSRect.h"
#include "nsDOMClassInfoID.h"

using namespace mozilla;

nsDOMCSSRect::nsDOMCSSRect(nsROCSSPrimitiveValue* aTop,
                           nsROCSSPrimitiveValue* aRight,
                           nsROCSSPrimitiveValue* aBottom,
                           nsROCSSPrimitiveValue* aLeft)
  : mTop(aTop), mRight(aRight), mBottom(aBottom), mLeft(aLeft)
{
  SetIsDOMBinding();
}

nsDOMCSSRect::~nsDOMCSSRect(void)
{
}

DOMCI_DATA(CSSRect, nsDOMCSSRect)

// QueryInterface implementation for nsCSSRect
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCSSRect)
  NS_INTERFACE_MAP_ENTRY(nsIDOMRect)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSRect)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCSSRect)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCSSRect)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_4(nsDOMCSSRect, mTop, mBottom, mLeft, mRight)
 
JSObject*
nsDOMCSSRect::WrapObject(JSContext* cx, JS::Handle<JSObject*> scope)
{
 return dom::RectBinding::Wrap(cx, scope, this);
}

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
