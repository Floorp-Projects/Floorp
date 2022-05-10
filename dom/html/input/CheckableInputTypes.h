/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_CheckableInputTypes_h__
#define mozilla_dom_CheckableInputTypes_h__

#include "mozilla/dom/InputType.h"

namespace mozilla::dom {

class CheckableInputTypeBase : public InputType {
 public:
  ~CheckableInputTypeBase() override = default;

 protected:
  explicit CheckableInputTypeBase(HTMLInputElement* aInputElement)
      : InputType(aInputElement) {}
};

// input type=checkbox
class CheckboxInputType : public CheckableInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) CheckboxInputType(aInputElement);
  }

  bool IsValueMissing() const override;

  nsresult GetValueMissingMessage(nsAString& aMessage) override;

 private:
  explicit CheckboxInputType(HTMLInputElement* aInputElement)
      : CheckableInputTypeBase(aInputElement) {}
};

// input type=radio
class RadioInputType : public CheckableInputTypeBase {
 public:
  static InputType* Create(HTMLInputElement* aInputElement, void* aMemory) {
    return new (aMemory) RadioInputType(aInputElement);
  }

  nsresult GetValueMissingMessage(nsAString& aMessage) override;

 private:
  explicit RadioInputType(HTMLInputElement* aInputElement)
      : CheckableInputTypeBase(aInputElement) {}
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_CheckableInputTypes_h__ */
