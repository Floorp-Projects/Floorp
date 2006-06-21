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
 *   Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsHTMLTableAccessible.h"
#include "nsAccessibilityAtoms.h"
#include "nsIDOMElement.h"
#include "nsINameSpaceManager.h"

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLTableCellAccessible, nsHyperTextAccessible)

nsHTMLTableCellAccessible::nsHTMLTableCellAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsHyperTextAccessible(aDomNode, aShell)
{ 
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsHTMLTableCellAccessible::GetRole(PRUint32 *aResult)
{
  *aResult = ROLE_CELL;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLTableCellAccessible::GetState(PRUint32 *aResult)
{
  nsAccessible::GetState(aResult);
  *aResult &= ~STATE_FOCUSABLE;   // Inherit all states except focusable state since table cells cannot be focused
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED0(nsHTMLTableAccessible, nsHyperTextAccessible)

nsHTMLTableAccessible::nsHTMLTableAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsHyperTextAccessible(aDomNode, aShell)
{ 
}

/* unsigned long getRole (); */
NS_IMETHODIMP nsHTMLTableAccessible::GetRole(PRUint32 *aResult)
{
  *aResult = ROLE_TABLE;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLTableAccessible::GetState(PRUint32 *aResult)
{
  nsAccessible::GetState(aResult);
  *aResult |= STATE_READONLY;
  *aResult &= ~STATE_FOCUSABLE;   // Inherit all states except focusable state since tables cannot be focused
  return NS_OK;
}

NS_IMETHODIMP nsHTMLTableAccessible::GetName(nsAString& aName)
{
  aName.Truncate();  // Default name is blank

  if (mRoleMapEntry) {
    nsAccessible::GetName(aName);
    if (!aName.IsEmpty()) {
      return NS_OK;
    }
  }

  nsCOMPtr<nsIDOMElement> element(do_QueryInterface(mDOMNode));
  if (element) {
    nsCOMPtr<nsIDOMNodeList> captions;
    nsAutoString nameSpaceURI;
    element->GetNamespaceURI(nameSpaceURI);
    element->GetElementsByTagNameNS(nameSpaceURI, NS_LITERAL_STRING("caption"), 
                                    getter_AddRefs(captions));
    if (captions) {
      nsCOMPtr<nsIDOMNode> captionNode;
      captions->Item(0, getter_AddRefs(captionNode));
      if (captionNode) {
        nsCOMPtr<nsIContent> captionContent(do_QueryInterface(captionNode));
        AppendFlatStringFromSubtree(captionContent, &aName);
      }
    }
    if (aName.IsEmpty()) {
      nsCOMPtr<nsIContent> content(do_QueryInterface(element));
      NS_ASSERTION(content, "No content for DOM element");
      content->GetAttr(kNameSpaceID_None, nsAccessibilityAtoms::summary, aName);
    }
  }
  return NS_OK;
}

