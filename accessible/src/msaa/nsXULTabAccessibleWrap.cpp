/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Corp.
 * Portions created by Netscape Corp.are Copyright (C) 2003 Netscape
 * Corp. All Rights Reserved.
 *
 * Original Author: Aaron Leventhal
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXULTabAccessibleWrap.h"
#include "nsTextFormatter.h"

// --------------------------------------------------------
// nsXULTabAccessibleWrap Accessible
// --------------------------------------------------------

nsXULTabAccessibleWrap::nsXULTabAccessibleWrap(nsIDOMNode *aDOMNode, 
                                               nsIWeakReference *aShell):
nsXULTabAccessible(aDOMNode, aShell)
{
}

NS_IMETHODIMP nsXULTabAccessibleWrap::GetDescription(nsAString& aDescription)
{
  aDescription.Truncate();
  nsCOMPtr<nsIAccessible> parent;
  GetParent(getter_AddRefs(parent));
  NS_ENSURE_TRUE(parent, NS_ERROR_FAILURE);
  
  PRInt32 indexInParent = 0, numSiblings = 0;
  
  nsCOMPtr<nsIAccessible> sibling, nextSibling;
  parent->GetFirstChild(getter_AddRefs(sibling));
  NS_ENSURE_TRUE(sibling, NS_ERROR_FAILURE);
  
  PRBool foundCurrentTab = PR_FALSE;
  PRUint32 role;
  while (sibling) {
    sibling->GetRole(&role);
    if (role == ROLE_PAGETAB) {
      ++ numSiblings;
      if (!foundCurrentTab) {
        ++ indexInParent;
        if (sibling == this) {
          foundCurrentTab = PR_TRUE;        
        }
      }
    }
    sibling->GetNextSibling(getter_AddRefs(nextSibling));
    sibling = nextSibling;
  }
  
  // Don't localize the string "of" -- that's just the format of this string.
  // The AT will parse the relevant numbers out and add its own localization.
  nsTextFormatter::ssprintf(aDescription, L"%d of %d", indexInParent, numSiblings);

  return NS_OK;
}
