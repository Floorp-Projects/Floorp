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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsSelectAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsRootAccessible.h"
#include "nsINameSpaceManager.h"
#include "nsMutableAccessible.h"
#include "nsLayoutAtoms.h"
#include "nsIDOMMenuListener.h"
#include "nsIDOMEventReceiver.h"

class nsSelectChildAccessible : public nsAccessible,
                                public nsIDOMMenuListener
{
public:
  
  NS_DECL_ISUPPORTS_INHERITED

  nsSelectChildAccessible(nsIAtom* aPopupAtom, nsIContent* aSelectContent, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual ~nsSelectChildAccessible();

  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(PRUnichar **_retval);
  NS_IMETHOD GetAccRole(PRUnichar **_retval);
  NS_IMETHOD GetAccValue(PRUnichar **_retval);

  virtual nsIAccessible* CreateNewNextAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual nsIAccessible* CreateNewPreviousAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  // popup listener
  NS_IMETHOD Create(nsIDOMEvent* aEvent);
  NS_IMETHOD Close(nsIDOMEvent* aEvent);
  NS_IMETHOD Destroy(nsIDOMEvent* aEvent);
  NS_IMETHOD Action(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD Broadcast(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD CommandUpdate(nsIDOMEvent* aEvent) { return NS_OK; }
  nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  nsCOMPtr<nsIAtom> mPopupAtom;
  nsCOMPtr<nsIContent> mSelectContent;
  PRBool mRegistered;
  PRBool mOpen;
};

NS_IMPL_ISUPPORTS_INHERITED(nsSelectChildAccessible, nsAccessible, nsIDOMMenuListener)

class nsSelectWindowAccessible : public nsAccessible,
                                 public nsIDOMMenuListener
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  nsSelectWindowAccessible(nsIAtom* aPopupAtom, nsIAccessible* aParent, nsIAccessible* aPrev, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual ~nsSelectWindowAccessible();

  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccLastChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccFirstChild(nsIAccessible **_retval);
  NS_IMETHOD GetAccChildCount(PRInt32 *_retval);
  NS_IMETHOD GetAccName(PRUnichar **_retval);
  NS_IMETHOD GetAccRole(PRUnichar **_retval);
  NS_IMETHOD GetAccState(PRUint32 *_retval);
  NS_IMETHOD GetAccExtState(PRUint32 *_retval);
  
  // popup listener
  NS_IMETHOD Create(nsIDOMEvent* aEvent);
  NS_IMETHOD Close(nsIDOMEvent* aEvent);
  NS_IMETHOD Destroy(nsIDOMEvent* aEvent);
  NS_IMETHOD Action(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD Broadcast(nsIDOMEvent* aEvent) { return NS_OK; }
  NS_IMETHOD CommandUpdate(nsIDOMEvent* aEvent) { return NS_OK; }
  nsresult HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }

  // helpers
  virtual nsIFrame* GetBoundsFrame();

  nsCOMPtr<nsIAccessible> mParent;
  nsCOMPtr<nsIAccessible> mPrev;
  nsCOMPtr<nsIAtom> mPopupAtom;
  PRBool mRegistered;
  PRBool mOpen;
};

NS_IMPL_ISUPPORTS_INHERITED(nsSelectWindowAccessible, nsAccessible, nsIDOMMenuListener)

class nsSelectListAccessible : public nsAccessible
{
public:
  
  nsSelectListAccessible(nsIAtom* aPopupAtom, nsIAccessible* aParent, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual ~nsSelectListAccessible() {}

  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccName(PRUnichar **_retval);
  NS_IMETHOD GetAccRole(PRUnichar **_retval);
  NS_IMETHOD GetAccNextSibling(nsIAccessible **_retval);
  NS_IMETHOD GetAccPreviousSibling(nsIAccessible **_retval);
  NS_IMETHOD AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height);

  virtual void GetListAtomForFrame(nsIFrame* aFrame, nsIAtom*& aList);
  virtual nsIAccessible* CreateNewFirstAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual nsIAccessible* CreateNewLastAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);

  nsCOMPtr<nsIAtom> mPopupAtom;
  nsCOMPtr<nsIAccessible> mParent;
};

class nsListChildAccessible : public nsAccessible
{
public:
  
  nsListChildAccessible(nsIAtom* aPopupAtom, nsIContent* aSelectContent, nsIAccessible* aParent, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);
  virtual ~nsListChildAccessible() {}

  NS_IMETHOD GetAccParent(nsIAccessible **_retval);
  NS_IMETHOD GetAccRole(PRUnichar **_retval);

  virtual void GetListAtomForFrame(nsIFrame* aFrame, nsIAtom*& aList);
  virtual nsIAccessible* CreateNewAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell);

  nsCOMPtr<nsIAccessible> mParent;
  nsCOMPtr<nsIAtom> mPopupAtom;
  nsCOMPtr<nsIContent> mSelectContent;
};

//---------
 
nsSelectAccessible::nsSelectAccessible(nsIAtom* aPopupAtom, 
                                       nsIAccessible* aAccessible, 
                                       nsIContent* aContent, 
                                       nsIWeakReference* aShell)
                                               :nsAccessible(aAccessible, aContent, aShell)
{
  mPopupAtom = aPopupAtom;
}

NS_IMETHODIMP nsSelectAccessible::GetAccValue(PRUnichar **_retval)
{
  // our value is our first child's value. Which is the combo boxes text.
  nsCOMPtr<nsIAccessible> text;
  nsresult rv = GetAccFirstChild(getter_AddRefs(text));

  if (NS_FAILED(rv)) {
     *_retval = nsnull;
     return rv;
  }

  if (!text) {
     *_retval = nsnull;
     return NS_ERROR_FAILURE;
  }

  // look at our role
  return text->GetAccValue(_retval);
}

NS_IMETHODIMP nsSelectAccessible::GetAccName(PRUnichar **_retval)
{
   *_retval = nsnull;
   return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsSelectAccessible::GetAccRole(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("combo box");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsSelectAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  // get the last child. Wrap it with a connector that connects it to the window accessible
  nsCOMPtr<nsIAccessible> last;
  nsresult rv = nsAccessible::GetAccLastChild(getter_AddRefs(last));
  if (NS_FAILED(rv))
    return rv;

  if (!last) {
    // we have a parent but not previous
    *_retval = new nsSelectWindowAccessible(mPopupAtom, this, nsnull, nsnull, mContent, mPresShell);
  } else {
    *_retval = last;
  }

  NS_ADDREF(*_retval);

   return NS_OK;
}

NS_IMETHODIMP nsSelectAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  // get the last child. Wrap it with a connector that connects it to the window accessible
  nsCOMPtr<nsIAccessible> first;
  nsresult rv = nsAccessible::GetAccFirstChild(getter_AddRefs(first));
  if (NS_FAILED(rv))
    return rv;

  if (!first) {
    *_retval = new nsSelectWindowAccessible(mPopupAtom, this, nsnull, nsnull, mContent, mPresShell);
  } else {
    *_retval = first;
  }

  NS_ADDREF(*_retval);

  return NS_OK;
}

nsIAccessible* nsSelectAccessible::CreateNewFirstAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return CreateNewLastAccessible(aAccessible, aContent, aShell);
}

