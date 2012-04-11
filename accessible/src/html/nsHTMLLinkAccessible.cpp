/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Leventhal <aleventh@us.ibm.com> (original author)
 *   Alexander Surkov <surkov.alexander@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHTMLLinkAccessible.h"

#include "nsCoreUtils.h"
#include "Role.h"
#include "States.h"

#include "nsDocAccessible.h"
#include "nsEventStates.h"
#include "mozilla/dom/Element.h"

using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLinkAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLLinkAccessible::
  nsHTMLLinkAccessible(nsIContent* aContent, nsDocAccessible* aDoc) :
  nsHyperTextAccessibleWrap(aContent, aDoc)
{
}

// Expose nsIAccessibleHyperLink unconditionally
NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLLinkAccessible, nsHyperTextAccessibleWrap,
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
  PRUint64 states = nsHyperTextAccessibleWrap::NativeState();

  states  &= ~states::READONLY;

  if (mContent->HasAttr(kNameSpaceID_None, nsGkAtoms::name)) {
    // This is how we indicate it is a named anchor
    // In other words, this anchor can be selected as a location :)
    // There is no other better state to use to indicate this.
    states |= states::SELECTABLE;
  }

  nsEventStates state = mContent->AsElement()->State();
  if (state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED |
                                  NS_EVENT_STATE_UNVISITED)) {
    states |= states::LINKED;

    if (state.HasState(NS_EVENT_STATE_VISITED))
      states |= states::TRAVERSED;

    return states;
  }

  // This is a either named anchor (a link with also a name attribute) or
  // it doesn't have any attributes. Check if 'click' event handler is
  // registered, otherwise bail out.
  if (nsCoreUtils::HasClickListener(mContent))
    states |= states::LINKED;

  return states;
}

void
nsHTMLLinkAccessible::Value(nsString& aValue)
{
  aValue.Truncate();

  nsHyperTextAccessible::Value(aValue);
  if (!aValue.IsEmpty())
    return;
  
  nsIPresShell* presShell(mDoc->PresShell());
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  presShell->GetLinkLocation(DOMNode, aValue);
}

PRUint8
nsHTMLLinkAccessible::ActionCount()
{
  return IsLinked() ? 1 : nsHyperTextAccessible::ActionCount();
}

NS_IMETHODIMP
nsHTMLLinkAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  if (!IsLinked())
    return nsHyperTextAccessible::GetActionName(aIndex, aName);

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
    return nsHyperTextAccessible::DoAction(aIndex);

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
