/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Author: Aaron Leventhal (aaronl@netscape.com)
 * Contributor(s): 
 */

#include "nsGenericAccessible.h"
#include "nsHTMLAreaAccessible.h"
#include "nsReadableUtils.h"
#include "nsAccessible.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"
#include "nsIDOMElement.h"
#include "nsIDOMHTMLAreaElement.h"


// --- area -----

nsHTMLAreaAccessible::nsHTMLAreaAccessible(nsIPresShell *aPresShell, nsIDOMNode *aDomNode, nsIAccessible *aAccParent):
nsGenericAccessible(), mPresShell(aPresShell), mDOMNode(aDomNode), mAccParent(aAccParent)
{ 
}

/* wstring getAccName (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccName(PRUnichar **_retval)
{
  *_retval = 0;
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) {
    nsAutoString hrefString;
    elt->GetAttribute(NS_LITERAL_STRING("title"), hrefString);
    if (!hrefString.IsEmpty()) {
      *_retval = hrefString.ToNewUnicode();
      return NS_OK;
    }
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
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccValue(PRUnichar **_retval)
{
  *_retval = 0;
  nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mDOMNode));
  if (elt) {
    nsAutoString hrefString;
    elt->GetAttribute(NS_LITERAL_STRING("href"), hrefString);
    *_retval = hrefString.ToNewUnicode();
  }
  return NS_OK;
}

/* wstring getAccDescription (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccDescription(PRUnichar **_retval)
{
  // Still to do - follow IE's standard here
  *_retval = 0;
  nsAutoString shapeString;
  nsCOMPtr<nsIDOMHTMLAreaElement> area(do_QueryInterface(mDOMNode));
  if (area) {
    area->GetShape(shapeString);
    if (!shapeString.IsEmpty())
      *_retval = ToNewUnicode(shapeString);
  }
  return NS_OK;
}


/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/* wstring getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsHTMLAreaAccessible::GetAccActionName(PRUint8 index, PRUnichar **_retval)
{
  if (index == 0) {
    *_retval = ToNewUnicode(NS_LITERAL_STRING("jump"));
    return NS_OK;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsHTMLAreaAccessible::AccDoAction(PRUint8 index)
{
  if (index == 0) {
    nsCOMPtr<nsIPresContext> presContext;
    mPresShell->GetPresContext(getter_AddRefs(presContext));
    if (presContext) {
      nsMouseEvent linkClickEvent;
      linkClickEvent.eventStructType = NS_EVENT;
      linkClickEvent.message = NS_MOUSE_LEFT_CLICK;
      linkClickEvent.isShift = PR_FALSE;
      linkClickEvent.isControl = PR_FALSE;
      linkClickEvent.isAlt = PR_FALSE;
      linkClickEvent.isMeta = PR_FALSE;
      linkClickEvent.clickCount = 0;
      linkClickEvent.widget = nsnull;

      nsEventStatus eventStatus =  nsEventStatus_eIgnore;
      nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
      content->HandleDOMEvent(presContext, &linkClickEvent, 
        nsnull, NS_EVENT_FLAG_INIT, &eventStatus);
      return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsHTMLAreaAccessible::GetAccState(PRUint32 *_retval) 
{
  *_retval = STATE_LINKED | STATE_FOCUSABLE | STATE_READONLY;
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
  nsresult rv;
  NS_WITH_SERVICE(nsIAccessibilityService, accService, "@mozilla.org/accessibilityService;1", &rv);
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