nsIAccessible* nsSelectAccessible::CreateNewLastAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aContent,"Error not accessible or content");
  return new nsSelectChildAccessible(mPopupAtom, mContent, aAccessible, aContent, aShell);
}

NS_IMETHODIMP nsSelectAccessible::GetAccChildCount(PRInt32 *_retval)
{
  nsresult rv = nsAccessible::GetAccChildCount(_retval);
  if (NS_FAILED(rv))
    return rv;

  // always have one more that is our window child
  (*_retval)++;
  return NS_OK;
}

//--------------------

nsSelectChildAccessible::nsSelectChildAccessible(nsIAtom* aPopupAtom, nsIContent* aSelectContent, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell):
nsAccessible(aAccessible, aContent, aShell)
{
  mPopupAtom = aPopupAtom;
  mSelectContent = aSelectContent;
  mRegistered = PR_FALSE;
  mOpen = PR_FALSE;
}

NS_IMETHODIMP nsSelectChildAccessible::GetAccValue(PRUnichar **_retval)
{
  nsresult rv = NS_OK;
  PRUnichar* string = nsnull;

  // look at our role
  rv = nsAccessible::GetAccRole(&string);
  if (NS_FAILED(rv)) {
   *_retval = nsnull;
   return rv;
  }

  nsAutoString role(string);

  // if its the text in the combo box then
  // its value should be its name.
  if (role.EqualsIgnoreCase("text")) {
    rv = nsAccessible::GetAccName(_retval);
  } else {
    rv = nsAccessible::GetAccValue(_retval);
  }

  delete string;
  return rv;
}

