/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * 
 * Contributor(s):
 *                  John Gaunt (jgaunt@netscape.com)
 *                  Kyle Yuan (kyle.yuan@sun.com)
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

#include "nsCOMPtr.h"
#include "nsFormControlAccessible.h"
#include "nsIAtom.h"
#include "nsIComboboxControlFrame.h"
#include "nsIDOMHTMLOptGroupElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMText.h"
#include "nsIDOMXULMultSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlEl.h"
#include "nsIDOMXULSelectCntrlItemEl.h"
#include "nsIFrame.h"
#include "nsIListControlFrame.h"
#include "nsISelectControlFrame.h"
#include "nsIServiceManager.h"
#include "nsLayoutAtoms.h"
#include "nsRootAccessible.h"
#include "nsSelectAccessible.h"


/** ------------------------------------------------------ */
/**  First, the common widgets                             */
/** ------------------------------------------------------ */

/** Constructor -- cache our parent */
nsSelectListAccessible::nsSelectListAccessible(nsIAccessible* aParent, 
                                                       nsIDOMNode* aDOMNode, 
                                                       nsIWeakReference* aShell)
:nsAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

/** Return our parents bounds */
NS_IMETHODIMP nsSelectListAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  return mParent->AccGetBounds(x,y,width,height);
}

/** Return our cached parent */
NS_IMETHODIMP nsSelectListAccessible::GetAccParent(nsIAccessible **_retval)
{   
  *_retval = mParent;
  NS_ADDREF(*_retval);
  return NS_OK;
}

/** We are a list */
NS_IMETHODIMP nsSelectListAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LIST;
  return NS_OK;
}

/** We are an only child */
NS_IMETHODIMP nsSelectListAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
}

/** We are an only child */
NS_IMETHODIMP nsSelectListAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * nsSelectOptionAccessible
  */

/** Constructor -- cache our parent  */
nsSelectOptionAccessible::nsSelectOptionAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  if (aParent)
    mParent = aParent;
  else {
    nsCOMPtr<nsIAccessibilityService> accService(do_GetService("@mozilla.org/accessibilityService;1"));
    nsCOMPtr<nsIDOMNode> parentNode, parentNode1;
    nsCOMPtr<nsIAccessible> parentAccessible, lastChildAcc;
    aDOMNode->GetParentNode(getter_AddRefs(parentNode));
    if (parentNode) {
      // this parent could be a Combobox or a ListBox. Each has a different
      // was to get to the ListElement. 
        nsCOMPtr<nsIDOMHTMLOptGroupElement> optGroupElement(do_QueryInterface(parentNode));
        if (optGroupElement) {
          parentNode->GetParentNode(getter_AddRefs(parentNode1));
          parentNode = parentNode1;
        }
        accService->GetAccessibleFor(parentNode, getter_AddRefs(parentAccessible));
        PRUint32 role;
        do {
          parentAccessible->GetAccLastChild(getter_AddRefs(lastChildAcc));
          if (lastChildAcc)
            lastChildAcc->GetAccRole(&role);
          parentAccessible = lastChildAcc;
        } while (role != nsIAccessible::ROLE_LIST && lastChildAcc);
    }
    mParent = parentAccessible;
  }
}

/** We are a ListItem */
NS_IMETHODIMP nsSelectOptionAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LISTITEM;
  return NS_OK;
}

