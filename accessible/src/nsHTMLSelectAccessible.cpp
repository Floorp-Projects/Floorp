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
 * Contributor(s):
 * Original Author: Eric Vaughan (evaughan@netscape.com)
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

#include "nsHTMLSelectAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsRootAccessible.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMXULListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsReadableUtils.h"
#include "nsIDOMHTMLCollection.h"
#include "nsISelectElement.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIAccessibilityService.h"
#include "nsIServiceManager.h"
#include "nsIDOMHTMLOptionElement.h"

/*
 * A class the represents the text field in the Select to the left
 * of the drop down button
 */
class nsHTMLSelectTextFieldAccessible  : public nsLeafAccessible
{
public:
  
  nsHTMLSelectTextFieldAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccValue(nsAWritableString& _retval);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame);

  nsCOMPtr<nsIAccessible> mParent;
};

/*
 * A base class that can listen to menu events. Its used so the 
 * button and the window accessibles can change there name and role
 * depending on whether the drop down list is dropped down on not
 */
class nsMenuListenerAccessible  : public nsAccessible, public nsIDOMXULListener
{
public:
  
  NS_DECL_ISUPPORTS_INHERITED

  nsMenuListenerAccessible(nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsMenuListenerAccessible();

  // popup listener
  NS_IMETHOD PopupShowing(nsIDOMEvent* aEvent);
  NS_IMETHOD PopupShown(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD PopupHiding(nsIDOMEvent* aEvent);
  NS_IMETHOD PopupHidden(nsIDOMEvent* aEvent) { return NS_OK; }
  
  NS_IMETHOD Close(nsIDOMEvent* aEvent);
  NS_IMETHOD Command(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD Broadcast(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD CommandUpdate(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  virtual void SetupMenuListener();

  PRBool mRegistered;
  PRBool mOpen;
};

NS_IMPL_ISUPPORTS_INHERITED1(nsMenuListenerAccessible, nsAccessible, nsIDOMXULListener)

/**
 * A class that represents the button inside the Select to the right of the text field
 */
class nsHTMLSelectButtonAccessible  : public nsMenuListenerAccessible
{
public:
  
  nsHTMLSelectButtonAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(nsAWritableString& _retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccNumActions(PRUint8 *_retval);
  NS_IMETHOD GetAccActionName(PRUint8 index, nsAWritableString& _retval);
  NS_IMETHOD AccDoAction(PRUint8 index);


  virtual void GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame);



  nsCOMPtr<nsIAccessible> mParent;
};

/*
 * A class that represents the window that lives to the right
 * of the drop down button inside the Select. This is the window
 * that is made visible when the button is pressed.
 */
class nsHTMLSelectWindowAccessible : public nsMenuListenerAccessible
{
public:

  nsHTMLSelectWindowAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);

  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);

  virtual void GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame);
   
  nsCOMPtr<nsIAccessible> mParent; 
};

/*
 * The list that contains all the options in the select. It is inside the window.
 */
class nsHTMLSelectListAccessible : public nsAccessible
{
public:
  
  nsHTMLSelectListAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell);
  virtual ~nsHTMLSelectListAccessible() {}

  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUint32 *_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);

  nsCOMPtr<nsIAccessible> mParent;
};

//--------- nsHTMLSelectAccessible -----
 
nsHTMLSelectAccessible::nsHTMLSelectAccessible(nsIDOMNode* aDOMNode, 
                                       nsIWeakReference* aShell)
                                               :nsAccessible(aDOMNode, aShell)
{
}

NS_IMPL_ISUPPORTS_INHERITED1(nsHTMLSelectAccessible, nsAccessible, nsIAccessibleSelectable)

// ------------- Helper method for determination of proper Frame ------
//static 
PRBool nsHTMLSelectAccessible::IsCorrectFrame( nsIFrame* aFrame, nsIAtom* aAtom ) {
  if (!aFrame || !aAtom)
    return PR_FALSE;
  nsCOMPtr<nsIAtom> frameType;
  aFrame->GetFrameType(getter_AddRefs(frameType));
  if (frameType.get() != aAtom)
    return PR_FALSE;
  return PR_TRUE;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccValue(nsAWritableString& _retval)
{
  nsCOMPtr<nsIAccessible> text;
  GetAccFirstChild(getter_AddRefs(text));
  if (text)
    return text->GetAccValue(_retval);

  return NS_ERROR_FAILURE;
}


NS_IMETHODIMP nsHTMLSelectAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_COMBOBOX;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  // create a window accessible
  *_retval = new nsHTMLSelectWindowAccessible(this, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  // create a text field

  *_retval = new nsHTMLSelectTextFieldAccessible(this, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetAccChildCount(PRInt32 *_retval)
{
  // always have 3 children
  *_retval = 3;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectAccessible::GetSelectedChildren(nsISupportsArray **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

//-------- SelectTextFieldAccessible ------

nsHTMLSelectTextFieldAccessible::nsHTMLSelectTextFieldAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

NS_IMETHODIMP nsHTMLSelectTextFieldAccessible::GetAccValue(nsAWritableString& _retval)
{
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);
  if ( !frame )
    return NS_ERROR_FAILURE;

  frame->FirstChild(context, nsnull, &frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::blockFrame) )
    return NS_ERROR_FAILURE;

  frame->FirstChild(context, nsnull, &frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::textFrame) )
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content;
  frame->GetContent(getter_AddRefs(content));

  if (!content) 
    return NS_ERROR_FAILURE;

  AppendFlatStringFromSubtree(content, &_retval);

  return NS_OK;
}

void nsHTMLSelectTextFieldAccessible::GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
  // get our first child's frame
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);
  if ( !frame )
    return;

  frame->FirstChild(context, nsnull, &frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::blockFrame) )
    return;

  frame->GetParent(aRelativeFrame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( *aRelativeFrame, nsLayoutAtoms::areaFrame) )
    return;

  frame->GetRect(aBounds);
}

