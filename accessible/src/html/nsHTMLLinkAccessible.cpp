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

#include "nsIEventStateManager.h"

////////////////////////////////////////////////////////////////////////////////
// nsHTMLLinkAccessible
////////////////////////////////////////////////////////////////////////////////

nsHTMLLinkAccessible::
  nsHTMLLinkAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsHyperTextAccessibleWrap(aContent, aShell)
{
}

// Expose nsIAccessibleHyperLink unconditionally
NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLLinkAccessible, nsHyperTextAccessibleWrap,
                             nsIAccessibleHyperLink)

////////////////////////////////////////////////////////////////////////////////
// nsIAccessible

PRUint32
nsHTMLLinkAccessible::NativeRole()
{
  return nsIAccessibleRole::ROLE_LINK;
}

nsresult
nsHTMLLinkAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessibleWrap::GetStateInternal(aState,
                                                            aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState  &= ~nsIAccessibleStates::STATE_READONLY;

  if (mContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::name)) {
    // This is how we indicate it is a named anchor
    // In other words, this anchor can be selected as a location :)
    // There is no other better state to use to indicate this.
    *aState |= nsIAccessibleStates::STATE_SELECTABLE;
  }

  nsEventStates state = mContent->IntrinsicState();
  if (state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED |
                                  NS_EVENT_STATE_UNVISITED)) {
    *aState |= nsIAccessibleStates::STATE_LINKED;

    if (state.HasState(NS_EVENT_STATE_VISITED))
      *aState |= nsIAccessibleStates::STATE_TRAVERSED;

    return NS_OK;
  }

  // This is a either named anchor (a link with also a name attribute) or
  // it doesn't have any attributes. Check if 'click' event handler is
  // registered, otherwise bail out.
  if (nsCoreUtils::HasClickListener(mContent))
    *aState |= nsIAccessibleStates::STATE_LINKED;

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLinkAccessible::GetValue(nsAString& aValue)
{
  aValue.Truncate();

  nsresult rv = nsHyperTextAccessible::GetValue(aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!aValue.IsEmpty())
    return NS_OK;
  
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  nsCOMPtr<nsIDOMNode> DOMNode(do_QueryInterface(mContent));
  return presShell->GetLinkLocation(DOMNode, aValue);
}

NS_IMETHODIMP
nsHTMLLinkAccessible::GetNumActions(PRUint8 *aNumActions)
{
  NS_ENSURE_ARG_POINTER(aNumActions);

  if (!IsLinked())
    return nsHyperTextAccessible::GetNumActions(aNumActions);

  *aNumActions = 1;
  return NS_OK;
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
nsHTMLLinkAccessible::IsHyperLink()
{
  // Expose HyperLinkAccessible unconditionally.
  return true;
}

already_AddRefed<nsIURI>
nsHTMLLinkAccessible::GetAnchorURI(PRUint32 aAnchorIndex)
{
  return aAnchorIndex == 0 ? mContent->GetHrefURI() : nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// Protected members

PRBool
nsHTMLLinkAccessible::IsLinked()
{
  if (IsDefunct())
    return PR_FALSE;

  nsEventStates state = mContent->IntrinsicState();
  return state.HasAtLeastOneOfStates(NS_EVENT_STATE_VISITED |
                                     NS_EVENT_STATE_UNVISITED);
}