/** Return our cached parent */
NS_IMETHODIMP nsSelectOptionAccessible::GetAccParent(nsIAccessible **_retval)
{   
  *_retval = mParent;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/**
  * Get our Name from our Content's subtree
  */
NS_IMETHODIMP nsSelectOptionAccessible::GetAccName(nsAString& _retval)
{
  // CASE #1 -- great majority of the cases
  // find the label attribute - this is what the W3C says we should use
  nsCOMPtr<nsIDOMElement> domElement(do_QueryInterface(mDOMNode));
  NS_ASSERTION(domElement, "No domElement for accessible DOM node!");
  nsresult rv = domElement->GetAttribute(NS_LITERAL_STRING("label"), _retval) ;

  if (NS_SUCCEEDED(rv) && !_retval.IsEmpty() ) {
    return NS_OK;
  }
  
  // CASE #2 -- no label parameter, get the first child, 
  // use it if it is a text node
  nsCOMPtr<nsIDOMNode> child;
  mDOMNode->GetFirstChild(getter_AddRefs(child));

  if (child) {
    nsCOMPtr<nsIDOMText> text (do_QueryInterface(child));
    if (text) {
      nsCOMPtr<nsIContent> content (do_QueryInterface(child));
      if (!content) {
        return NS_ERROR_FAILURE;
      }
      nsAutoString txtValue;
      rv = AppendFlatStringFromContentNode(content, &txtValue);
      if (NS_SUCCEEDED(rv)) {
        // Temp var (txtValue) needed until CompressWhitespace built for nsAString
        txtValue.CompressWhitespace();
        _retval.Assign(txtValue);
        return NS_OK;
      }
    }
  }
  
  return NS_ERROR_FAILURE;
}

/** ----- nsComboboxTextFieldAccessible ----- */

/** Constructor */
nsComboboxTextFieldAccessible::nsComboboxTextFieldAccessible(nsIAccessible* aParent, 
                                                                 nsIDOMNode* aDOMNode, 
                                                                 nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

/**
  * Currently gets the text from the first option, needs to check for selection
  *     and then return that text.
  *     Walks the Frame tree and checks for proper frames.
  */
NS_IMETHODIMP nsComboboxTextFieldAccessible::GetAccValue(nsAString& _retval)
{
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);
  if (!frame || !context)
    return NS_ERROR_FAILURE;

  frame->FirstChild(context, nsnull, &frame);
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::blockFrame))
    return NS_ERROR_FAILURE;
#endif

  frame->FirstChild(context, nsnull, &frame);
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::textFrame))
    return NS_ERROR_FAILURE;
#endif

  nsCOMPtr<nsIContent> content;
  frame->GetContent(getter_AddRefs(content));

  if (!content) 
    return NS_ERROR_FAILURE;

  AppendFlatStringFromSubtree(content, &_retval);

  return NS_OK;
}

/**
  * Gets the bounds for the BlockFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsComboboxTextFieldAccessible::GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  // get our first child's frame
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);
  if (!frame || !context)
    return;

  frame->FirstChild(context, nsnull, aBoundingFrame);
  frame->FirstChild(context, nsnull, &frame);
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::blockFrame))
    return;
#endif

  frame->GetRect(aBounds);
}

/** Return our cached parent */
NS_IMETHODIMP nsComboboxTextFieldAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

/**
  * We are the first child of our parent, no previous sibling
  */
NS_IMETHODIMP nsComboboxTextFieldAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
} 

/**
  * Our role is currently only static text, but we should be able to have
  *     editable text here and we need to check that case.
  */
NS_IMETHODIMP nsComboboxTextFieldAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_STATICTEXT;
  return NS_OK;
}

/**
  * As a nsComboboxTextFieldAccessible we can have the following states:
  *     STATE_READONLY
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsComboboxTextFieldAccessible::GetAccState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetAccState(_retval);

  *_retval |= STATE_READONLY | STATE_FOCUSABLE;

  return NS_OK;
}

/** -----ComboboxButtonAccessible ----- */

