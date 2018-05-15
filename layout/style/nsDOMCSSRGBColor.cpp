/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing color values in DOM computed style */

#include "nsDOMCSSRGBColor.h"

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

nsDOMCSSRGBColor::~nsDOMCSSRGBColor() = default;
