/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 * Contributor(s):
 *   Aaron Leventhal (aaronl@netscape.com)
 *   Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsHTMLTextAccessible.h"

nsHTMLTextAccessible::nsHTMLTextAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsTextAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLTextAccessible::GetAccName(nsAString& aName)
{ 
  nsAutoString accName;
  if (NS_FAILED(mDOMNode->GetNodeValue(accName)))
    return NS_ERROR_FAILURE;
  accName.CompressWhitespace();
  aName = accName;
  return NS_OK;
}

nsHTMLHRAccessible::nsHTMLHRAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsLeafAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLHRAccessible::GetAccRole(PRUint32 *aRole)
{
  *aRole = ROLE_SEPARATOR;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLHRAccessible::GetAccState(PRUint32 *aState)
{
  nsLeafAccessible::GetAccState(aState);
  *aState &= ~STATE_FOCUSABLE;
  return NS_OK;
}

#ifdef MOZ_ACCESSIBILITY_ATK

NS_IMPL_ISUPPORTS_INHERITED2(nsHTMLBlockAccessible, nsBlockAccessible, nsIAccessibleHyperText, nsIAccessibleText)

nsHTMLBlockAccessible::nsHTMLBlockAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsBlockAccessible(aDomNode, aShell), nsAccessibleHyperText(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLBlockAccessible::GetAccName(nsAString& aName)
{
  nsAutoString name(NS_LITERAL_STRING("Paragraph "));
  name.AppendInt(GetIndex());
  aName = name;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLBlockAccessible::GetAccRole(PRUint32 *aRole)
{
  *aRole = ROLE_TEXT;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLBlockAccessible::GetAccState(PRUint32 *aState)
{
  nsAccessible::GetAccState(aState);
  *aState &= ~STATE_FOCUSABLE;
  return NS_OK;
}

#endif //MOZ_ACCESSIBILITY_ATK

nsHTMLLabelAccessible::nsHTMLLabelAccessible(nsIDOMNode* aDomNode, nsIWeakReference* aShell):
nsTextAccessible(aDomNode, aShell)
{ 
}

NS_IMETHODIMP nsHTMLLabelAccessible::GetAccName(nsAString& aReturn)
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

NS_IMETHODIMP nsHTMLLabelAccessible::GetAccRole(PRUint32 *aRole)
{
  *aRole = ROLE_STATICTEXT;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLLabelAccessible::GetAccState(PRUint32 *aState)
{
  nsTextAccessible::GetAccState(aState);
  *aState &= (STATE_LINKED|STATE_TRAVERSED);  // Only use link states
  return NS_OK;
}

NS_IMETHODIMP nsHTMLLabelAccessible::GetAccFirstChild(nsIAccessible **aAccFirstChild) 
{  
  // A <label> is not necessarily a leaf!
  return nsAccessible::GetAccFirstChild(aAccFirstChild);
}

  /* readonly attribute nsIAccessible accFirstChild; */
NS_IMETHODIMP nsHTMLLabelAccessible::GetAccLastChild(nsIAccessible **aAccLastChild)
{  
  // A <label> is not necessarily a leaf!
  return nsAccessible::GetAccLastChild(aAccLastChild);
}

/* readonly attribute long accChildCount; */
NS_IMETHODIMP nsHTMLLabelAccessible::GetAccChildCount(PRInt32 *aAccChildCount) 
{
  // A <label> is not necessarily a leaf!
  return nsAccessible::GetAccChildCount(aAccChildCount);
}
