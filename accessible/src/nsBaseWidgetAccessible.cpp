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
 *          John Gaunt (jgaunt@netscape.com)
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

#include "nsAccessible.h"
#include "nsBaseWidgetAccessible.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsIContent.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsIFrame.h"
#include "nsILink.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsISelection.h"
#include "nsISelectionController.h"

// ------------
// nsBlockAccessible
// ------------

nsBlockAccessible::nsBlockAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):nsAccessible(aNode, aShell)
{
}

/* nsIAccessible accGetAt (in long x, in long y); */
NS_IMETHODIMP nsBlockAccessible::AccGetAt(PRInt32 tx, PRInt32 ty, nsIAccessible **_retval)
{
  // We're going to find the child that contains coordinates (tx,ty)
  PRInt32 x,y,w,h;
  AccGetBounds(&x,&y,&w,&h);  // Get bounds for this accessible
  if (tx > x && tx < x + w && ty > y && ty < y + h)
  {
    // It's within this nsIAccessible, let's drill down
    nsCOMPtr<nsIAccessible> child;
    nsCOMPtr<nsIAccessible> smallestChild;
    PRInt32 smallestArea = -1;
    nsCOMPtr<nsIAccessible> next;
    GetAccFirstChild(getter_AddRefs(child));
    PRInt32 cx,cy,cw,ch;  // Child bounds

    while(child) {
      child->AccGetBounds(&cx,&cy,&cw,&ch);
      
      // ok if there are multiple frames the contain the point 
      // and they overlap then pick the smallest. We need to do this
      // for text frames.
      
      // For example, A point that's in block #2 is also in block #1, but we want to return #2:
      //
      // [[block #1 is long wrapped text that continues to
      // another line]]  [[here is a shorter block #2]]

      if (tx > cx && tx < cx + cw && ty > cy && ty < cy + ch) 
      {
        if (smallestArea == -1 || cw*ch < smallestArea) {
          smallestArea = cw*ch;
          smallestChild = child;
        }
      }
      child->GetAccNextSibling(getter_AddRefs(next));
      child = next;
    }

    if (smallestChild != nsnull)
    {
      *_retval = smallestChild;
      NS_ADDREF(*_retval);
      return NS_OK;
    }

    *_retval = this;
    NS_ADDREF(this);
    return NS_OK;
  }

  *_retval = nsnull;
  return NS_OK;
}

/**
  * nsContainerAccessible
  */
nsContainerAccessible::nsContainerAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{
}

/** no actions */
NS_IMETHODIMP nsContainerAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eNo_Action;
  return NS_OK;
}

/** no actions */
NS_IMETHODIMP nsContainerAccessible::GetAccActionName(PRUint8 index, nsAWritableString& _retval)
{
  return NS_OK;
}

/** no actions */
NS_IMETHODIMP nsContainerAccessible::AccDoAction(PRUint8 index)
{
  return NS_OK;
}

/** no state -- normal */
NS_IMETHODIMP nsContainerAccessible::GetAccState(PRUint32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

/** no value */
NS_IMETHODIMP nsContainerAccessible::GetAccValue(nsAWritableString& _retval)
{
  return NS_OK;
}

/** no name*/
NS_IMETHODIMP nsContainerAccessible::GetAccName(nsAWritableString& _retval)
{
  return NS_OK;
}


//-------------
// nsLeafFrameAccessible
//-------------

nsLeafAccessible::nsLeafAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell)
{
}

/* nsIAccessible getAccFirstChild (); */
NS_IMETHODIMP nsLeafAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* nsIAccessible getAccLastChild (); */
NS_IMETHODIMP nsLeafAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

/* long getAccChildCount (); */
NS_IMETHODIMP nsLeafAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}


//----------------
// nsLinkableAccessible
//----------------

nsLinkableAccessible::nsLinkableAccessible(nsIDOMNode* aNode, nsIWeakReference* aShell):
nsAccessible(aNode, aShell), mIsALinkCached(PR_FALSE), mLinkContent(nsnull), mIsLinkVisited(PR_FALSE)
{ 
}

/* long GetAccState (); */
NS_IMETHODIMP nsLinkableAccessible::GetAccState(PRUint32 *_retval)
{
  nsAccessible::GetAccState(_retval);
  *_retval |= STATE_READONLY | STATE_SELECTABLE;
  if (IsALink()) {
    *_retval |= STATE_LINKED;
    if (mIsLinkVisited)
      *_retval |= STATE_TRAVERSED;
  }
  
  // Get current selection and find out if current node is in it
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) {
     return NS_ERROR_FAILURE;  
  }

  nsCOMPtr<nsIPresContext> context;
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsIContent> content(do_QueryInterface(mDOMNode));
  nsIFrame *frame;
  if (content && NS_SUCCEEDED(shell->GetPrimaryFrameFor(content, &frame))) {
    nsCOMPtr<nsISelectionController> selCon;
    frame->GetSelectionController(context,getter_AddRefs(selCon));
    if (selCon) {
      nsCOMPtr<nsISelection> domSel;
      selCon->GetSelection(nsISelectionController::SELECTION_NORMAL, getter_AddRefs(domSel));
      if (domSel) {
        PRBool isSelected = PR_FALSE, isCollapsed = PR_TRUE;
        domSel->ContainsNode(mDOMNode, PR_TRUE, &isSelected);
        domSel->GetIsCollapsed(&isCollapsed);
        if (isSelected && !isCollapsed)
          *_retval |=STATE_SELECTED;
      }
    }
  }

  // Focused? Do we implement that here or up the chain?
  return NS_OK;
}


