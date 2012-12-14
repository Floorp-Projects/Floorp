/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing color values in DOM computed style */

#include "nsDOMCSSRGBColor.h"

#include "mozilla/dom/RGBColorBinding.h"
#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsContentUtils.h"
#include "nsROCSSPrimitiveValue.h"

using namespace mozilla;

nsDOMCSSRGBColor::nsDOMCSSRGBColor(nsROCSSPrimitiveValue* aRed,
                                   nsROCSSPrimitiveValue* aGreen,
                                   nsROCSSPrimitiveValue* aBlue,
                                   nsROCSSPrimitiveValue* aAlpha,
                                   bool aHasAlpha)
  : mRed(aRed), mGreen(aGreen), mBlue(aBlue), mAlpha(aAlpha)
  , mHasAlpha(aHasAlpha)
{
  SetIsDOMBinding();
}

nsDOMCSSRGBColor::~nsDOMCSSRGBColor(void)
{
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMCSSRGBColor)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_4(nsDOMCSSRGBColor, mAlpha,  mBlue, mGreen, mRed)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMCSSRGBColor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMCSSRGBColor)

JSObject*
nsDOMCSSRGBColor::WrapObject(JSContext *aCx, JSObject *aScope, bool *aTried)
{
  return dom::RGBColorBinding::Wrap(aCx, aScope, this, aTried);
}

