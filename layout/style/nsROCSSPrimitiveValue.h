/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing values in DOM computed style */

#ifndef nsROCSSPrimitiveValue_h___
#define nsROCSSPrimitiveValue_h___

#include "nsCSSKeywords.h"
#include "CSSValue.h"
#include "nsCOMPtr.h"
#include "nsCoord.h"

class nsIURI;

/**
 * Read-only CSS primitive value - a DOM object representing values in DOM
 * computed style.
 */
class nsROCSSPrimitiveValue final : public mozilla::dom::CSSValue {
 public:
  enum : uint16_t {
    CSS_UNKNOWN,
    CSS_NUMBER,
    CSS_PERCENTAGE,
    CSS_EMS,
    CSS_EXS,
    CSS_PX,
    CSS_CM,
    CSS_MM,
    CSS_IN,
    CSS_PT,
    CSS_PC,
    CSS_DEG,
    CSS_MS,
    CSS_S,
    CSS_HZ,
    CSS_KHZ,
    CSS_DIMENSION,
    CSS_STRING,
    CSS_URI,
    CSS_IDENT,
    CSS_ATTR,
    CSS_COUNTER,
    CSS_RGBCOLOR,
    CSS_NUMBER_INT32,
    CSS_NUMBER_UINT32,
  };

  // CSSValue
  void GetCssText(nsString& aText, mozilla::ErrorResult& aRv) final;
  uint16_t CssValueType() const final;

  // CSSPrimitiveValue
  uint16_t PrimitiveType();

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue();

  nsresult GetCssText(nsAString& aText);
  void SetNumber(float aValue);
  void SetNumber(int32_t aValue);
  void SetNumber(uint32_t aValue);
  void SetPercent(float aValue);
  void SetDegree(float aValue);
  void SetAppUnits(nscoord aValue);
  void SetAppUnits(float aValue);
  void SetIdent(nsCSSKeyword aKeyword);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(const nsACString& aString, uint16_t aType = CSS_STRING);
  // FIXME: CSS_STRING should imply a string with "" and a need for escaping.
  void SetString(const nsAString& aString, uint16_t aType = CSS_STRING);
  void SetURI(nsIURI* aURI);
  void SetTime(float aValue);
  void Reset();

  virtual ~nsROCSSPrimitiveValue();

 protected:
  uint16_t mType;

  union {
    nscoord mAppUnits;
    float mFloat;
    int32_t mInt32;
    uint32_t mUint32;
    char16_t* mString;
    nsIURI* MOZ_OWNING_REF mURI;
    nsCSSKeyword mKeyword;
  } mValue;
};

inline nsROCSSPrimitiveValue* mozilla::dom::CSSValue::AsPrimitiveValue() {
  return CssValueType() == mozilla::dom::CSSValue::CSS_PRIMITIVE_VALUE
             ? static_cast<nsROCSSPrimitiveValue*>(this)
             : nullptr;
}

#endif /* nsROCSSPrimitiveValue_h___ */
