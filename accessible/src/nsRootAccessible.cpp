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
 * Contributor(s): 
 */

#include "nsRootAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIDOMEventTarget.h"
#include "nsIDOMElement.h"
#include "nsIDOMEventReceiver.h"
#include "nsReadableUtils.h"

NS_INTERFACE_MAP_BEGIN(nsRootAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleEventReceiver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAccessibleEventReceiver)
NS_INTERFACE_MAP_END_INHERITING(nsAccessible)

NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsAccessible);
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsAccessible);

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIWeakReference* aShell, nsIFrame* aFrame):nsAccessible(nsnull,nsnull,aShell)
{
 // mFrame = aFrame;
 mListener = nsnull;
}

//-----------------------------------------------------
// destruction
//-----------------------------------------------------
nsRootAccessible::~nsRootAccessible()
{
  RemoveAccessibleEventListener(mListener);
}

  /* attribute wstring accName; */
NS_IMETHODIMP nsRootAccessible::GetAccName(PRUnichar * *aAccName) 
{ 
  *aAccName = ToNewUnicode(NS_LITERAL_STRING("Mozilla Document"));
  return NS_OK;  
}

// helpers
nsIFrame* nsRootAccessible::GetFrame()
{
  //if (!mFrame) {
    nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
    nsIFrame* root = nsnull;
    if (shell) 
      shell->GetRootFrame(&root);
  
    return root;
  //}
 
 // return mFrame;
}

nsIAccessible* nsRootAccessible::CreateNewAccessible(nsIAccessible* aAccessible, nsIContent* aContent, nsIWeakReference* aShell)
{
  return new nsHTMLBlockAccessible(aAccessible, aContent, aShell);
}

  /* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsRootAccessible::GetAccParent(nsIAccessible * *aAccParent) 
{ 
  *aAccParent = nsnull;
  return NS_OK;
}

  /* readonly attribute wstring accRole; */
NS_IMETHODIMP nsRootAccessible::GetAccRole(PRUnichar * *aAccRole) 
{ 
  *aAccRole = ToNewUnicode(NS_LITERAL_STRING("client"));
  return NS_OK;  
}

/* void addAccessibleEventListener (in nsIAccessibleEventListener aListener); */
NS_IMETHODIMP nsRootAccessible::AddAccessibleEventListener(nsIAccessibleEventListener *aListener)
{
  if (!mListener)
  {
     // add an event listener to the document
     nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
     nsCOMPtr<nsIDocument> document;
     shell->GetDocument(getter_AddRefs(document));

     nsCOMPtr<nsIDOMEventReceiver> receiver;
     if (NS_SUCCEEDED(document->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(receiver))) && receiver)
     {
        nsresult rv = receiver->AddEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener *, this), NS_GET_IID(nsIDOMFocusListener));
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
     }
  }

  // create a weak reference to the listener
  mListener =  aListener;

  return NS_OK;
}

/* void removeAccessibleEventListener (in nsIAccessibleEventListener aListener); */
NS_IMETHODIMP nsRootAccessible::RemoveAccessibleEventListener(nsIAccessibleEventListener *aListener)
{
  if (mListener)
  {
     nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
     nsCOMPtr<nsIDocument> document;
     if (!shell)
       return NS_OK;

     shell->GetDocument(getter_AddRefs(document));

     nsCOMPtr<nsIDOMEventReceiver> erP;
     if (NS_SUCCEEDED(document->QueryInterface(NS_GET_IID(nsIDOMEventReceiver), getter_AddRefs(erP))) && erP)
     {
        nsresult rv = erP->RemoveEventListenerByIID(NS_STATIC_CAST(nsIDOMFocusListener *, this), NS_GET_IID(nsIDOMFocusListener));
        NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
     }
  }

  mListener = nsnull;

  return NS_OK;
}


nsresult nsRootAccessible::HandleEvent(nsIDOMEvent* aEvent)
{
  if (mListener) {
     nsCOMPtr<nsIDOMEventTarget> t;
     aEvent->GetOriginalTarget(getter_AddRefs(t));
  
     // create and accessible for the target
     nsCOMPtr<nsIContent> content = do_QueryInterface(t); 

     if (!content)
       return NS_OK;

     if (mCurrentFocus == content)
       return NS_OK;

     mCurrentFocus = content;

     nsIFrame* frame = nsnull;
     nsCOMPtr<nsIPresShell> shell = do_QueryReferent(mPresShell);
     shell->GetPrimaryFrameFor(content, &frame);

     if (!frame)
       return NS_OK;

     nsCOMPtr<nsIAccessible> a = do_QueryInterface(frame);

     if (!a)
       a = do_QueryInterface(content);

     nsCOMPtr<nsIAccessible> na = CreateNewAccessible(a, content, mPresShell);

     mListener->HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, na);
  }

  return NS_OK;
}

nsresult nsRootAccessible::Focus(nsIDOMEvent* aEvent) 
{ 
  return NS_OK; 
}

nsresult nsRootAccessible::Blur(nsIDOMEvent* aEvent) 
{ 
  return NS_OK;
}



