/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing color values in DOM computed style */

#ifndef nsDOMCSSRGBColor_h__
#define nsDOMCSSRGBColor_h__

#include "nsISupports.h"
#include "nsIDOMNSRGBAColor.h"
#include "nsCOMPtr.h"

class nsIDOMCSSPrimitiveValue;

class nsDOMCSSRGBColor : public nsIDOMNSRGBAColor {
public:
  nsDOMCSSRGBColor(nsIDOMCSSPrimitiveValue* aRed,
                   nsIDOMCSSPrimitiveValue* aGreen,
                   nsIDOMCSSPrimitiveValue* aBlue,
                   nsIDOMCSSPrimitiveValue* aAlpha,
                   bool aHasAlpha);

  virtual ~nsDOMCSSRGBColor(void);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMRGBCOLOR
  NS_DECL_NSIDOMNSRGBACOLOR

  bool HasAlpha() const { return mHasAlpha; }

private:
  nsCOMPtr<nsIDOMCSSPrimitiveValue> mRed;
  nsCOMPtr<nsIDOMCSSPrimitiveValue> mGreen;
  nsCOMPtr<nsIDOMCSSPrimitiveValue> mBlue;
  nsCOMPtr<nsIDOMCSSPrimitiveValue> mAlpha;
  bool mHasAlpha;
};

#endif // nsDOMCSSRGBColor_h__
