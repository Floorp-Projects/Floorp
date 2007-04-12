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
#include "nsAccessibleTreeWalker.h"
#include "nsBulletFrame.h"
#include "nsIAccessibleDocument.h"
#include "nsIAccessibleEvent.h"
#include "nsIFrame.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsISelectionController.h"
#include "nsComponentManagerUtils.h"

nsHTMLTextAccessible::nsHTMLTextAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell, nsIFrame *aFrame):
nsTextAccessibleWrap(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLTextAccessible::GetName(nsAString& aName)
{
  aName.Truncate();
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;
  }

  nsIFrame *frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_FAILURE);

  nsAutoString name;
  nsresult rv = mDOMNode->GetNodeValue(name);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!frame->GetStyleText()->WhiteSpaceIsSignificant()) {
    // Replace \r\n\t in markup with space unless in this is preformatted text
    // where those characters are significant
    name.ReplaceChar("\r\n\t", ' ');
  }
  aName = name;
  return rv;
}

NS_IMETHODIMP nsHTMLTextAccessible::GetRole(PRUint32 *aRole)
{
  nsIFrame *frame = GetFrame();
  NS_ENSURE_TRUE(frame, NS_ERROR_NULL_POINTER);

  if (frame->IsGeneratedContentFrame()) {
    *aRole = nsIAccessibleRole::ROLE_STATICTEXT;
    return NS_OK;
  }

  return nsTextAccessible::GetRole(aRole);
}

NS_IMETHODIMP
nsHTMLTextAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsTextAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIAccessible> docAccessible = 
    do_QueryInterface(nsCOMPtr<nsIAccessibleDocument>(GetDocAccessible()));
  if (docAccessible) {
     PRUint32 state, extState;
     docAccessible->GetFinalState(&state, &extState);
     if (0 == (extState & nsIAccessibleStates::EXT_STATE_EDITABLE)) {
       *aState |= nsIAccessibleStates::STATE_READONLY; // Links not focusable in editor
     }
  }

  return NS_OK;
}

nsresult
nsHTMLTextAccessible::GetAttributesInternal(nsIPersistentProperties *aAttributes)
{
  if (!mDOMNode) {
    return NS_ERROR_FAILURE;  // Node already shut down
  }

  PRUint32 role;
  GetRole(&role);
  if (role == nsIAccessibleRole::ROLE_STATICTEXT) {
    nsAutoString oldValueUnused;
    aAttributes->SetStringProperty(NS_LITERAL_CSTRING("static"),
                                  NS_LITERAL_STRING("true"), oldValueUnused);
  }

  return NS_OK;
}

nsHTMLHRAccessible::nsHTMLHRAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsLeafAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLHRAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_SEPARATOR;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLHRAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsLeafAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  return NS_OK;
}

nsHTMLBRAccessible::nsHTMLBRAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsLeafAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLBRAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_WHITESPACE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLBRAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  *aState = nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLBRAccessible::GetName(nsAString& aName)
{
  aName = NS_STATIC_CAST(PRUnichar, '\n');    // Newline char
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
  *aRole = nsIAccessibleRole::ROLE_LABEL;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLLabelAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsTextAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= (nsIAccessibleStates::STATE_LINKED |
              nsIAccessibleStates::STATE_TRAVERSED);  // Only use link states
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

nsHTMLLIAccessible::nsHTMLLIAccessible(nsIDOMNode *aDOMNode, nsIWeakReference* aShell, 
                   nsIFrame *aBulletFrame, const nsAString& aBulletText):
  nsHyperTextAccessible(aDOMNode, aShell)
{
  if (!aBulletText.IsEmpty()) {
    mBulletAccessible = new nsHTMLListBulletAccessible(mDOMNode, mWeakShell, 
                                                       aBulletFrame, aBulletText);
    nsCOMPtr<nsPIAccessNode> bulletANode(mBulletAccessible);
    if (bulletANode) {
      bulletANode->Init();
    }
  }
}

NS_IMETHODIMP nsHTMLLIAccessible::Shutdown()
{
  if (mBulletAccessible) {
    // Ensure that weak pointer to this is nulled out
    mBulletAccessible->Shutdown();
  }
  nsresult rv = nsAccessibleWrap::Shutdown();
  mBulletAccessible = nsnull;
  return rv;
}

NS_IMETHODIMP nsHTMLLIAccessible::GetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  nsresult rv = nsAccessibleWrap::GetBounds(x, y, width, height);
  if (NS_FAILED(rv) || !mBulletAccessible) {
    return rv;
  }

  PRInt32 bulletX, bulletY, bulletWidth, bulletHeight;
  rv = mBulletAccessible->GetBounds(&bulletX, &bulletY, &bulletWidth, &bulletHeight);
  NS_ENSURE_SUCCESS(rv, rv);

  *x = bulletX; // Move x coordinate of list item over to cover bullet as well
  *width += bulletWidth;
  return NS_OK;
}

void nsHTMLLIAccessible::CacheChildren()
{
  if (!mWeakShell || mAccChildCount != eChildCountUninitialized) {
    return;
  }

  nsAccessibleWrap::CacheChildren();

  if (mBulletAccessible) {
    mBulletAccessible->SetNextSibling(mFirstChild);
    mBulletAccessible->SetParent(this); // Set weak parent;
    SetFirstChild(mBulletAccessible);
    ++ mAccChildCount;
  }
}


// nsHTMLListBulletAccessible
nsHTMLListBulletAccessible::
  nsHTMLListBulletAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell,
                             nsIFrame *aFrame, const nsAString& aBulletText) :
    nsLeafAccessible(aDomNode, aShell), mWeakParent(nsnull),
    mBulletText(aBulletText)
{
  mBulletText += ' '; // Otherwise bullets are jammed up against list text
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetUniqueID(void **aUniqueID)
{
  // Since mDOMNode is same as for list item, use |this| pointer as the unique Id
  *aUniqueID = NS_STATIC_CAST(void*, this);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::Shutdown()
{
  mBulletText.Truncate();
  mWeakParent = nsnull;

  return nsLeafAccessible::Shutdown();
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetName(nsAString &aName)
{
  aName = mBulletText;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetRole(PRUint32 *aRole)
{
  *aRole = nsIAccessibleRole::ROLE_STATICTEXT;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsLeafAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::SetParent(nsIAccessible *aParentAccessible)
{
  mParent = nsnull;
  mWeakParent = aParentAccessible;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetParent(nsIAccessible **aParentAccessible)
{
  NS_IF_ADDREF(*aParentAccessible = mWeakParent);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLListBulletAccessible::GetContentText(nsAString& aText)
{
  aText = mBulletText;
  return NS_OK;
}

// nsHTMLListAccessible

NS_IMETHODIMP
nsHTMLListAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsHyperTextAccessible::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState &= ~nsIAccessibleStates::STATE_FOCUSABLE;
  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

// nsHTMLLIAccessible

NS_IMETHODIMP
nsHTMLLIAccessible::GetState(PRUint32 *aState, PRUint32 *aExtraState)
{
  nsresult rv = nsAccessibleWrap::GetState(aState, aExtraState);
  NS_ENSURE_SUCCESS(rv, rv);

  *aState |= nsIAccessibleStates::STATE_READONLY;
  return NS_OK;
}

