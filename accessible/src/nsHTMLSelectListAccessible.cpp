/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 *
 * Contributor(s):  John Gaunt (jgaunt@netscape.com)
 */

#include "nsHTMLSelectListAccessible.h"

#include "nsRootAccessible.h"
#include "nsCOMPtr.h"
#include "nsLayoutAtoms.h"
#include "nsIAtom.h"
#include "nsIFrame.h"
#include "nsISelectControlFrame.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLSelectElement.h"

/** ----- nsHTMLSelectListAccessible ----- */

/**
  *
  */
nsHTMLSelectListAccessible::nsHTMLSelectListAccessible(nsIAccessible* aParent, 
                                                       nsIDOMNode* aDOMNode, 
                                                       nsIWeakReference* aShell)
                           :nsAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  return mParent->AccGetBounds(x,y,width,height);
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccParent(nsIAccessible **_retval)
{   
  *_retval = mParent;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LIST;
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> last;
  mDOMNode->GetLastChild(getter_AddRefs(last));

  *_retval = new nsHTMLSelectOptionAccessible(this, last, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  nsCOMPtr<nsIDOMNode> first;
  mDOMNode->GetFirstChild(getter_AddRefs(first));

  *_retval = new nsHTMLSelectOptionAccessible(this, first, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/**
  * As a nsHTMLSelectListAccessible we can have the following states:
  *     STATE_MULTISELECTABLE
  *     STATE_EXTSELECTABLE
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccState(PRUint32 *_retval)
{
  // this sets either STATE_FOCUSED or 0
  nsAccessible::GetAccState(_retval);

  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if ( select ) {
    PRBool multiple;
    select->GetMultiple(&multiple);
    if ( multiple )
      *_retval |= STATE_MULTISELECTABLE | STATE_EXTSELECTABLE;
  }

  *_retval |= STATE_FOCUSABLE;
  return NS_OK;
}


/** ----- nsHTMLSelectOptionAccessible ----- */

/**
  *
  */
nsHTMLSelectOptionAccessible::nsHTMLSelectOptionAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LISTITEM;
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccParent(nsIAccessible **_retval)
{   
  *_retval = mParent;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;

  nsCOMPtr<nsIDOMNode> next;
  mDOMNode->GetNextSibling(getter_AddRefs(next));

  if (next) {
    *_retval = new nsHTMLSelectOptionAccessible(mParent, next, mPresShell);
    if ( ! *_retval )
      return NS_ERROR_FAILURE;
    NS_ADDREF(*_retval);
  }

  return NS_OK;
} 

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;

  nsCOMPtr<nsIDOMNode> prev;
  mDOMNode->GetPreviousSibling(getter_AddRefs(prev));

  if (prev) {
    *_retval = new nsHTMLSelectOptionAccessible(mParent, prev, mPresShell);
    if ( ! *_retval )
      return NS_ERROR_FAILURE;
    NS_ADDREF(*_retval);
  }

  return NS_OK;
} 

/**
  *
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccName(nsAWritableString& _retval)
{
  nsCOMPtr<nsIContent> content (do_QueryInterface(mDOMNode));
  if (!content) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString option;
  nsresult rv = AppendFlatStringFromSubtree(content, &option);
  if (NS_SUCCEEDED(rv)) {
    // Temp var needed until CompressWhitespace built for nsAWritableString
    option.CompressWhitespace();
    _retval.Assign(option);
  }
  return rv;
}

/**
  * As a nsHTMLSelectOptionAccessible we can have the following states:
  *     STATE_SELECTABLE
  *     STATE_SELECTED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_INVISIBLE // not implemented yet
  */
NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccState(PRUint32 *_retval)
{
  // this sets either STATE_FOCUSED or 0
  nsAccessible::GetAccState(_retval);

  // Are we selected?
  nsCOMPtr<nsIDOMHTMLOptionElement> option (do_QueryInterface(mDOMNode));
  if ( option ) {
    PRBool isSelected = PR_FALSE;
    option->GetSelected(&isSelected);
    if ( isSelected ) 
      *_retval |= STATE_SELECTED;
  }

  *_retval |= STATE_SELECTABLE | STATE_FOCUSABLE;
  
  return NS_OK;
}