NS_IMETHODIMP nsHTMLSelectTextFieldAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectTextFieldAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLSelectButtonAccessible(parent, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
} 

NS_IMETHODIMP nsHTMLSelectTextFieldAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
} 

NS_IMETHODIMP nsHTMLSelectTextFieldAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_STATICTEXT;
  return NS_OK;
}

// --------- nsMenuListenerAccessible -----------

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
  // not not already one register ourselves as a popup listener
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


//-------- SelectButtonAccessible ------

nsHTMLSelectButtonAccessible::nsHTMLSelectButtonAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsMenuListenerAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

/* void accDoAction (in PRUint8 index); */
NS_IMETHODIMP nsHTMLSelectButtonAccessible::AccDoAction(PRUint8 index)
{
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  frame->FirstChild(context, nsnull, &frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::blockFrame) )
    return NS_ERROR_FAILURE;
 
  frame->GetNextSibling(&frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::gfxButtonControlFrame) )
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIContent> content;
  frame->GetContent(getter_AddRefs(content));

  if (index == 0) {
    nsCOMPtr<nsIDOMHTMLInputElement> element(do_QueryInterface(content));
    if (element)
    {
       element->Click();
       return NS_OK;
    }
    return NS_ERROR_FAILURE;
  }
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* PRUint8 getAccNumActions (); */
NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccNumActions(PRUint8 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

void nsHTMLSelectButtonAccessible::GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
  // get our second child's frame
  nsIFrame* frame = nsAccessible::GetBoundsFrame();
  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  frame->FirstChild(context, nsnull, &frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::blockFrame) )
    return;

  frame->GetNextSibling(&frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::gfxButtonControlFrame) )
    return;

  frame->GetParent(aRelativeFrame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( *aRelativeFrame, nsLayoutAtoms::areaFrame) )
    return;

  frame->GetRect(aBounds);
}

NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_PUSHBUTTON;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}


NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccName(nsAWritableString& _retval)
{
  return GetAccActionName(0, _retval);
}

NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccActionName(PRUint8 index, nsAWritableString& _retval)
{
   SetupMenuListener();

   // get the current state open or closed
   // set _retval to it.
   // notice its supposed to be reversed. Close if opened
   // and Open if closed.

   if (mOpen)
       _retval = NS_LITERAL_STRING("Close");
   else
       _retval = NS_LITERAL_STRING("Open");

  return NS_OK;
}


NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLSelectWindowAccessible(parent, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
} 

NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLSelectTextFieldAccessible(parent, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
} 

NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectButtonAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 0;
  return NS_OK;
}

//---------------------


nsHTMLSelectWindowAccessible::nsHTMLSelectWindowAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell)
:nsMenuListenerAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}


NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccState(PRUint32 *_retval)
{
   nsAccessible::GetAccState(_retval);

   SetupMenuListener();

  // if open we are visible if closed we are invisible
   // set _retval to it.
   if (mOpen)
     *_retval |= STATE_DEFAULT;
   else
     *_retval |= STATE_INVISIBLE;

   return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_WINDOW;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}
 
NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> parent;
  GetAccParent(getter_AddRefs(parent));

  *_retval = new nsHTMLSelectButtonAccessible(parent, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
} 

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(this, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = new nsHTMLSelectListAccessible(this, mDOMNode, mPresShell);
  if ( ! *_retval )
    return NS_ERROR_FAILURE;
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectWindowAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

void nsHTMLSelectWindowAccessible::GetBounds(nsRect& aBounds, nsIFrame** aRelativeFrame)
{
    // get our first option
  nsCOMPtr<nsIDOMNode> child;
  mDOMNode->GetFirstChild(getter_AddRefs(child));

  // now get its frame
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  if (!shell) {
    *aRelativeFrame = nsnull;
    return;
  }

  nsIFrame* frame = nsnull;
  nsCOMPtr<nsIContent> content(do_QueryInterface(child));
  shell->GetPrimaryFrameFor(content, &frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::blockFrame) )
    return;

  frame->GetParent(&frame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( frame, nsLayoutAtoms::areaFrame) )
    return;

  frame->GetParent(aRelativeFrame);
  if ( ! nsHTMLSelectAccessible::IsCorrectFrame( *aRelativeFrame, nsLayoutAtoms::listControlFrame) )
    return;

  frame->GetRect(aBounds);
}

//----------


nsHTMLSelectListAccessible::nsHTMLSelectListAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell)
:nsAccessible(aDOMNode, aShell)
{
    mParent = aParent;
}

NS_IMETHODIMP nsHTMLSelectListAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  return mParent->AccGetBounds(x,y,width,height);
}

NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_ADDREF(*_retval);
    return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LIST;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectListAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

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

//--------

nsHTMLSelectOptionAccessible::nsHTMLSelectOptionAccessible(nsIAccessible* aParent, nsIDOMNode* aDOMNode, nsIWeakReference* aShell):
nsLeafAccessible(aDOMNode, aShell)
{
  mParent = aParent;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccRole(PRUint32 *_retval)
{
  *_retval = ROLE_LISTITEM;
  return NS_OK;
}

NS_IMETHODIMP nsHTMLSelectOptionAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

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

