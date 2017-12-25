/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HTMLLinkAccessible.h"

#include "nsCoreUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"

#include "nsContentUtils.h"
#include "mozilla/EventStates.h"
#include "mozilla/dom/Element.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// HTMLLinkAccessible
////////////////////////////////////////////////////////////////////////////////

HTMLLinkAccessible::
  HTMLLinkAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(HTMLLinkAccessible, HyperTextAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

role
HTMLLinkAccessible::NativeRole()
{
  return roles::LINK;
}

uint64_t
HTMLLinkAccessible::NativeState()
{
  return HyperTextAccessibleWrap::NativeState() & ~states::READONLY;
}

uint64_t
HTMLLinkAccessible::NativeLinkState() const
{
  EventStates eventState = mContent->AsElement()->State();
  if (eventState.HasState(NS_EVENT_STATE_UNVISITED))
    return states::LINKED;

  if (eventState.HasState(NS_EVENT_STATE_VISITED))
    return states::LINKED | states::TRAVERSED;

  // This is a either named anchor (a link with also a name attribute) or
  // it doesn't have any attributes. Check if 'click' event handler is
  // registered, otherwise bail out.
  return nsCoreUtils::HasClickListener(mContent) ? states::LINKED : 0;
}

uint64_t
HTMLLinkAccessible::NativeInteractiveState() const
{
  uint64_t state = HyperTextAccessibleWrap::NativeInteractiveState();

  // This is how we indicate it is a named anchor. In other words, this anchor
  // can be selected as a location :) There is no other better state to use to
  // indicate this.
  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::name))
    state |= states::SELECTABLE;

  return state;
}

void
HTMLLinkAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  HyperTextAccessible::Value(aValue);
  if (aValue.IsEmpty())
    nsContentUtils::GetLinkLocation(mContent->AsElement(), aValue);
}

uint8_t
HTMLLinkAccessible::ActionCount()
{
  return IsLinked() ? 1 : HyperTextAccessible::ActionCount();
}

void
HTMLLinkAccessible::ActionNameAt(uint8_t aIndex, nsAString& aName)
{
  aName.Truncate();

  if (!IsLinked()) {
    HyperTextAccessible::ActionNameAt(aIndex, aName);
    return;
  }

  // Action 0 (default action): Jump to link
  if (aIndex == eAction_Jump)
    aName.AssignLiteral("jump");
}

bool
HTMLLinkAccessible::DoAction(uint8_t aIndex)
{
  if (!IsLinked())
    return HyperTextAccessible::DoAction(aIndex);

  // Action 0 (default action): Jump to link
  if (aIndex != eAction_Jump)
    return false;

  DoCommand();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// HyperLinkAccessible

bool
HTMLLinkAccessible::IsLink()
{
  // Expose HyperLinkAccessible unconditionally.
  return true;
}

already_AddRefed<nsIURI>
HTMLLinkAccessible::AnchorURIAt(uint32_t aAnchorIndex)
{
  return aAnchorIndex == 0 ? mContent->GetHrefURI() : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

bool
HTMLLinkAccessible::IsLinked() const
{
  EventStates state = mContent->AsElement()->State();
  return state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED |
                                     NS_EVENT_STATE_UNVISITED);
}
