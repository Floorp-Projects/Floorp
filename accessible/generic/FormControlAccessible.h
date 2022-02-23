/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_A11Y_FormControlAccessible_H_
#define MOZILLA_A11Y_FormControlAccessible_H_

#include "BaseAccessibles.h"

namespace mozilla {
namespace a11y {

/**
 * Checkbox accessible.
 */
class CheckboxAccessible : public LeafAccessible {
 public:
  enum { eAction_Click = 0 };

  CheckboxAccessible(nsIContent* aContent, DocAccessible* aDoc)
      : LeafAccessible(aContent, aDoc) {
    // Ignore "CheckboxStateChange" DOM event in lieu of document observer
    // state change notification.
    if (aContent->IsHTMLElement()) {
      mStateFlags |= eIgnoreDOMUIEvent;
    }
  }

  // LocalAccessible
  virtual mozilla::a11y::role NativeRole() const override;
  virtual uint64_t NativeState() const override;

  // ActionAccessible
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool HasPrimaryAction() const override;

  // Widgets
  virtual bool IsWidget() const override;
};

/**
 * Generic class used for radio buttons.
 */
class RadioButtonAccessible : public LeafAccessible {
 public:
  RadioButtonAccessible(nsIContent* aContent, DocAccessible* aDoc);

  // LocalAccessible
  virtual mozilla::a11y::role NativeRole() const override;

  // ActionAccessible
  virtual void ActionNameAt(uint8_t aIndex, nsAString& aName) override;
  virtual bool HasPrimaryAction() const override;

  enum { eAction_Click = 0 };

  // Widgets
  virtual bool IsWidget() const override;
};

}  // namespace a11y
}  // namespace mozilla

#endif
