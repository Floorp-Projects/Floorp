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
 * The Initial Developer of the Original Code is IBM Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Author: Aaron Leventhal (aleventh@us.ibm.com)
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

#include "nsXULAlertAccessible.h"


// ------------------------ Alert  -----------------------------

NS_IMPL_ISUPPORTS_INHERITED0(nsXULAlertAccessible, nsAccessible)

nsXULAlertAccessible::nsXULAlertAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell) :
  nsAccessibleWrap(aNode, aShell)
{
}

NS_IMETHODIMP nsXULAlertAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_ALERT;
  return NS_OK;
}

NS_IMETHODIMP
nsXULAlertAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  *aState |= nsIAccessibleStates::STATE_ALERT_MEDIUM; // XUL has no markup for low, medium or high
  return NS_OK;
}

#if 0
// We don't need this, but the AT will need to read all of the alert's children
// when it receives EVENT_ALERT on a ROLE_ALERT
NS_IMETHODIMP nsXULAlertAccessible::GetName(nsAString &aName)
{
  nsCOMPtr<nsIPresShell> presShell(do_QueryReferent(mWeakShell));
  if (!presShell) {
    return NS_ERROR_FAILURE; // Node already shut down
  }
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  NS_ASSERTION(content, "Should not be null if we still have a presShell");

  nsCOMPtr<nsIDOMNodeList> siblingList;
  // returns null if no anon nodes
  presShell->GetDocument()->GetXBLChildNodesFor(content,
                                                getter_AddRefs(siblingList));
  if (siblingList) {
    PRUint32 length, count;
    siblingList->GetLength(&length);
    for (count = 0; count < length; count ++) {
      nsCOMPtr<nsIDOMNode> domNode;
      siblingList->Item(count, getter_AddRefs(domNode));
      nsCOMPtr<nsIDOMXULLabeledControlElement> labeledEl(do_QueryInterface(domNode));
      if (labeledEl) {
        nsAutoString label;
        labeledEl->GetLabel(label);
        aName += NS_LITERAL_STRING(" ") + label + NS_LITERAL_STRING(" ");
      }
      else {
        nsCOMPtr<nsIContent> content(do_QueryInterface(domNode));
        if (content) {
          AppendFlatStringFromSubtree(content, &aName);
        }
      }
    }
  }
  else {
    AppendFlatStringFromSubtree(content, &aName);
  }

  return NS_OK;
}
#endif
