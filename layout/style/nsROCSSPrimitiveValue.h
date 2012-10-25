/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing values in DOM computed style */

#ifndef nsROCSSPrimitiveValue_h___
#define nsROCSSPrimitiveValue_h___

#include "nsIDOMCSSValue.h"
#include "nsIDOMCSSPrimitiveValue.h"
#include "nsCSSKeywords.h"
#include "CSSValue.h"
#include "nsAutoPtr.h"
#include "nsCoord.h"
#include "nsWrapperCache.h"

class nsIURI;
class nsComputedDOMStyle;
class nsDOMCSSRGBColor;

/**
 * Read-only CSS primitive value - a DOM object representing values in DOM
 * computed style.
 */
class nsROCSSPrimitiveValue MOZ_FINAL : public mozilla::dom::CSSValue,
  public nsIDOMCSSPrimitiveValue
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsROCSSPrimitiveValue, mozilla::dom::CSSValue)

  // nsIDOMCSSPrimitiveValue
  NS_DECL_NSIDOMCSSPRIMITIVEVALUE

  // nsIDOMCSSValue
  NS_DECL_NSIDOMCSSVALUE

  // CSSValue
  virtual void GetCssText(nsString& aText, mozilla::ErrorResult& aRv) MOZ_OVERRIDE MOZ_FINAL;
  virtual void SetCssText(const nsAString& aText, mozilla::ErrorResult& aRv) MOZ_OVERRIDE MOZ_FINAL;
  virtual uint16_t CssValueType() const MOZ_OVERRIDE MOZ_FINAL;

  // CSSPrimitiveValue
  uint16_t PrimitiveType()
  {
    return mType;
  }
  void SetFloatValue(uint16_t aUnitType, float aValue,
                     mozilla::ErrorResult& aRv);
  float GetFloatValue(uint16_t aUnitType, mozilla::ErrorResult& aRv);
  void GetStringValue(nsString& aString, mozilla::ErrorResult& aRv);
  void SetStringValue(uint16_t aUnitType, const nsAString& aString,
                      mozilla::ErrorResult& aRv);
  already_AddRefed<nsIDOMCounter> GetCounterValue(mozilla::ErrorResult& aRv);
  already_AddRefed<nsIDOMRect> GetRectValue(mozilla::ErrorResult& aRv);
  nsDOMCSSRGBColor *GetRGBColorValue(mozilla::ErrorResult& aRv);

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue();
  ~nsROCSSPrimitiveValue();

  void SetNumber(float aValue);
  void SetNumber(int32_t aValue);
  void SetNumber(uint32_t aValue);
  void SetPercent(float aValue);
  void SetAppUnits(nscoord aValue);
  void SetAppUnits(float aValue);
  void SetIdent(nsCSSKeyword aKeyword);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(const nsACString& aString, uint16_t aType = CSS_STRING);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(const nsAString& aString, uint16_t aType = CSS_STRING);
  void SetURI(nsIURI *aURI);
  void SetColor(nsDOMCSSRGBColor* aColor);
  void SetRect(nsIDOMRect* aRect);
  void SetTime(float aValue);
  void Reset();

  nsISupports* GetParentObject() const
  {
    return nullptr;
  }

  virtual JSObject *WrapObject(JSContext *cx, JSObject *scope, bool *triedToWrap);

private:
  uint16_t mType;

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

