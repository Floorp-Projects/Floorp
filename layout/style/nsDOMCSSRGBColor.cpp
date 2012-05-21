/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing color values in DOM computed style */

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsDOMCSSRGBColor.h"
#include "nsContentUtils.h"

nsDOMCSSRGBColor::nsDOMCSSRGBColor(nsIDOMCSSPrimitiveValue* aRed,
                                   nsIDOMCSSPrimitiveValue* aGreen,
                                   nsIDOMCSSPrimitiveValue* aBlue,
                                   nsIDOMCSSPrimitiveValue* aAlpha,
                                   bool aHasAlpha)
  : mRed(aRed), mGreen(aGreen), mBlue(aBlue), mAlpha(aAlpha)
  , mHasAlpha(aHasAlpha)
{
}

nsDOMCSSRGBColor::~nsDOMCSSRGBColor(void)
{
}

DOMCI_DATA(CSSRGBColor, nsDOMCSSRGBColor)

NS_INTERFACE_MAP_BEGIN(nsDOMCSSRGBColor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMRGBColor)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNSRGBAColor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSRGBColor)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsDOMCSSRGBColor)
NS_IMPL_RELEASE(nsDOMCSSRGBColor)


NS_IMETHODIMP
nsDOMCSSRGBColor::GetRed(nsIDOMCSSPrimitiveValue** aRed)
{
  NS_ENSURE_TRUE(mRed, NS_ERROR_NOT_INITIALIZED);
  *aRed = mRed;
  NS_ADDREF(*aRed);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRGBColor::GetGreen(nsIDOMCSSPrimitiveValue** aGreen)
{
  NS_ENSURE_TRUE(mGreen, NS_ERROR_NOT_INITIALIZED);
  *aGreen = mGreen;
  NS_ADDREF(*aGreen);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRGBColor::GetBlue(nsIDOMCSSPrimitiveValue** aBlue)
{
  NS_ENSURE_TRUE(mBlue, NS_ERROR_NOT_INITIALIZED);
  *aBlue = mBlue;
  NS_ADDREF(*aBlue);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMCSSRGBColor::GetAlpha(nsIDOMCSSPrimitiveValue** aAlpha)
{
  NS_ENSURE_TRUE(mAlpha, NS_ERROR_NOT_INITIALIZED);
  *aAlpha = mAlpha;
  NS_ADDREF(*aAlpha);
  return NS_OK;
}