/** Constructor -- cache our parent */
nsComboboxButtonAccessible::nsComboboxButtonAccessible(nsIAccessible* aParent, 
                                                           nsIDOMNode* aDOMNode, 
                                                           nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

/** Just one action ( click ). */
NS_IMETHODIMP nsComboboxButtonAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

/**
  * Gets the bounds for the gfxButtonControlFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsComboboxButtonAccessible::GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
  // get our second child's frame
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);
  if (!context)
    return;

  *aBoundingFrame = frame;  // bounding frame is the ComboboxControlFrame
  frame->FirstChild(context, nsnull, &frame); // first frame is for the textfield
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::blockFrame))
    return;
#endif

  frame->GetNextSibling(&frame);  // sibling frame is for the button
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::gfxButtonControlFrame))
    return;
#endif

  frame->GetRect(aBounds);
}

/** We are a button. */
NS_IMETHODIMP nsComboboxButtonAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

/** Return our cached parent */
NS_IMETHODIMP nsComboboxButtonAccessible::GetAccParent(nsIAccessible **_retval)
{   
  *_retval = mParent;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/** 
  * Gets the name from GetAccActionName()
  */
NS_IMETHODIMP nsComboboxButtonAccessible::GetAccName(nsAString& _retval)
{
  return GetAccActionName(eAction_Click, _retval);
}

/**
  * Our action name is the reverse of our state: 
  *     if we are closed -> open is our name.
  *     if we are open -> closed is our name.
  * Uses the frame to get the state, updated on every click
  */
NS_IMETHODIMP nsComboboxButtonAccessible::GetAccActionName(PRUint8 index, nsAString& _retval)
{
  PRBool isOpen = PR_FALSE;
  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;
  comboFrame->IsDroppedDown(&isOpen);
  if (isOpen)
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("close"), _retval); 
  else
    nsAccessible::GetTranslatedString(NS_LITERAL_STRING("open"), _retval); 

  return NS_OK;
}

/**
  * As a nsComboboxButtonAccessible we can have the following states:
  *     STATE_PRESSED
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  */
NS_IMETHODIMP nsComboboxButtonAccessible::GetAccState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetAccState(_retval);

  // we are open or closed --> pressed or not
  PRBool isOpen = PR_FALSE;
  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;
  comboFrame->IsDroppedDown(&isOpen);
  if (isOpen)
  *_retval |= STATE_PRESSED;

  *_retval |= STATE_FOCUSABLE;

  return NS_OK;
}

/** ----- nsComboboxWindowAccessible ----- */

/**
  * Constructor -- cache our parent
  */
nsComboboxWindowAccessible::nsComboboxWindowAccessible(nsIAccessible* aParent, 
                                                           nsIDOMNode* aDOMNode, 
                                                           nsIWeakReference* aShell):
nsAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

/**
  * As a nsComboboxWindowAccessible we can have the following states:
  *     STATE_FOCUSED
  *     STATE_FOCUSABLE
  *     STATE_INVISIBLE
  *     STATE_FLOATING
  */
NS_IMETHODIMP nsComboboxWindowAccessible::GetAccState(PRUint32 *_retval)
{
  // Get focus status from base class
  nsAccessible::GetAccState(_retval);

  // we are open or closed
  PRBool isOpen = PR_FALSE;
  nsIFrame *boundsFrame = GetBoundsFrame();
  nsIComboboxControlFrame* comboFrame = nsnull;
  boundsFrame->QueryInterface(NS_GET_IID(nsIComboboxControlFrame), (void**)&comboFrame);
  if (!comboFrame)
    return NS_ERROR_FAILURE;
  comboFrame->IsDroppedDown(&isOpen);
  if (! isOpen)
    *_retval |= STATE_INVISIBLE;

  *_retval |= STATE_FOCUSABLE | STATE_FLOATING;

  return NS_OK;
}

/** We are a window */
NS_IMETHODIMP nsComboboxWindowAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_WINDOW;
  return NS_OK;
}

/** Return our cached parent */
NS_IMETHODIMP nsComboboxWindowAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

/**
  * We are the last sibling of our parent.
  */
NS_IMETHODIMP nsComboboxWindowAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/**
  * We only have one child, a list
  */
NS_IMETHODIMP nsComboboxWindowAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/**
  * Gets the bounds for the areaFrame.
  *     Walks the Frame tree and checks for proper frames.
  */
void nsComboboxWindowAccessible::GetBounds(nsRect& aBounds, nsIFrame** aBoundingFrame)
{
   // get our first option
  nsCOMPtr<nsIDOMNode> child;
  mDOMNode->GetFirstChild(getter_AddRefs(child));

  // now get its frame
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) {
    *aBoundingFrame = nsnull;
    return;
  }

  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(child));
  shell->GetPrimaryFrameFor(content, &frame);
  if (!frame) {
    *aBoundingFrame = nsnull;
    return;
  }
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::blockFrame))
    return;
#endif

  frame->GetParent(aBoundingFrame);
  frame->GetParent(&frame);
#ifdef DEBUG
  if (! nsAccessible::IsCorrectFrameType(frame, nsLayoutAtoms::areaFrame))
    return;
#endif

  frame->GetRect(aBounds);
}

