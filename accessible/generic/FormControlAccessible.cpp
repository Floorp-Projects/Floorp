/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTE: alphabetically ordered

#include "FormControlAccessible.h"

#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/FloatingPoint.h"
#include "Role.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// CheckboxAccessible
////////////////////////////////////////////////////////////////////////////////

role CheckboxAccessible::NativeRole() const { return roles::CHECKBUTTON; }

uint8_t CheckboxAccessible::ActionCount() const { return 1; }

void CheckboxAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == eAction_Click) {
    uint64_t state = NativeState();
    if (state & states::CHECKED) {
      aName.AssignLiteral("uncheck");
    } else if (state & states::MIXED) {
      aName.AssignLiteral("cycle");
    } else {
      aName.AssignLiteral("check");
    }
  }
}

bool CheckboxAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != eAction_Click) {
    return false;
  }
  DoCommand();
  return true;
}

uint64_t CheckboxAccessible::NativeState() const {
  uint64_t state = LeafAccessible::NativeState();

  state |= states::CHECKABLE;
  dom::HTMLInputElement* input = dom::HTMLInputElement::FromNode(mContent);
  if (input) {  // HTML:input@type="checkbox"
    if (input->Indeterminate()) {
      return state | states::MIXED;
    }

    if (input->Checked()) {
      return state | states::CHECKED;
    }

  } else if (mContent->AsElement()->AttrValueIs(
                 kNameSpaceID_None, nsGkAtoms::checked, nsGkAtoms::_true,
                 eCaseMatters)) {  // XUL checkbox
    return state | states::CHECKED;
  }

  return state;
}

////////////////////////////////////////////////////////////////////////////////
// CheckboxAccessible: Widgets

bool CheckboxAccessible::IsWidget() const { return true; }

////////////////////////////////////////////////////////////////////////////////
// RadioButtonAccessible
////////////////////////////////////////////////////////////////////////////////

RadioButtonAccessible::RadioButtonAccessible(nsIContent* aContent,
                                             DocAccessible* aDoc)
    : LeafAccessible(aContent, aDoc) {}

uint8_t RadioButtonAccessible::ActionCount() const { return 1; }

void RadioButtonAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  if (aIndex == eAction_Click) aName.AssignLiteral("select");
}

bool RadioButtonAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != eAction_Click) return false;

  DoCommand();
  return true;
}

role RadioButtonAccessible::NativeRole() const { return roles::RADIOBUTTON; }

////////////////////////////////////////////////////////////////////////////////
// RadioButtonAccessible: Widgets

bool RadioButtonAccessible::IsWidget() const { return true; }