NS_IMETHODIMP nsLinkableAccessible::GetAccValue(nsAWritableString& _retval)
{
  if (IsALink()) {
    nsCOMPtr<nsIDOMElement> elt(do_QueryInterface(mLinkContent));
    if (elt) 
      return elt->GetAttribute(NS_LITERAL_STRING("href"), _retval);
  }
  return NS_ERROR_FAILURE;
}


/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsLinkableAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = eSingle_Action;
  return NS_OK;
}

/* wstring getAccActionName (in PRUint8 index); */
NS_IMETHODIMP nsLinkableAccessible::GetAccActionName(PRUint8 index, nsAWritableString& _retval)
{
  // Action 0 (default action): Jump to link
  if (index == eAction_Jump) {   
    if (IsALink()) {
      _retval = NS_LITERAL_STRING("jump"); 
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsLinkableAccessible::AccDoAction(PRUint8 index)
{
  // Action 0 (default action): Jump to link
  if (index == 0) {
    if (IsALink()) {
      nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
      if (!shell) {
        return NS_ERROR_FAILURE;  
      }

      nsCOMPtr<nsIPresContext> presContext;
      shell->GetPresContext(getter_AddRefs(presContext));
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
        mLinkContent->HandleDOMEvent(presContext, 
                                     &linkClickEvent, 
                                     nsnull, 
                                     NS_EVENT_FLAG_INIT, 
                                     &eventStatus);
        return NS_OK;
      }
    }
  }
  return NS_ERROR_FAILURE;
}


PRBool nsLinkableAccessible::IsALink()
{
  if (mIsALinkCached)  // Cached answer?
    return mLinkContent? PR_TRUE: PR_FALSE;

  nsCOMPtr<nsIContent> walkUpContent(do_QueryInterface(mDOMNode));
  if (walkUpContent) {
    nsCOMPtr<nsIContent> tempContent = walkUpContent;
    while (walkUpContent) {
      nsCOMPtr<nsILink> link(do_QueryInterface(walkUpContent));
      if (link) {
        mLinkContent = tempContent;
        mIsALinkCached = PR_TRUE;
        nsLinkState linkState;
        link->GetLinkState(linkState);
        if (linkState == eLinkState_Visited)
          mIsLinkVisited = PR_TRUE;
        return PR_TRUE;
      }
      walkUpContent->GetParent(*getter_AddRefs(tempContent));
      walkUpContent = tempContent;
    }
  }
  mIsALinkCached = PR_TRUE;  // Cached that there is no link
  return PR_FALSE;
}

// ------------
// nsMenuListenerAccessible
// ------------

NS_IMPL_ISUPPORTS_INHERITED1(nsMenuListenerAccessible, nsAccessible, nsIDOMXULListener)

nsMenuListenerAccessible::nsMenuListenerAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsAccessible(aDOMNode, aShell)
{
  mRegistered = PR_FALSE;
  mOpen = PR_FALSE;
}

nsMenuListenerAccessible::~nsMenuListenerAccessible()
{
  if (mRegistered) {
     nsCOMPtr<nsIDOMEventReceiver> eventReceiver(do_QueryInterface(mDOMNode));
     if (eventReceiver) 
       eventReceiver->RemoveEventListener(NS_LITERAL_STRING("popupshowing"), this, PR_TRUE);   
  }
}

NS_IMETHODIMP nsMenuListenerAccessible::PopupShowing(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_TRUE;

  /* TBD send state change event */ 

  return NS_OK; 
}

NS_IMETHODIMP nsMenuListenerAccessible::PopupHiding(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_FALSE;

  /* TBD send state change event */ 

  return NS_OK; 
}

NS_IMETHODIMP nsMenuListenerAccessible::Close(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_FALSE;

  /* TBD send state change event */ 

  return NS_OK; 
}

void
nsMenuListenerAccessible::SetupMenuListener()
{
  // if not already one, register ourselves as a popup listener
  if (!mRegistered) {
     nsCOMPtr<nsIDOMEventReceiver> eventReceiver(do_QueryInterface(mDOMNode));
     if (!eventReceiver) {
       return;
     }

     nsresult rv = eventReceiver->AddEventListener(NS_LITERAL_STRING("popupshowing"), this, PR_TRUE);   

     if (NS_FAILED(rv)) {
       return;
     }

     mRegistered = PR_TRUE;
  }
}

