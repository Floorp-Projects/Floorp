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
 * Contributor(s):
 * Author: Aaron Leventhal (aaronl@netscape.com)
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

#include "nsGenericAccessible.h"
#include "nsHTMLAreaAccessible.h"
#include "nsReadableUtils.h"
#include "nsString.h"
#include "nsAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLAreaElement.h"


// --- area -----

nsHTMLAreaAccessible::nsHTMLAreaAccessible(nsIDOMNode *aDomNode, nsIAccessible *aAccParent, nsIWeakReference* aShell):
nsLinkableAccessible(aDomNode, aShell), mAccParent(aAccParent)
{ 
}

/* wstring getAccName (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccName(nsAWritableString & _retval)
{
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) {
    nsAutoString hrefString;
    elt->GetAttribute(NS_LITERAL_STRING("title"), _retval);
  }
  return NS_OK;
}

/* unsigned long getAccRole (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LINK;
  return NS_OK;
}

/* wstring getAccValue (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccValue(nsAWritableString& _retval)
{
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) 
    elt->GetAttribute(NS_LITERAL_STRING("href"), _retval);
  return NS_OK;
}

/* wstring getAccDescription (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccDescription(nsAWritableString& _retval)
{
  // Still to do - follow IE's standard here
  nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(mDOMNode));
  if (area) 
    area->GetShape(_retval);
  return NS_OK;
}


/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}


NS_IMETHODIMP nsHTMLAreaAccessible::GetAccParent(nsIAccessible * *aAccParent) 
{ 
  *aAccParent = mAccParent;
  NS_IF_ADDREF(*aAccParent);
  return NS_OK;
}

nsIAccessible *nsHTMLAreaAccessible::CreateAreaAccessible(nsIDOMNode *aDOMNode)
{
  nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
  if (accService) {
    nsIAccessible* acc = nsnull;
    accService->CreateHTMLAreaAccessible(mPresShell, aDOMNode, mAccParent, &acc);
    return acc;
  }
  return nsnull;

}

NS_IMETHODIMP nsHTMLAreaAccessible::GetAccNextSibling(nsIAccessible * *aAccNextSibling) 
{ 
  *aAccNextSibling = nsnull;
  nsCOMPtr<nsIDOMNode> nextNode;
  mDOMNode->GetNextSibling(getter_AddRefs(nextNode));
  if (nextNode)
    *aAccNextSibling = CreateAreaAccessible(nextNode);
  return NS_OK;  
}

/* readonly attribute nsIAccessible accPreviousSibling; */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccPreviousSibling(nsIAccessible * *aAccPrevSibling) 
{
  *aAccPrevSibling = nsnull;
  nsCOMPtr<nsIDOMNode> prevNode;
  mDOMNode->GetPreviousSibling(getter_AddRefs(prevNode));
  if (prevNode)
    *aAccPrevSibling = CreateAreaAccessible(prevNode);
  return NS_OK;  
}


/* void accGetBounds (out long x, out long y, out long width, out long height); */
NS_IMETHODIMP nsHTMLAreaAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  //nsIFrame *frame;
  // Do a better job on this later - need to use GetRect on mAreas of nsImageMap from nsImageFrame
  //return mAccParent->nsAccessible::AccGetBounds(x,y,width,height);

  *x = *y = *width = *height = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

