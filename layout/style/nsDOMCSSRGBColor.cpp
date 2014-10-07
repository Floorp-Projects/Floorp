/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */ /* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing color values in DOM computed style */

#include "nsDOMCSSRGBColor.h"

#include "mozilla/dom/RGBColorBinding.h"
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
}

nsDOMCSSRGBColor::~nsDOMCSSRGBColor(void)
{
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsDOMCSSRGBColor, mAlpha,  mBlue, mGreen, mRed)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(nsDOMCSSRGBColor, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(nsDOMCSSRGBColor, Release)

JSObject*
nsDOMCSSRGBColor::WrapObject(JSContext *aCx)
{
  return dom::RGBColorBinding::Wrap(aCx, this);
}

