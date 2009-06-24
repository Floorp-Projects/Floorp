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
nsresult
nsOuterDocAccessible::GetRoleInternal(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_INTERNAL_FRAME;
  return NS_OK;
}

nsresult
nsOuterDocAccessible::GetStateInternal(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetStateInternal(aState, aExtraState);
  NS_ENSURE_A11Y_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  return NS_OK;
}

// nsAccessible::GetChildAtPoint()
nsresult
nsOuterDocAccessible::GetChildAtPoint(PRInt32 aX, PRInt32 aY,
                                      PRBool aDeepestChild,
                                      nsIAccessible **aChild)
{
  PRInt32 docX = 0, docY = 0, docWidth = 0, docHeight = 0;
  nsresult rv = GetBounds(&docX, &docY, &docWidth, &docHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aX < docX || aX >= docX + docWidth || aY < docY || aY >= docY + docHeight)
    return NS_OK;

  // Always return the inner doc as direct child accessible unless bounds
  // outside of it.
  nsCOMPtr<nsIAccessible> childAcc;
  rv = GetFirstChild(getter_AddRefs(childAcc));
  NS_ENSURE_SUCCESS(rv, rv);

  if (!childAcc)
    return NS_OK;

  if (aDeepestChild)
    return childAcc->GetDeepestChildAtPoint(aX, aY, aChild);

  NS_ADDREF(*aChild = childAcc);
  return NS_OK;
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
  nsRefPtr<nsAccessible> innerAcc(nsAccUtils::QueryAccessible(innerAccessible));
  if (!innerAcc)
    return;

  // Success getting inner document as first child -- now we cache it.
  mAccChildCount = 1;
  SetFirstChild(innerAccessible); // weak ref
  innerAcc->SetParent(this);
  innerAcc->SetNextSibling(nsnull);
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
