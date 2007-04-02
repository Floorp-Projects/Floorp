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
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

// NOTE: alphabetically ordered
#include "nsAccessibilityAtoms.h"
#include "nsBaseWidgetAccessible.h"
#include "nsIDOMXULDescriptionElement.h"
#include "nsINameSpaceManager.h"
#include "nsString.h"
#include "nsXULTextAccessible.h"

/**
  * For XUL descriptions and labels
  */
nsXULTextAccessible::nsXULTextAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsHyperTextAccessible(aDomNode, aShell)
{ 
}

/* wstring getName (); */
NS_IMETHODIMP nsXULTextAccessible::GetName(nsAString& aName)
{ 
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;  // Node shut down
  }
  if (!content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value,
                        aName)) {
    // if the value doesn't exist, flatten the inner content as the name (for descriptions)
    return AppendFlatStringFromSubtree(content, &aName);
  }
  // otherwise, use the value attribute as the name (for labels)
  return NS_OK;
}

NS_IMETHODIMP
nsXULTextAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Labels and description have read only state
  // They are not focusable or selectable
  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

/**
  * For XUL tooltip
  */
nsXULTooltipAccessible::nsXULTooltipAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsLeafAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsXULTooltipAccessible::GetName(nsAString& aName)
{
  return GetXULName(aName, PR_TRUE);
}

NS_IMETHODIMP
nsXULTooltipAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsLeafAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP nsXULTooltipAccessible::GetRole(PRUint32 *_retval)
{
  *_retval = nsIAccessibleRole::ROLE_TOOLTIP;
  return NS_OK;
}

/**
 * For XUL text links
 */
nsXULLinkAccessible::nsXULLinkAccessible(nsIDOMNode *aDomNode, nsIWeakReference *aShell):
nsLinkableAccessible(aDomNode, aShell)
{
}

NS_IMETHODIMP nsXULLinkAccessible::GetValue(nsAString& aValue)
{
  if (mIsLink) {
    mActionContent->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::href, aValue);
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsXULLinkAccessible::GetName(nsAString& aName)
{ 
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;  // Node shut down
  }
  if (!content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::value,
                        aName)) {
    // if the value doesn't exist, flatten the inner content as the name (for descriptions)
    return AppendFlatStringFromSubtree(content, &aName);
  }
  // otherwise, use the value attribute as the name (for labels)
  return NS_OK;
}

NS_IMETHODIMP nsXULLinkAccessible::GetRole(PRUint32 *aRole)
{
  if (mIsLink) {
    *aRole = nsIAccessibleRole::ROLE_LINK;
  } else {
    // default to calling the link a button; might have javascript
    *aRole = nsIAccessibleRole::ROLE_PUSHBUTTON;
  }
  // should there be a third case where it becomes just text?
  return NS_OK;
}

void nsXULLinkAccessible::CacheActionContent()
{
  // not a link if no content
  nsCOMPtr<nsIContent> mTempContent = do_QueryInterface(mDOMNode);
  if (!mTempContent) {
    return;
  }

  // not a link if there is no href attribute or not on a <link> tag
  if (mTempContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::href) ||
      mTempContent->Tag() == nsAccessibilityAtoms::link) {
    mIsLink = PR_TRUE;
    mActionContent = mTempContent;
  }
  else if (mTempContent->HasAttr(kNameSpaceID_None, nsAccessibilityAtoms::onclick)) {
    mIsOnclick = PR_TRUE;
    mActionContent = mTempContent;
  }
}
