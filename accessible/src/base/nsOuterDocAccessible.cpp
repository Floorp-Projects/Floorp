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
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsOuterDocAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIAccessibleDocument.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIServiceManager.h"
#include "nsIContent.h"

NS_IMPL_ISUPPORTS_INHERITED0(nsOuterDocAccessible, nsAccessible)

nsOuterDocAccessible::nsOuterDocAccessible(nsIDOMNode* aNode, 
                                           nsIWeakReference* aShell):
  nsAccessibleWrap(aNode, aShell)
{
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsOuterDocAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_INTERNAL_FRAME;
  return NS_OK;
}

nsresult
nsOuterDocAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  return NS_OK;
}

// nsIAccessible::getChildAtPoint(in long x, in long y)
NS_IMETHODIMP
nsOuterDocAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                      nsIAccessible **aAccessible)
{
  NS_ENSURE_ARG_POINTER(aAccessible);
  *aAccessible = nsnull;
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }
  PRInt32 docX, docY, docWidth, docHeight;
  GetBounds(&docX, &docY, &docWidth, &docHeight);
  if (aX < docX || aX >= docX + docWidth || aY < docY || aY >= docY + docHeight) {
    return NS_ERROR_FAILURE;
  }

  return GetFirstChild(aAccessible);  // Always return the inner doc unless bounds outside of it
}

// nsIAccessible::getDeepestChildAtPoint(in long x, in long y)
NS_IMETHODIMP
nsOuterDocAccessible::GetDeepestChildAtPoint(PRInt32 aX, PRInt32 aY,
                                      nsIAccessible **aAccessible)
{
  // Call getDeepestChildAtPoint on the fist child accessible of the outer
  // document accessible if the given point is inside of outer document.
  nsCOMPtr<nsIAccessible> childAcc;
  nsresult rv = GetChildAtPoint(aX, aY, getter_AddRefs(childAcc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!childAcc)
    return NS_OK;

  return childAcc->GetDeepestChildAtPoint(aX, aY, aAccessible);
}

void nsOuterDocAccessible::CacheChildren()
{  
  // An outer doc accessible usually has 1 nsDocAccessible child,
  // but could have none if we can't get to the inner documnet
  if (!mWeakShell) {
    mAccChildCount = eChildCountUninitialized;
    return;   // This outer doc node has been shut down
  }
  if (mAccChildCount != eChildCountUninitialized) {
    return;
  }

  InvalidateChildren();
  mAccChildCount = 0;

  // In these variable names, "outer" relates to the nsOuterDocAccessible
  // as opposed to the nsDocAccessibleWrap which is "inner".
  // The outer node is a something like a <browser>, <frame>, <iframe>, <page> or
  // <editor> tag, whereas the inner node corresponds to the inner document root.

  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ASSERTION(content, "No nsIContent for <browser>/<iframe>/<editor> dom node");

  nsCOMPtr<nsIDocument> outerDoc = content->GetDocument();
  if (!outerDoc) {
    return;
  }

  nsIDocument *innerDoc = outerDoc->GetSubDocumentFor(content);
  nsCOMPtr<nsIDOMNode> innerNode(do_QueryInterface(innerDoc));
  if (!innerNode) {
    return;
  }

  nsCOMPtr<nsIAccessible> innerAccessible;
  nsCOMPtr<nsIAccessibilityService> accService = 
    do_GetService("@mozilla.org/accessibilityService;1");
  accService->GetAccessibleFor(innerNode, getter_AddRefs(innerAccessible));
  nsCOMPtr<nsPIAccessible> privateInnerAccessible = 
    do_QueryInterface(innerAccessible);
  if (!privateInnerAccessible) {
    return;
  }

  // Success getting inner document as first child -- now we cache it.
  mAccChildCount = 1;
  SetFirstChild(innerAccessible); // weak ref
  privateInnerAccessible->SetParent(this);
  privateInnerAccessible->SetNextSibling(nsnull);
}

nsresult
nsOuterDocAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  nsAutoString tag;
  aAttributes->GetStringProperty(NS_LITERAL_CSTRING("tag"), tag);
  if (!tag.IsEmpty()) {
    // We're overriding the ARIA attributes on an sub document, but we don't want to
    // override the other attributes
    return NS_OK;
  }
  return nsAccessible::GetAttributesInternal(aAttributes);
}

// Internal frame, which is the doc's parent, should not have a click action
NS_IMETHODIMP
nsOuterDocAccessible::GetNumActions(PRUint8 *aNumActions)
{
  NS_ENSURE_ARG_POINTER(aNumActions);
  *aNumActions = 0;

  return NS_OK;
}

NS_IMETHODIMP
nsOuterDocAccessible::GetActionName(PRUint8 aIndex, nsAString& aName)
{
  aName.Truncate();

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsOuterDocAccessible::GetActionDescription(PRUint8 aIndex, nsAString& aDescription)
{
  aDescription.Truncate();

  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
nsOuterDocAccessible::DoAction(PRUint8 aIndex)
{
  return NS_ERROR_INVALID_ARG;
}
