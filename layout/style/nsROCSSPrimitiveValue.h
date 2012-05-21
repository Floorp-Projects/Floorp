/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing values in DOM computed style */

#ifndef nsROCSSPrimitiveValue_h___
#define nsROCSSPrimitiveValue_h___

#include "nsIDOMCSSPrimitiveValue.h"
#include "nsCoord.h"
#include "nsCSSKeywords.h"

class nsIURI;
class nsDOMCSSRGBColor;

/**
 * Read-only CSS primitive value - a DOM object representing values in DOM
 * computed style.
 */
class nsROCSSPrimitiveValue : public nsIDOMCSSPrimitiveValue
{
public:
  NS_DECL_ISUPPORTS

  // nsIDOMCSSPrimitiveValue
  NS_DECL_NSIDOMCSSPRIMITIVEVALUE

  // nsIDOMCSSValue
  NS_DECL_NSIDOMCSSVALUE

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue();
  virtual ~nsROCSSPrimitiveValue();

  void SetNumber(float aValue);
  void SetNumber(PRInt32 aValue);
  void SetNumber(PRUint32 aValue);
  void SetPercent(float aValue);
  void SetAppUnits(nscoord aValue);
  void SetAppUnits(float aValue);
  void SetIdent(nsCSSKeyword aKeyword);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(const nsACString& aString, PRUint16 aType = CSS_STRING);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(const nsAString& aString, PRUint16 aType = CSS_STRING);
  void SetURI(nsIURI *aURI);
  void SetColor(nsDOMCSSRGBColor* aColor);
  void SetRect(nsIDOMRect* aRect);
  void SetTime(float aValue);
  void Reset();

private:
  PRUint16 mType;

  union {
    nscoord         mAppUnits;
    float           mFloat;
    nsDOMCSSRGBColor* mColor;
    nsIDOMRect*     mRect;
    PRUnichar*      mString;
    nsIURI*         mURI;
    nsCSSKeyword    mKeyword;
  } mValue;
};

#endif /* nsROCSSPrimitiveValue_h___ */

