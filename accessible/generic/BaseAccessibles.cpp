/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BaseAccessibles.h"

#include "Accessible-inl.h"
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

LeafAccessible::
  LeafAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  AccessibleWrap(aContent, aDoc)
{
  mStateFlags |= eNoKidsFromDOM;
}

NS_IMPL_ISUPPORTS_INHERITED0(LeafAccessible, Accessible)

////////////////////////////////////////////////////////////////////////////////
// LeafAccessible: Accessible public

Accessible*
LeafAccessible::ChildAtPoint(int32_t aX, int32_t aY,
                             EWhichChildAtPoint aWhichChild)
{
  // Don't walk into leaf accessibles.
  return this;
}

bool
LeafAccessible::InsertChildAt(uint32_t aIndex, Accessible* aChild)
{
  NS_NOTREACHED("InsertChildAt called on leaf accessible!");
  return false;
}

bool
LeafAccessible::RemoveChild(Accessible* aChild)
{
  NS_NOTREACHED("RemoveChild called on leaf accessible!");
  return false;
}

bool
LeafAccessible::IsAcceptableChild(nsIContent* aEl) const
{
  // No children for leaf accessible.
  return false;
}


////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible
////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED0(LinkableAccessible, AccessibleWrap)

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible. nsIAccessible

void
LinkableAccessible::TakeFocus()
{
  if (Accessible* actionAcc = ActionWalk()) {
    actionAcc->TakeFocus();
  } else {
    AccessibleWrap::TakeFocus();
  }
}

uint64_t
LinkableAccessible::NativeLinkState() const
{
  bool isLink;
  Accessible* actionAcc =
    const_cast<LinkableAccessible*>(this)->ActionWalk(&isLink);
  if (isLink) {
    return states::LINKED | (actionAcc->LinkState() & states::TRAVERSED);
  }

  return 0;
}

void
LinkableAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  Accessible::Value(aValue);
  if (!aValue.IsEmpty()) {
    return;
  }

  bool isLink;
  Accessible* actionAcc = ActionWalk(&isLink);
  if (isLink) {
    actionAcc->Value(aValue);
  }
}

uint8_t
LinkableAccessible::ActionCount()
{
  bool isLink, isOnclick, isLabelWithControl;
  ActionWalk(&isLink, &isOnclick, &isLabelWithControl);
  return (isLink || isOnclick || isLabelWithControl) ? 1 : 0;
}

Accessible*
LinkableAccessible::ActionWalk(bool* aIsLink, bool* aIsOnclick,
                               bool* aIsLabelWithControl)
{
  if (aIsOnclick) {
    *aIsOnclick = false;
  }
  if (aIsLink) {
    *aIsLink = false;
  }
  if (aIsLabelWithControl) {
    *aIsLabelWithControl = false;
  }

  if (nsCoreUtils::HasClickListener(mContent)) {
    if (aIsOnclick) {
      *aIsOnclick = true;
    }
    return nullptr;
  }

  // XXX: The logic looks broken since the click listener may be registered
  // on non accessible node in parent chain but this node is skipped when tree
  // is traversed.
  Accessible* walkUpAcc = this;
  while ((walkUpAcc = walkUpAcc->Parent()) && !walkUpAcc->IsDoc()) {
    if (walkUpAcc->LinkState() & states::LINKED) {
      if (aIsLink) {
        *aIsLink = true;
      }
      return walkUpAcc;
    }

    if (nsCoreUtils::HasClickListener(walkUpAcc->GetContent())) {
      if (aIsOnclick) {
        *aIsOnclick = true;
      }
      return walkUpAcc;
    }

    if (nsCoreUtils::IsLabelWithControl(walkUpAcc->GetContent())) {
      if (aIsLabelWithControl) {
        *aIsLabelWithControl = true;
      }
      return walkUpAcc;
    }
  }
  return nullptr;
}

void
LinkableAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();

  // Action 0 (default action): Jump to link
  if (aIndex == eAction_Jump) {
    bool isOnclick, isLink, isLabelWithControl;
    ActionWalk(&isLink, &isOnclick, &isLabelWithControl);
    if (isLink) {
      aName.AssignLiteral("jump");
    } else if (isOnclick || isLabelWithControl) {
      aName.AssignLiteral("click");
    }
  }
}

bool
LinkableAccessible::DoAction(uint8_t aIndex)
{
  if (aIndex != eAction_Jump) {
    return false;
  }

  if (Accessible* actionAcc = ActionWalk()) {
    return actionAcc->DoAction(aIndex);
  }

  return AccessibleWrap::DoAction(aIndex);
}

KeyBinding
LinkableAccessible::AccessKey() const
{
  if (const Accessible* actionAcc =
    const_cast<LinkableAccessible*>(this)->ActionWalk()) {
    return actionAcc->AccessKey();
  }

  return Accessible::AccessKey();
}

////////////////////////////////////////////////////////////////////////////////
// LinkableAccessible: HyperLinkAccessible

already_AddRefed<nsIURI>
LinkableAccessible::AnchorURIAt(uint32_t aAnchorIndex)
{
  bool isLink;
  Accessible* actionAcc = ActionWalk(&isLink);
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

uint64_t
DummyAccessible::NativeState()
{
  return 0;
}
uint64_t
DummyAccessible::NativeInteractiveState() const
{
  return 0;
}

uint64_t
DummyAccessible::NativeLinkState() const
{
  return 0;
}

bool
DummyAccessible::NativelyUnavailable() const
{
  return false;
}

void
DummyAccessible::ApplyARIAState(uint64_t* aState) const
{
}