NS_IMETHODIMP nsSelectChildAccessible::GetAccRole(PRUnichar **_retval)
{
  nsresult rv = NS_OK;
  PRUnichar* string = nsnull;

  // look at our role
  rv = nsAccessible::GetAccRole(&string);
  if (NS_FAILED(rv)) {
   *_retval = nsnull;
   return rv;
  }

  nsAutoString role(string);

  // any text in the combo box is static
  if (role.EqualsIgnoreCase("text")) {
    // if it the comboboxes text. Make it static
    nsAutoString name;
    name.AssignWithConversion("static text");
    *_retval = name.ToNewUnicode();
  } else {
    rv = nsAccessible::GetAccRole(_retval);
  }

  delete string;
  return rv;
}


NS_IMETHODIMP nsSelectChildAccessible::GetAccName(PRUnichar **_retval)
{
  nsresult rv = NS_OK;
  PRUnichar* string = nsnull;

  // look at our role
  nsAccessible::GetAccRole(&string);
  nsAutoString role(string);

  // if button then we need to make the name be open or close
  if (role.EqualsIgnoreCase("push button"))
  {
     // if its a button and not already registered, 
     // register ourselves as a popup listener   
     if (!mRegistered) {
       nsCOMPtr<nsIDOMEventReceiver> eventReceiver = do_QueryInterface(mSelectContent);
       if (!eventReceiver) {
         *_retval = nsnull;
         return NS_ERROR_NOT_IMPLEMENTED;
       }

       eventReceiver->AddEventListener(NS_LITERAL_STRING("create"), this, PR_TRUE);   

       mRegistered = PR_TRUE;
     }

     // get the current state open or closed
     // set _retval to it.
     // notice its supposed to be reversed. Close if opened
     // and Open if closed.
     nsAutoString name;
     if (mOpen)
       name.AssignWithConversion("Close");
     else
       name.AssignWithConversion("Open");

     *_retval = name.ToNewUnicode();

  } else {
    /*rv = nsAccessible::GetAccName(_retval);*/
    rv = NS_ERROR_NOT_IMPLEMENTED;
    *_retval = nsnull;
  }

  delete string;

  return rv;
}


nsSelectChildAccessible::~nsSelectChildAccessible()
{
  if (mRegistered) {
     nsCOMPtr<nsIDOMEventReceiver> eventReceiver = do_QueryInterface(mSelectContent);
     if (eventReceiver) 
       eventReceiver->RemoveEventListener(NS_LITERAL_STRING("create"), this, PR_TRUE);   
  }
}

NS_IMETHODIMP nsSelectChildAccessible::Create(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_TRUE;
  printf("Open\n");

  /* TBD send state change event */ 

  return NS_OK; 
}

NS_IMETHODIMP nsSelectChildAccessible::Destroy(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_FALSE;
  printf("Close\n");

  /* TBD send state change event */ 

  return NS_OK; 
}

NS_IMETHODIMP nsSelectChildAccessible::Close(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_FALSE;
  printf("Close\n");

  /* TBD send state change event */ 

  return NS_OK; 
}

nsIAccessible* nsSelectChildAccessible::CreateNewNextAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return CreateNewPreviousAccessible(aAccessible, aContent, aShell);
}

