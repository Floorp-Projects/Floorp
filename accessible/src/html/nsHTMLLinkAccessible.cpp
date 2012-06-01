/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsHTMLLinkAccessible.h"

#include "nsCoreUtils.h"
#include "DocAccessible.h"
#include "Role.h"
#include "States.h"

#include "nsContentUtils.h"
#include "nsEventStates.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLinkAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLLinkAccessible::
  nsHTMLLinkAccessible(nsIContent* aContent, DocAccessible* aDoc) :
  HyperTextAccessibleWrap(aContent, aDoc)
{
}

// Expose nsIAccessibleHyperLink unconditionally
NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLLinkAccessible, HyperTextAccessibleWrap,
                             nsIAccessibleHyperLink)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

role
nsHTMLLinkAccessible::NativeRole()
{
  return roles::LINK;
}

PRUint64
nsHTMLLinkAccessible::NativeState()
{
  PRUint64 states = HyperTextAccessibleWrap::NativeState();

  states  &= ~states::READONLY;

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::name)) {
    // This is how we indicate it is a named anchor
    // In other words, this anchor can be selected as a location :)
    // There is no other better state to use to indicate this.
    states |= states::SELECTABLE;
  }

  return states;
}

PRUint64
nsHTMLLinkAccessible::NativeLinkState() const
{
  nsEventStates eventState = mContent->AsElement()->State();
  if (eventState.HasState(NS_EVENT_STATE_UNVISITED))
    return states::LINKED;

  if (eventState.HasState(NS_EVENT_STATE_VISITED))
    return states::LINKED | states::TRAVERSED;

  // This is a either named anchor (a link with also a name attribute) or
  // it doesn't have any attributes. Check if 'click' event handler is
  // registered, otherwise bail out.
  return nsCoreUtils::HasClickListener(mContent) ? states::LINKED : 0;
}

void
nsHTMLLinkAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  HyperTextAccessible::Value(aValue);
  if (aValue.IsEmpty())
    nsContentUtils::GetLinkLocation(mContent->AsElement(), aValue);
}

PRUint8
nsHTMLLinkAccessible::ActionCount()
{
  return IsLinked() ? 1 : HyperTextAccessible::ActionCount();
}

NS_IMETHODIMP
nsHTMLLinkAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  if (!IsLinked())
    return HyperTextAccessible::GetActionName(aIndex, aName);

  // Action 0 (default action): Jump to link
  if (aIndex != eAction_Jump)
    return NS_ERROR_INVALID_ARG;

  aName.AssignLiteral("jump");
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkAccessible::DoAction(PRUint8 aIndex)
{
  if (!IsLinked())
    return HyperTextAccessible::DoAction(aIndex);

  // Action 0 (default action): Jump to link
  if (aIndex != eAction_Jump)
    return NS_ERROR_INVALID_ARG;

  if (IsDefunct())
    return NS_ERROR_FAILURE;

  DoCommand();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// HyperLinkAccessible

bool
nsHTMLLinkAccessible::IsLink()
{
  // Expose HyperLinkAccessible unconditionally.
  return true;
}

already_AddRefed<nsIURI>
nsHTMLLinkAccessible::AnchorURIAt(PRUint32 aAnchorIndex)
{
  return aAnchorIndex == 0 ? mContent->GetHrefURI() : nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

bool
nsHTMLLinkAccessible::IsLinked()
{
  if (IsDefunct())
    return false;

  nsEventStates state = mContent->AsElement()->State();
  return state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED |
                                     NS_EVENT_STATE_UNVISITED);
}
