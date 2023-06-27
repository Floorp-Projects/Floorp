/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing values in DOM computed style */

#ifndef nsROCSSPrimitiveValue_h___
#define nsROCSSPrimitiveValue_h___

#include "CSSValue.h"
#include "nsCoord.h"
#include "nsString.h"

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
    CSS_PX,
    CSS_DEG,
    CSS_S,
    CSS_STRING,
    CSS_RGBCOLOR,
    CSS_NUMBER_INT32,
    CSS_NUMBER_UINT32,
  };

  // CSSValue
  void GetCssText(nsAString&) final;
  uint16_t CssValueType() const final;

  // CSSPrimitiveValue
  uint16_t PrimitiveType();

  // nsROCSSPrimitiveValue
  nsROCSSPrimitiveValue();

  void SetNumber(float aValue);
  void SetNumber(int32_t aValue);
  void SetNumber(uint32_t aValue);
  void SetPercent(float aValue);
  void SetDegree(float aValue);
  void SetPixels(float aValue);
  void SetAppUnits(nscoord aValue);
  void SetAppUnits(float aValue);
  void SetString(const nsACString& aString);
  void SetString(const nsAString& aString);

  template <size_t N>
  void SetString(const char (&aString)[N]) {
    SetString(nsLiteralCString(aString));
  }

  void SetTime(float aValue);
  void Reset();

  virtual ~nsROCSSPrimitiveValue();

 protected:
  uint16_t mType;

  union {
    float mFloat;
    int32_t mInt32;
    uint32_t mUint32;
    char16_t* mString;
  } mValue;
};

inline nsROCSSPrimitiveValue* mozilla::dom::CSSValue::AsPrimitiveValue() {
  return CssValueType() == mozilla::dom::CSSValue::CSS_PRIMITIVE_VALUE
             ? static_cast<nsROCSSPrimitiveValue*>(this)
             : nullptr;
}

#endif /* nsROCSSPrimitiveValue_h___ */