nsIAccessible* nsSelectChildAccessible::CreateNewPreviousAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aContent,"Error not accessible or content");
  return new nsSelectChildAccessible(mPopupAtom, mSelectContent, aAccessible, aContent, aShell);
}

NS_IMETHODIMP nsSelectChildAccessible::GetAccNextSibling(nsIAccessible **_retval)
{ 
  nsCOMPtr<nsIAccessible> next;
  nsresult rv = nsAccessible::GetAccNextSibling(getter_AddRefs(next));
  if (NS_FAILED(rv))
    return rv;

  if (!next) {
    // ok no more siblings. Lets create our window
    nsCOMPtr<nsIAccessible> parent;
    GetAccParent(getter_AddRefs(parent));

    *_retval = new nsSelectWindowAccessible(mPopupAtom, parent, nsnull, nsnull, mSelectContent, mPresShell);
  } else {
    *_retval = next;
  }

  NS_ADDREF(*_retval);

  return NS_OK;
} 


//---------------------


nsSelectWindowAccessible::nsSelectWindowAccessible(nsIAtom* aPopupAtom, nsIAccessible* aParent, nsIAccessible* aPrev, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
:nsAccessible(aAccessible, aContent, aShell)
{
  mParent = aParent;
  mPrev = aPrev;
  mPopupAtom = aPopupAtom;
  mRegistered = PR_FALSE;
  mOpen = PR_FALSE;
}

nsSelectWindowAccessible::~nsSelectWindowAccessible()
{
  if (mRegistered) {
     nsCOMPtr<nsIDOMEventReceiver> eventReceiver = do_QueryInterface(mContent);
     if (eventReceiver) 
       eventReceiver->RemoveEventListener(NS_LITERAL_STRING("create"), this, PR_TRUE);   
  }
}

NS_IMETHODIMP nsSelectWindowAccessible::Create(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_TRUE;
  printf("Open\n");

  /* TBD send state change event */ 

  return NS_OK; 
}

NS_IMETHODIMP nsSelectWindowAccessible::Destroy(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_FALSE;
  printf("Close\n");

  /* TBD send state change event */ 

  return NS_OK; 
}

NS_IMETHODIMP nsSelectWindowAccessible::Close(nsIDOMEvent* aEvent)
{ 
  mOpen = PR_FALSE;
  printf("Close\n");

  /* TBD send state change event */ 

  return NS_OK; 
}


NS_IMETHODIMP nsSelectWindowAccessible::GetAccState(PRUint32 *_retval)
{
  // not not already one register ourselves as a popup listener
   
  if (!mRegistered) {

     nsCOMPtr<nsIDOMEventReceiver> eventReceiver = do_QueryInterface(mContent);
     if (!eventReceiver) {
       *_retval = 0;
       return NS_ERROR_NOT_IMPLEMENTED;
     }

     nsresult rv = eventReceiver->AddEventListener(NS_LITERAL_STRING("create"), this, PR_TRUE);   

     if (NS_FAILED(rv)) {
       *_retval = 0;
       return rv;
     }

     mRegistered = PR_TRUE;
  }

  // if open we are visible if closed we are invisible
   // set _retval to it.
   if (mOpen)
     *_retval |= STATE_DEFAULT;
   else
     *_retval |= STATE_INVISIBLE;

   return NS_OK;
}


NS_IMETHODIMP nsSelectWindowAccessible::GetAccExtState(PRUint32 *_retval)
{
	*_retval=0;
   return NS_OK;
}

NS_IMETHODIMP nsSelectWindowAccessible::GetAccName(PRUnichar **_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
 
NS_IMETHODIMP nsSelectWindowAccessible::GetAccRole(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("window");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsSelectWindowAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}
 
NS_IMETHODIMP nsSelectWindowAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = mPrev;
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsSelectWindowAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsSelectWindowAccessible::GetAccLastChild(nsIAccessible **_retval)
{
  *_retval = new nsSelectListAccessible(mPopupAtom, this, nsnull, mContent, mPresShell);
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsSelectWindowAccessible::GetAccFirstChild(nsIAccessible **_retval)
{
  *_retval = new nsSelectListAccessible(mPopupAtom, this, nsnull, mContent, mPresShell);
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_IMETHODIMP nsSelectWindowAccessible::GetAccChildCount(PRInt32 *_retval)
{
  *_retval = 1;
  return NS_OK;
}

/*
NS_IMETHODIMP nsSelectWindowAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  *x = *y = *width = *height = 0;
  return NS_OK;
}
*/

nsIFrame* nsSelectWindowAccessible::GetBoundsFrame()
{
  // get our frame
  nsIFrame* frame = GetFrame();

  nsCOMPtr<nsIPresContext> context;
  GetPresContext(context);

  // get its first popup child that should be the window
  frame->FirstChild(context, mPopupAtom, &frame);

  return frame;
}

//----------


nsSelectListAccessible::nsSelectListAccessible(nsIAtom* aPopupAtom, nsIAccessible* aParent, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
:nsAccessible(aAccessible, aContent, aShell)
{
    mPopupAtom = aPopupAtom;
    mParent = aParent;
}

void nsSelectListAccessible::GetListAtomForFrame(nsIFrame* aFrame, nsIAtom*& aList)
{
   nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
   nsIFrame* frame = nsnull;
   shell->GetPrimaryFrameFor(mContent, &frame);
   if (aFrame == frame)
     aList = mPopupAtom;
   else
     aList = nsnull;
}

NS_IMETHODIMP nsSelectListAccessible::AccGetBounds(PRInt32 *x, PRInt32 *y, PRInt32 *width, PRInt32 *height)
{
  return mParent->AccGetBounds(x,y,width,height);
}

NS_IMETHODIMP nsSelectListAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_ADDREF(*_retval);
    return NS_OK;
}

NS_IMETHODIMP nsSelectListAccessible::GetAccName(PRUnichar **_retval)
{
  *_retval = nsnull;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsSelectListAccessible::GetAccRole(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("list");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsSelectListAccessible::GetAccPreviousSibling(nsIAccessible **_retval)
{ 
  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsSelectListAccessible::GetAccNextSibling(nsIAccessible **_retval)
{
  *_retval = nsnull;
  return NS_OK;
}

nsIAccessible* nsSelectListAccessible::CreateNewFirstAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aContent,"Error not accessible or content");
  return new nsListChildAccessible(mPopupAtom, mContent, this, aAccessible, aContent, aShell);
}

nsIAccessible* nsSelectListAccessible::CreateNewLastAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aContent,"Error not accessible or content");
  return new nsListChildAccessible(mPopupAtom, mContent, this, aAccessible, aContent, aShell);
}
 
//--------

nsListChildAccessible::nsListChildAccessible(nsIAtom* aPopupAtom, nsIContent* aSelectContent, nsIAccessible* aParent, nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell):
nsAccessible(aAccessible, aContent, aShell)
{
  mParent = aParent;
  mPopupAtom = aPopupAtom;
  mSelectContent = aSelectContent;
}

nsIAccessible* nsListChildAccessible::CreateNewAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  NS_ASSERTION(aAccessible && aContent,"Error not accessible or content");
  return new nsListChildAccessible(mPopupAtom, mSelectContent, mParent, aAccessible, aContent, aShell);
}

void nsListChildAccessible::GetListAtomForFrame(nsIFrame* aFrame, nsIAtom*& aList)
{
   nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
   nsIFrame* frame = nsnull;
   shell->GetPrimaryFrameFor(mSelectContent, &frame);
   if (aFrame == frame)
     aList = mPopupAtom;
   else
     aList = nsnull;
}

NS_IMETHODIMP nsListChildAccessible::GetAccRole(PRUnichar **_retval)
{
  nsAutoString a;
  a.AssignWithConversion("list item");
  *_retval = a.ToNewUnicode();
  return NS_OK;
}

NS_IMETHODIMP nsListChildAccessible::GetAccParent(nsIAccessible **_retval)
{   
    *_retval = mParent;
    NS_IF_ADDREF(*_retval);
    return NS_OK;
}

