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
 *   John Gaunt (jgaunt@netscape.com)
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

#include "nsBaseWidgetAccessible.h"

#include "nsAccessibilityAtoms.h"
#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsHyperTextAccessibleWrap.h"

#include "nsIDOMNSHTMLElement.h"
#include "nsGUIEvent.h"
#include "nsILink.h"
#include "nsIFrame.h"
#include "nsINameSpaceManager.h"
#include "nsIURI.h"

////////////////////////////////////////////////////////////////////////////////
// nsLeafAccessible
////////////////////////////////////////////////////////////////////////////////

nsLeafAccessible::
  nsLeafAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsLeafAccessible, nsAccessible)

////////////////////////////////////////////////////////////////////////////////
// nsLeafAccessible: nsAccessible public

nsresult
nsLeafAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                  PRBool aDeepestChild,
                                  nsIAccessible **aChild)
{
  // Don't walk into leaf accessibles.
  NS_ADDREF(*aChild = this);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsLeafAccessible: nsAccessible private

void
nsLeafAccessible::CacheChildren()
{
  // No children for leaf accessible.
}


////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible
////////////////////////////////////////////////////////////////////////////////

nsLinkableAccessible::
  nsLinkableAccessible(nsIContent *aContent, nsIWeakReference *aShell) :
  nsAccessibleWrap(aContent, aShell),
  mActionContent(nsnull),
  mIsLink(PR_FALSE),
  mIsOnclick(PR_FALSE)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsLinkableAccessible, nsAccessibleWrap)

////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible. nsIAccessible

NS_IMETHODIMP
nsLinkableAccessible::TakeFocus()
{
  nsAccessible *actionAcc = GetActionAccessible();
  if (actionAcc)
    return actionAcc->TakeFocus();

  return nsAccessibleWrap::TakeFocus();
}

nsresult
nsLinkableAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsAccessibleWrap::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  if (mIsLink) {
    *aState |= nsIAccessibleStates::STATE_LINKED;
    nsAccessible *actionAcc = GetActionAccessible();
    if (nsAccUtils::State(actionAcc) & nsIAccessibleStates::STATE_TRAVERSED)
      *aState |= nsIAccessibleStates::STATE_TRAVERSED;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLinkableAccessible::GetValue(nsAString& aValue)
{
  aValue.Truncate();

  nsAccessible::GetValue(aValue);
  if (!aValue.IsEmpty())
    return NS_OK;

  if (mIsLink) {
    nsAccessible *actionAcc = GetActionAccessible();
    if (actionAcc)
      return actionAcc->GetValue(aValue);
  }

  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsLinkableAccessible::GetNumActions(PRUint8 *aNumActions)
{
  NS_ENSURE_ARG_POINTER(aNumActions);

  *aNumActions = mActionContent ? 1 : 0;
  return NS_OK;
}

NS_IMETHODIMP
nsLinkableAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  // Action 0 (default action): Jump to link
  if (aIndex == eAction_Jump) {   
    if (mIsLink) {
      aName.AssignLiteral("jump");
      return NS_OK;
    }
    else if (mIsOnclick) {
      aName.AssignLiteral("click");
      return NS_OK;
    }
    return NS_ERROR_NOT_IMPLEMENTED;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsLinkableAccessible::DoAction(PRUint8 aIndex)
{
  if (aIndex != eAction_Jump)
    return NS_ERROR_INVALID_ARG;

  nsAccessible *actionAcc = GetActionAccessible();
  if (actionAcc)
    return actionAcc->DoAction(aIndex);
  
  return nsAccessibleWrap::DoAction(aIndex);
}

NS_IMETHODIMP
nsLinkableAccessible::GetKeyboardShortcut(nsAString& aKeyboardShortcut)
{
  aKeyboardShortcut.Truncate();

  nsAccessible *actionAcc = GetActionAccessible();
  if (actionAcc)
    return actionAcc->GetKeyboardShortcut(aKeyboardShortcut);

  return nsAccessible::GetKeyboardShortcut(aKeyboardShortcut);
}

////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible. nsAccessNode

PRBool
nsLinkableAccessible::Init()
{
  CacheActionContent();
  return nsAccessibleWrap::Init();
}

void
nsLinkableAccessible::Shutdown()
{
  mActionContent = nsnull;
  nsAccessibleWrap::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible: HyperLinkAccessible

already_AddRefed<nsIURI>
nsLinkableAccessible::GetAnchorURI(PRUint32 aAnchorIndex)
{
  if (mIsLink) {
    nsAccessible* link = GetActionAccessible();
    if (link) {
      NS_ASSERTION(link->IsHyperLink(),
                   "nsIAccessibleHyperLink isn't implemented.");

      if (link->IsHyperLink())
        return link->GetAnchorURI(aAnchorIndex);
    }
  }

  return nsnull;
}

////////////////////////////////////////////////////////////////////////////////
// nsLinkableAccessible

void
nsLinkableAccessible::CacheActionContent()
{
  nsIContent* walkUpContent = mContent;
  PRBool isOnclick = nsCoreUtils::HasClickListener(walkUpContent);

  if (isOnclick) {
    mActionContent = walkUpContent;
    mIsOnclick = PR_TRUE;
    return;
  }

  while ((walkUpContent = walkUpContent->GetParent())) {
    nsAccessible* walkUpAcc =
      GetAccService()->GetAccessibleInWeakShell(walkUpContent, mWeakShell);

    if (walkUpAcc && walkUpAcc->Role() == nsIAccessibleRole::ROLE_LINK &&
        nsAccUtils::State(walkUpAcc) & nsIAccessibleStates::STATE_LINKED) {
      mIsLink = PR_TRUE;
      mActionContent = walkUpContent;
      return;
    }

    isOnclick = nsCoreUtils::HasClickListener(walkUpContent);
    if (isOnclick) {
      mActionContent = walkUpContent;
      mIsOnclick = PR_TRUE;
      return;
    }
  }
}

nsAccessible *
nsLinkableAccessible::GetActionAccessible() const
{
  // Return accessible for the action content if it's different from node of
  // this accessible. If the action accessible is not null then it is used to
  // redirect methods calls otherwise we use method implementation from the
  // base class.
  if (!mActionContent || mContent == mActionContent)
    return nsnull;

  return GetAccService()->GetAccessibleInWeakShell(mActionContent, mWeakShell);
}

////////////////////////////////////////////////////////////////////////////////
// nsEnumRoleAccessible
////////////////////////////////////////////////////////////////////////////////

nsEnumRoleAccessible::
  nsEnumRoleAccessible(nsIContent *aNode, nsIWeakReference *aShell,
                       PRUint32 aRole) :
  nsAccessibleWrap(aNode, aShell), mRole(aRole)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsEnumRoleAccessible, nsAccessible)

PRUint32
nsEnumRoleAccessible::NativeRole()
{
  return mRole;
}
