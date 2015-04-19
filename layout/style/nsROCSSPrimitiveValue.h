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
#include "nsCOMPtr.h"
#include "nsCoord.h"

class nsIURI;
class nsDOMCSSRect;
class nsDOMCSSRGBColor;

// There is no CSS_TURN constant on the CSSPrimitiveValue interface,
// since that unit is newer than DOM Level 2 Style, and CSS OM will
// probably expose CSS values in some other way in the future.  We
// use this value in mType for "turn"-unit angles, but we define it
// here to avoid exposing it to content.
#define CSS_TURN 30U
// Likewise we have some internal aliases for CSS_NUMBER that we don't
// want to expose.
#define CSS_NUMBER_INT32 31U
#define CSS_NUMBER_UINT32 32U

/**
 * Read-only CSS primitive value - a DOM object representing values in DOM
 * computed style.
 */
class nsROCSSPrimitiveValue final : public mozilla::dom::CSSValue,
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
  virtual void GetCssText(nsString& aText, mozilla::ErrorResult& aRv) override final;
  virtual void SetCssText(const nsAString& aText, mozilla::ErrorResult& aRv) override final;
  virtual uint16_t CssValueType() const override final;

  // CSSPrimitiveValue
  uint16_t PrimitiveType()
  {
    // New value types were introduced but not added to CSS OM.
    // Return CSS_UNKNOWN to avoid exposing CSS_TURN to content.
    if (mType > CSS_RGBCOLOR) {
      if (mType == CSS_NUMBER_INT32 || mType == CSS_NUMBER_UINT32) {
        return CSS_NUMBER;
      }
      return CSS_UNKNOWN;
    }
    return mType;
  }
  void SetFloatValue(uint16_t aUnitType, float aValue,
                     mozilla::ErrorResult& aRv);
  float GetFloatValue(uint16_t aUnitType, mozilla::ErrorResult& aRv);
  void GetStringValue(nsString& aString, mozilla::ErrorResult& aRv);
  void SetStringValue(uint16_t aUnitType, const nsAString& aString,
                      mozilla::ErrorResult& aRv);
  already_AddRefed<nsIDOMCounter> GetCounterValue(mozilla::ErrorResult& aRv);
  nsDOMCSSRect* GetRectValue(mozilla::ErrorResult& aRv);
  nsDOMCSSRGBColor *GetRGBColorValue(mozilla::ErrorResult& aRv);

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue();

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
  void SetString(const nsACString& aString, uint16_t aType = CSS_STRING);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(const nsAString& aString, uint16_t aType = CSS_STRING);
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

inline nsROCSSPrimitiveValue *mozilla::dom::CSSValue::AsPrimitiveValue()
{
  return CssValueType() == nsIDOMCSSValue::CSS_PRIMITIVE_VALUE ?
    static_cast<nsROCSSPrimitiveValue*>(this) : nullptr;
}

#endif /* nsROCSSPrimitiveValue_h___ */

