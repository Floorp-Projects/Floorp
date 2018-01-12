/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing values in DOM computed style */

#ifndef nsROCSSPrimitiveValue_h___
#define nsROCSSPrimitiveValue_h___

#include "mozilla/dom/CSSPrimitiveValueBinding.h"
#include "mozilla/dom/CSSValueBinding.h"

#include "nsCSSKeywords.h"
#include "CSSValue.h"
#include "nsCOMPtr.h"
#include "nsCoord.h"

class nsIURI;
class nsDOMCSSRect;
class nsDOMCSSRGBColor;

/**
 * Read-only CSS primitive value - a DOM object representing values in DOM
 * computed style.
 */
class nsROCSSPrimitiveValue final : public mozilla::dom::CSSValue
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(nsROCSSPrimitiveValue)

  // CSSValue
  virtual void GetCssText(nsString& aText, mozilla::ErrorResult& aRv) override final;
  virtual void SetCssText(const nsAString& aText, mozilla::ErrorResult& aRv) override final;
  virtual uint16_t CssValueType() const override final;

  // CSSPrimitiveValue
  uint16_t PrimitiveType();
  void SetFloatValue(uint16_t aUnitType, float aValue,
                     mozilla::ErrorResult& aRv);
  float GetFloatValue(uint16_t aUnitType, mozilla::ErrorResult& aRv);
  void GetStringValue(nsString& aString, mozilla::ErrorResult& aRv);
  void SetStringValue(uint16_t aUnitType, const nsAString& aString,
                      mozilla::ErrorResult& aRv);
  void GetCounterValue(mozilla::ErrorResult& aRv);
  nsDOMCSSRect* GetRectValue(mozilla::ErrorResult& aRv);
  nsDOMCSSRGBColor *GetRGBColorValue(mozilla::ErrorResult& aRv);

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue();

  nsresult GetCssText(nsAString& aText);
  void SetNumber(float aValue);
  void SetNumber(int32_t aValue);
  void SetNumber(uint32_t aValue);
  void SetPercent(float aValue);
  void SetDegree(float aValue);
  void SetGrad(float aValue);
  void SetRadian(float aValue);
  void SetTurn(float aValue);
  void SetAppUnits(nscoord aValue);
  void SetAppUnits(float aValue);
  void SetIdent(nsCSSKeyword aKeyword);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(
      const nsACString& aString,
      uint16_t aType = mozilla::dom::CSSPrimitiveValueBinding::CSS_STRING);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(
      const nsAString& aString,
      uint16_t aType = mozilla::dom::CSSPrimitiveValueBinding::CSS_STRING);
  void SetURI(nsIURI *aURI);
  void SetColor(nsDOMCSSRGBColor* aColor);
  void SetRect(nsDOMCSSRect* aRect);
  void SetTime(float aValue);
  void Reset();

  nsISupports* GetParentObject() const
  {
    return nullptr;
  }

  virtual JSObject *WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override;

private:
  ~nsROCSSPrimitiveValue();

  uint16_t mType;

  union {
    nscoord         mAppUnits;
    float           mFloat;
    int32_t         mInt32;
    uint32_t        mUint32;
    // These can't be nsCOMPtr/nsRefPtr's because they are used inside a union.
    nsDOMCSSRGBColor* MOZ_OWNING_REF mColor;
    nsDOMCSSRect* MOZ_OWNING_REF mRect;
    char16_t*      mString;
    nsIURI* MOZ_OWNING_REF mURI;
    nsCSSKeyword    mKeyword;
  } mValue;
};

inline nsROCSSPrimitiveValue*
mozilla::dom::CSSValue::AsPrimitiveValue()
{
  return CssValueType() == CSSValueBinding::CSS_PRIMITIVE_VALUE ?
    static_cast<nsROCSSPrimitiveValue*>(this) : nullptr;
}

#endif /* nsROCSSPrimitiveValue_h___ */
