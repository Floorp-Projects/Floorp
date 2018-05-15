/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* DOM object representing values in DOM computed style */

#ifndef mozilla_dom_CSSValue_h_
#define mozilla_dom_CSSValue_h_

#include "nsStringFwd.h"
#include "mozilla/RefCounted.h"

class nsROCSSPrimitiveValue;
namespace mozilla {
class ErrorResult;
} // namespace mozilla

namespace mozilla {
namespace dom {

/**
 * CSSValue - a DOM object representing values in DOM computed style.
 */
class CSSValue : public RefCounted<CSSValue>
{
public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(CSSValue);
  enum : uint16_t {
    CSS_INHERIT,
    CSS_PRIMITIVE_VALUE,
    CSS_VALUE_LIST,
    CSS_CUSTOM,
  };

  // CSSValue
  virtual void GetCssText(nsString& aText, mozilla::ErrorResult& aRv) = 0;
  virtual void SetCssText(const nsAString& aText, mozilla::ErrorResult& aRv) = 0;
  virtual uint16_t CssValueType() const = 0;

  virtual ~CSSValue() { };

  // Downcasting

  /**
   * Return this as a nsROCSSPrimitiveValue* if its a primitive value, and null
   * otherwise.
   *
   * Defined in nsROCSSPrimitiveValue.h.
   */
  inline nsROCSSPrimitiveValue* AsPrimitiveValue();
};

} // namespace dom
} // namespace mozilla

#endif
