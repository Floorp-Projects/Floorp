/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_NumericInputTypes_h__
#define mozilla_dom_NumericInputTypes_h__

#include "mozilla/dom/InputType.h"

namespace mozilla::dom {

class NumericInputTypeBase : public InputType {
 public:
  ~NumericInputTypeBase() override = default;

  bool IsRangeOverflow() const override;
  bool IsRangeUnderflow() const override;
  bool HasStepMismatch() const override;

  nsresult GetRangeOverflowMessage(nsAString& aMessage) override;
  nsresult GetRangeUnderflowMessage(nsAString& aMessage) override;

  StringToNumberResult ConvertStringToNumber(
      const nsAString& aValue) const override;
  bool ConvertNumberToString(Decimal aValue,
                             nsAString& aResultString) const override;

 protected:
  explicit NumericInputTypeBase(HTMLInputElement* aInputElement)
      : InputType(aInputElement) {}
};

// input type=number
class NumberInputType final : public NumericInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) NumberInputType(aInputElement);
  }

  bool IsValueMissing() const override;
  bool HasBadInput() const override;

  nsresult GetValueMissingMessage(nsAString& aMessage) override;
  nsresult GetBadInputMessage(nsAString& aMessage) override;

  StringToNumberResult ConvertStringToNumber(const nsAString&) const override;
  bool ConvertNumberToString(Decimal aValue,
                             nsAString& aResultString) const override;

 protected:
  bool IsMutable() const override;

 private:
  explicit NumberInputType(HTMLInputElement* aInputElement)
      : NumericInputTypeBase(aInputElement) {}
};

// input type=range
class RangeInputType : public NumericInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) RangeInputType(aInputElement);
  }

  MOZ_CAN_RUN_SCRIPT void MinMaxStepAttrChanged() override;

 private:
  explicit RangeInputType(HTMLInputElement* aInputElement)
      : NumericInputTypeBase(aInputElement) {}
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_NumericInputTypes_h__ */
