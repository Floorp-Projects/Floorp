/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Initial Developer of the Original Code is Netscape Communications Corporation..
 * Portions created by Netscape Communications Corporation. are
 * Copyright (C) 1998 Netscape Communications Corporation.. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Original Author: Eric Vaughan (evaughan@netscape.com)
 * Contributor(s):  John Gaunt (jgaunt@netscape.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "nsHTMLSelectListAccessible.h"

#include "nsRootAccessible.h"
#include "nsCOMPtr.h"
#include "nsLayoutAtoms.h"
#include "nsIAtom.h"
#include "nsIFrame.h"
#include "nsISelectControlFrame.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsIListControlFrame.h"

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
  *     no STATE_FOCUSED!  -- can't be focused, options are focused instead
  *     no STATE_FOCUSABLE!  -- can't be focused, options are focused instead
  */
NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccState(PRUint32 *_retval)
{
  nsCOMPtr<nsIDOMHTMLSelectElement> select (do_QueryInterface(mDOMNode));
  if ( select ) {
    PRBool multiple;
    select->GetMultiple(&multiple);
    if ( multiple )
      *_retval |= STATE_MULTISELECTABLE | STATE_EXTSELECTABLE;
  }

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
  // this sets either STATE_FOCUSED or 0 (nsAccessible::GetAccState() doesn't know about list options)
  *_retval = 0;
  nsCOMPtr<nsIDOMNode> focusedOptionNode, parentNode;
  mParent->AccGetDOMNode(getter_AddRefs(parentNode));
  GetFocusedOptionNode(mPresShell, parentNode, focusedOptionNode);
  if (focusedOptionNode == mDOMNode)
    *_retval |= STATE_FOCUSED;

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

nsresult nsHTMLSelectOptionAccessible::GetFocusedOptionNode(nsIWeakReference *aPresShell, 
                                                            nsIDOMNode *aListNode, 
                                                            nsCOMPtr<nsIDOMNode>& aFocusedOptionNode)
{
  NS_ASSERTION(aListNode, "Called GetFocusedOptionNode without a valid list node");
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(aPresShell));
  if (!shell)
    return NS_ERROR_FAILURE;

  nsIFrame *frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(aListNode));
  shell->GetPrimaryFrameFor(content, &frame);

  nsresult rv;
  nsCOMPtr<nsIListControlFrame> listFrame(do_QueryInterface(frame, &rv));
  if (NS_FAILED(rv))
    return rv;  // How can list content not have a list frame?
  NS_ASSERTION(listFrame, "We don't have a list frame, but rv returned a success code.");

  // Get what's focused by asking frame for "selected item". 
  // Don't use DOM method of getting selected item, which instead gives *first* selected option 
  PRInt32 focusedOptionIndex = 0;
  rv = listFrame->GetSelectedIndex(&focusedOptionIndex);  

  nsCOMPtr<nsIDOMHTMLCollection> options;

  // Get options
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMHTMLSelectElement> selectElement(do_QueryInterface(aListNode));
    NS_ASSERTION(selectElement, "No select element where it should be");
    rv = selectElement->GetOptions(getter_AddRefs(options));
  }

  // Either use options and focused index, or default to list node itself
  if (NS_SUCCEEDED(rv) && options && focusedOptionIndex >= 0)   // Something is focused
    rv = options->Item(focusedOptionIndex, getter_AddRefs(aFocusedOptionNode));
  else {  // If no options in list or focusedOptionIndex <0, then we are not focused on an item
    aFocusedOptionNode = aListNode;  // return normal target content
    rv = NS_OK;
  }

  return rv;
}

