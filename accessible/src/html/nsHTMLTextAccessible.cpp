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

#include "nsHTMLTextAccessible.h"
#include "nsIAccessibleDocument.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsISelectionController.h"

nsHTMLTextAccessible::nsHTMLTextAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell, nsIFrame *aFrame):
nsTextAccessibleWrap(aDomNode, aShell), mFrame(aFrame)
{ 
}

NS_IMETHODIMP nsHTMLTextAccessible::GetName(nsAString& aName)
{ 
  nsAutoString accName;
  if (NS_FAILED(mDOMNode->GetNodeValue(accName)))
    return NS_ERROR_FAILURE;
  accName.CompressWhitespace();
  aName = accName;
  return NS_OK;
}

nsIFrame* nsHTMLTextAccessible::GetFrame()
{
  if (mWeakShell) {
    return mFrame? mFrame : nsTextAccessible::GetFrame();
  }
  return nsnull;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetState(PRUint32 *aState)
{
  nsTextAccessible::GetState(aState);
  // Get current selection and find out if current node is in it
  nsCOMPtr<nsIPresShell> shell(GetPresShell());
  if (!shell) {
     return NS_ERROR_FAILURE;  
  }

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsIFrame *frame = nsnull;
  // The root frame and all text frames in the document share the same
  // selection controller.
  shell->GetRootFrame(&frame);
  if (frame) {
    nsCOMPtr<nsISelectionController> selCon;
    frame->GetSelectionController(context, getter_AddRefs(selCon));
    if (selCon) {
      nsCOMPtr<nsISelection> domSel;
      selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
      if (domSel) {
        PRBool isSelected = PR_FALSE, isCollapsed = PR_TRUE;
        domSel->ContainsNode(mDOMNode, PR_TRUE, &isSelected);
        domSel->GetIsCollapsed(&isCollapsed);
        if (isSelected && !isCollapsed)
          *aState |=STATE_SELECTED;
      }
    }
  }
  *aState |= STATE_SELECTABLE;

  nsCOMPtr<nsIAccessibleDocument> docAccessible(GetDocAccessible());
  if (docAccessible) {
    PRBool isEditable;
    docAccessible->GetIsEditable(&isEditable);
    if (!isEditable) {
      *aState |= STATE_READONLY;
    }
  }

  return NS_OK;
}

nsHTMLHRAccessible::nsHTMLHRAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsLeafAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLHRAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = ROLE_SEPARATOR;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLHRAccessible::GetState(PRUint32 *aState)
{
  nsLeafAccessible::GetState(aState);
  *aState &= ~STATE_FOCUSABLE;
  return NS_OK;
}

nsHTMLLabelAccessible::nsHTMLLabelAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsTextAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLLabelAccessible::GetName(nsAString& aReturn)
{ 
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));

  nsAutoString name;
  if (content)
    rv = AppendFlatStringFromSubtree(content, &name);

  if (NS_SUCCEEDED(rv)) {
    // Temp var needed until CompressWhitespace built for nsAString
    name.CompressWhitespace();
    aReturn = name;
  }

  return rv;
}

NS_IMETHODIMP nsHTMLLabelAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = ROLE_STATICTEXT;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLLabelAccessible::GetState(PRUint32 *aState)
{
  nsTextAccessible::GetState(aState);
  *aState &= (STATE_LINKED|STATE_TRAVERSED);  // Only use link states
  return NS_OK;
}

NS_IMETHODIMP nsHTMLLabelAccessible::GetFirstChild(nsIAccessible **aFirstChild) 
{  
  // A <label> is not necessarily a leaf!
  return nsAccessible::GetFirstChild(aFirstChild);
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsHTMLLabelAccessible::GetLastChild(nsIAccessible **aLastChild)
{  
  // A <label> is not necessarily a leaf!
  return nsAccessible::GetLastChild(aLastChild);
}

/* readonly attribute long accChildCount; */
NS_IMETHODIMP nsHTMLLabelAccessible::GetChildCount(PRInt32 *aAccChildCount) 
{
  // A <label> is not necessarily a leaf!
  return nsAccessible::GetChildCount(aAccChildCount);
}
