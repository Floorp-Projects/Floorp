/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing color values in DOM computed style */

#ifndef nsDOMCSSRGBColor_h__
#define nsDOMCSSRGBColor_h__

#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"

class nsROCSSPrimitiveValue;

class nsDOMCSSRGBColor final : public mozilla::RefCounted<nsDOMCSSRGBColor>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(nsDOMCSSRGBColor);
  nsDOMCSSRGBColor(nsROCSSPrimitiveValue* aRed,
                   nsROCSSPrimitiveValue* aGreen,
                   nsROCSSPrimitiveValue* aBlue,
                   nsROCSSPrimitiveValue* aAlpha,
                   bool aHasAlpha);

  bool HasAlpha() const { return mHasAlpha; }

  // RGBColor webidl interface
  nsROCSSPrimitiveValue* Red() const
  {
    return mRed;
  }
  nsROCSSPrimitiveValue* Green() const
  {
    return mGreen;
  }
  nsROCSSPrimitiveValue* Blue() const
  {
    return mBlue;
  }
  nsROCSSPrimitiveValue* Alpha() const
  {
    return mAlpha;
  }

  ~nsDOMCSSRGBColor();

protected:
  RefPtr<nsROCSSPrimitiveValue> mRed;
  RefPtr<nsROCSSPrimitiveValue> mGreen;
  RefPtr<nsROCSSPrimitiveValue> mBlue;
  RefPtr<nsROCSSPrimitiveValue> mAlpha;
  bool mHasAlpha;
};

#endif // nsDOMCSSRGBColor_h__
