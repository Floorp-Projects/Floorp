/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef InputType_h__
#define InputType_h__

#include <stdint.h>
#include "mozilla/Decimal.h"
#include "mozilla/UniquePtr.h"
#include "nsString.h"
#include "nsError.h"

// This must come outside of any namespace, or else it won't overload with the
// double based version in nsMathUtils.h
inline mozilla::Decimal
NS_floorModulo(mozilla::Decimal x, mozilla::Decimal y)
{
  return (x - y * (x / y).floor());
}

namespace mozilla {
namespace dom {
class HTMLInputElement;
} // namespace dom
} // namespace mozilla

struct DoNotDelete;

/**
 * A common superclass for different types of a HTMLInputElement.
 */
class InputType
{
public:
  static mozilla::UniquePtr<InputType, DoNotDelete>
  Create(mozilla::dom::HTMLInputElement* aInputElement, uint8_t aType,
         void* aMemory);

  virtual ~InputType() {}

  // Float value returned by GetStep() when the step attribute is set to 'any'.
  static const mozilla::Decimal kStepAny;

  /**
   * Drop the reference to the input element.
   */
  void DropReference();

  virtual bool IsTooLong() const;
  virtual bool IsTooShort() const;
  virtual bool IsValueMissing() const;
  virtual bool HasTypeMismatch() const;
  virtual bool HasPatternMismatch() const;
  virtual bool IsRangeOverflow() const;
  virtual bool IsRangeUnderflow() const;
  virtual bool HasStepMismatch(bool aUseZeroIfValueNaN) const;

  virtual nsresult MinMaxStepAttrChanged();

protected:
  explicit InputType(mozilla::dom::HTMLInputElement* aInputElement)
    : mInputElement(aInputElement)
  {}

  /**
   * Get the mutable state of the element.
   * When the element isn't mutable (immutable), the value or checkedness
   * should not be changed by the user.
   *
   * See: https://html.spec.whatwg.org/multipage/forms.html#the-input-element:concept-fe-mutable
   */
  virtual bool IsMutable() const;

  /**
   * Returns whether the input element's current value is the empty string.
   * This only makes sense for some input types; does NOT make sense for file
   * inputs.
   *
   * @return whether the input element's current value is the empty string.
   */
  bool IsValueEmpty() const;

  // A getter for callers that know we're not dealing with a file input, so they
  // don't have to think about the caller type.
  void GetNonFileValueInternal(nsAString& aValue) const;

  /**
   * Setting the input element's value.
   *
   * @param aValue      String to set.
   * @param aFlags      See nsTextEditorState::SetValueFlags.
   */
  nsresult SetValueInternal(const nsAString& aValue, uint32_t aFlags);

  /**
   * Return the base used to compute if a value matches step.
   * Basically, it's the min attribute if present and a default value otherwise.
   *
   * @return The step base.
   */
  mozilla::Decimal GetStepBase() const;

  mozilla::dom::HTMLInputElement* mInputElement;
};

// Custom deleter for UniquePtr<InputType> to avoid freeing memory pre-allocated
// for InputType, but we still need to call the destructor explictly.
struct DoNotDelete { void operator()(::InputType* p) { p->~InputType(); } };

#endif /* InputType_h__ */
