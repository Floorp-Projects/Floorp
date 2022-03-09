/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseAccessibles.h"

#include "LocalAccessible-inl.h"
#include "HyperTextAccessibleWrap.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "Role.h"
#include "States.h"
#include "nsIURI.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// LeafAccessible
////////////////////////////////////////////////////////////////////////////////

LeafAccessible::LeafAccessible(nsIContent* aContent, DocAccessible* aDoc)
    : AccessibleWrap(aContent, aDoc) {
  mStateFlags |= eNoKidsFromDOM;
}

////////////////////////////////////////////////////////////////////////////////
// LeafAccessible: LocalAccessible public

LocalAccessible* LeafAccessible::LocalChildAtPoint(
    int32_t aX, int32_t aY, EWhichChildAtPoint aWhichChild) {
  // Don't walk into leaf accessibles.
  return this;
}

bool LeafAccessible::InsertChildAt(uint32_t aIndex, LocalAccessible* aChild) {
  MOZ_ASSERT_UNREACHABLE("InsertChildAt called on leaf accessible!");
  return false;
}

bool LeafAccessible::RemoveChild(LocalAccessible* aChild) {
  MOZ_ASSERT_UNREACHABLE("RemoveChild called on leaf accessible!");
  return false;
}

bool LeafAccessible::IsAcceptableChild(nsIContent* aEl) const {
  // No children for leaf accessible.
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible. nsIAccessible

void LinkableAccessible::TakeFocus() const {
  if (const LocalAccessible* actionAcc = ActionWalk()) {
    actionAcc->TakeFocus();
  } else {
    AccessibleWrap::TakeFocus();
  }
}

uint64_t LinkableAccessible::NativeLinkState() const {
  bool isLink;
  const LocalAccessible* actionAcc = ActionWalk(&isLink);
  if (isLink) {
    return states::LINKED | (actionAcc->LinkState() & states::TRAVERSED);
  }

  return 0;
}

void LinkableAccessible::Value(nsString& aValue) const {
  aValue.Truncate();

  LocalAccessible::Value(aValue);
  if (!aValue.IsEmpty()) {
    return;
  }

  bool isLink;
  const LocalAccessible* actionAcc = ActionWalk(&isLink);
  if (isLink) {
    actionAcc->Value(aValue);
  }
}

uint8_t LinkableAccessible::ActionCount() const {
  bool isLink, isOnclick;
  ActionWalk(&isLink, &isOnclick);
  return (isLink || isOnclick) ? 1 : 0;
}

const LocalAccessible* LinkableAccessible::ActionWalk(bool* aIsLink,
                                                      bool* aIsOnclick) const {
  if (aIsOnclick) {
    *aIsOnclick = false;
  }
  if (aIsLink) {
    *aIsLink = false;
  }

  if (HasPrimaryAction()) {
    if (aIsOnclick) {
      *aIsOnclick = true;
    }

    return nullptr;
  }

  const Accessible* actionAcc = ActionAncestor();

  const LocalAccessible* localAction =
      actionAcc ? const_cast<Accessible*>(actionAcc)->AsLocal() : nullptr;

  if (!localAction) {
    return nullptr;
  }

  if (localAction->LinkState() & states::LINKED) {
    if (aIsLink) {
      *aIsLink = true;
    }
  } else if (aIsOnclick) {
    *aIsOnclick = true;
  }

  return localAction;
}

void LinkableAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName) {
  aName.Truncate();

  // Action 0 (default action): Jump to link
  if (aIndex == eAction_Jump) {
    bool isOnclick, isLink;
    ActionWalk(&isLink, &isOnclick);
    if (isLink) {
      aName.AssignLiteral("jump");
    } else if (isOnclick) {
      aName.AssignLiteral("click");
    }
  }
}

bool LinkableAccessible::DoAction(uint8_t aIndex) const {
  if (aIndex != eAction_Jump) {
    return false;
  }

  if (const LocalAccessible* actionAcc = ActionWalk()) {
    return actionAcc->DoAction(aIndex);
  }

  return AccessibleWrap::DoAction(aIndex);
}

KeyBinding LinkableAccessible::AccessKey() const {
  if (const LocalAccessible* actionAcc =
          const_cast<LinkableAccessible*>(this)->ActionWalk()) {
    return actionAcc->AccessKey();
  }

  return LocalAccessible::AccessKey();
}

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible: HyperLinkAccessible

already_AddRefed<nsIURI> LinkableAccessible::AnchorURIAt(
    uint32_t aAnchorIndex) const {
  bool isLink;
  const LocalAccessible* actionAcc = ActionWalk(&isLink);
  if (isLink) {
    NS_ASSERTION(actionAcc->IsLink(), "HyperLink isn't implemented.");

    if (actionAcc->IsLink()) {
      return actionAcc->AnchorURIAt(aAnchorIndex);
    }
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// DummyAccessible
////////////////////////////////////////////////////////////////////////////////

uint64_t DummyAccessible::NativeState() const { return 0; }
uint64_t DummyAccessible::NativeInteractiveState() const { return 0; }

uint64_t DummyAccessible::NativeLinkState() const { return 0; }

bool DummyAccessible::NativelyUnavailable() const { return false; }

void DummyAccessible::ApplyARIAState(uint64_t* aState) const {}
