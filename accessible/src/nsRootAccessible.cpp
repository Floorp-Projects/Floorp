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
#include "nsILink.h"
#include "nsHTMLFormControlAccessible.h"
#include "nsHTMLLinkAccessible.h"
#include "nsIURI.h"
#include "nsIDocShellTreeItem.h"

NS_INTERFACE_MAP_BEGIN(nsRootAccessible)
  NS_INTERFACE_MAP_ENTRY(nsIAccessibleEventReceiver)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMFormListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIDOMEventListener, nsIDOMFormListener)
NS_INTERFACE_MAP_END_INHERITING(nsAccessible)

NS_IMPL_ADDREF_INHERITED(nsRootAccessible, nsAccessible);
NS_IMPL_RELEASE_INHERITED(nsRootAccessible, nsAccessible);

//-----------------------------------------------------
// construction 
//-----------------------------------------------------
nsRootAccessible::nsRootAccessible(nsIWeakReference* aShell):nsAccessible(nsnull,nsnull,aShell)
{
  mListener = nsnull;
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  shell->GetDocument(getter_AddRefs(mDocument));
  mDOMNode = do_QueryInterface(mDocument);
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
  const nsString* docTitle = mDocument->GetDocumentTitle();
  if (docTitle && !docTitle->IsEmpty())
    *aAccName = docTitle->ToNewUnicode();
  else *aAccName = ToNewUnicode(NS_LITERAL_STRING("Document"));
  
  return NS_OK;  
}

// helpers
nsIFrame* nsRootAccessible::GetFrame()
{
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  nsIFrame* root = nsnull;
  if (shell) 
    shell->GetRootFrame(&root);

  return root;
}

void nsRootAccessible::GetBounds(nsRect& aBounds)
{
  nsIFrame* frame = GetFrame();
  frame->GetRect(aBounds);
}

nsIAccessible* nsRootAccessible::CreateNewAccessible(nsIAccessible* aAccessible, nsIDOMNode* aNode, nsIWeakReference* aShell)
{
  return new nsHTMLBlockAccessible(aAccessible, aNode, aShell);
}

/* readonly attribute nsIAccessible accParent; */
NS_IMETHODIMP nsRootAccessible::GetAccParent(nsIAccessible * *aAccParent) 
{ 
  *aAccParent = nsnull;
  return NS_OK;
}

/* readonly attribute unsigned long accRole; */
NS_IMETHODIMP nsRootAccessible::GetAccRole(PRUint32 *aAccRole) 
{ 
  nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
  nsCOMPtr<nsIPresContext> context; 
  shell->GetPresContext(getter_AddRefs(context));
  nsCOMPtr<nsISupports> container;
  context->GetContainer(getter_AddRefs(container));
  if (container) {
    nsCOMPtr<nsIDocShellTreeItem> parentTreeItem, docTreeItem(do_QueryInterface(container));
    if (docTreeItem) {
      docTreeItem->GetSameTypeParent(getter_AddRefs(parentTreeItem));
      // Basically, if this docshell has a parent of the same type, it's a frame
      if (parentTreeItem) {
        *aAccRole = ROLE_PANE;
        return NS_OK;
      }
    }
  }

  *aAccRole = ROLE_CLIENT;
  return NS_OK;  
}

NS_IMETHODIMP nsRootAccessible::GetAccValue(PRUnichar * *aAccValue)
{
  nsCOMPtr<nsIURI> pURI(mDocument->GetDocumentURL());
  char *path;
  pURI->GetSpec(&path);
  *aAccValue = ToNewUnicode(nsLiteralCString(path));
  return NS_OK;
}

/* void addAccessibleEventListener (in nsIAccessibleEventListener aListener); */
NS_IMETHODIMP nsRootAccessible::AddAccessibleEventListener(nsIAccessibleEventListener *aListener)
{
  if (!mListener)
  {
    // add an event listener to the document
    nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
    nsCOMPtr<nsIDocument> document;
    shell->GetDocument(getter_AddRefs(document));

    // use AddEventListener from the nsIDOMEventTarget interface
    nsCOMPtr<nsIDOMEventTarget> target;
    if (NS_SUCCEEDED(document->QueryInterface(NS_GET_IID(nsIDOMEventTarget), getter_AddRefs(target))) && target)
    {
      nsresult rv = NS_OK;
      // we're a DOMEventListener now!!
      nsCOMPtr<nsIDOMEventListener> listener;
      rv = this->QueryInterface( NS_GET_IID(nsIDOMEventListener), getter_AddRefs(listener) );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to QI");
      // capture DOM focus events 
      rv = target->AddEventListener( NS_LITERAL_STRING("focus") , listener, PR_TRUE );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      // capture Form change events 
      rv = target->AddEventListener( NS_LITERAL_STRING("change") , listener, PR_TRUE );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      // add ourself as a CheckboxStateChange listener ( custom event fired in nsHTMLInputElement.cpp )
      rv = target->AddEventListener( NS_LITERAL_STRING("CheckboxStateChange") , listener, PR_TRUE );
      NS_ASSERTION(NS_SUCCEEDED(rv), "failed to register listener");
      // add ourself as a RadiobuttonStateChange listener ( custom event fired in nsHTMLInputElement.cpp )
      rv = target->AddEventListener( NS_LITERAL_STRING("RadiobuttonStateChange") , listener, PR_TRUE );
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
     nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
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

// --------------- nsIDOMEventListener Methods (3) ------------------------

NS_IMETHODIMP nsRootAccessible::HandleEvent(nsIDOMEvent* aEvent)
{
  if (mListener) {
    nsCOMPtr<nsIDOMEventTarget> t;
    aEvent->GetOriginalTarget(getter_AddRefs(t));
  
    nsCOMPtr<nsIContent> content(do_QueryInterface(t));  
    if (!content)
      return NS_OK;

    nsAutoString eventType;
    aEvent->GetType(eventType);

    // the "focus" type is pulled from nsDOMEvent.cpp
    if ( eventType.EqualsIgnoreCase("focus") ) {
      if (mCurrentFocus == content)
        return NS_OK;
      mCurrentFocus = content;
    }

    nsIFrame* frame = nsnull;
    nsCOMPtr<nsIPresShell> shell(do_QueryReferent(mPresShell));
    shell->GetPrimaryFrameFor(content, &frame);
    if (!frame)
      return NS_OK;

    nsCOMPtr<nsIAccessible> a(do_QueryInterface(frame));
    if (!a)
      a = do_QueryInterface(content);

    if (!a) {
      // is it a link?
      nsCOMPtr<nsILink> link(do_QueryInterface(content));
      if (link) {
        #ifdef DEBUG
        printf("focus link!\n");
        #endif
        nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content));
        if (node)
          a = new nsHTMLLinkAccessible(shell, node);
      }
    }

    if (a) {
      nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content)); 
      nsCOMPtr<nsIAccessible> na(CreateNewAccessible(a, node, mPresShell));
      if ( !na ) 
        return NS_OK;

      if ( eventType.EqualsIgnoreCase("focus") ) {
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_FOCUS, na);
      }
      else if ( eventType.EqualsIgnoreCase("change") ) {
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, na);
      }
      else if ( eventType.EqualsIgnoreCase("CheckboxStateChange") ) {
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, na);
      }
      else if ( eventType.EqualsIgnoreCase("RadiobuttonStateChange") ) {
        mListener->HandleEvent(nsIAccessibleEventListener::EVENT_STATE_CHANGE, na);
      }
    }
  }
  return NS_OK;
}

// ------- nsIDOMFocusListener Methods (2) -------------

NS_IMETHODIMP nsRootAccessible::Focus(nsIDOMEvent* aEvent) 
{ 
  return HandleEvent(aEvent);
}

NS_IMETHODIMP nsRootAccessible::Blur(nsIDOMEvent* aEvent) { return NS_OK; }

// ------- nsIDOMFormListener Methods (5) -------------

NS_IMETHODIMP nsRootAccessible::Submit(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Reset(nsIDOMEvent* aEvent) { return NS_OK; }

NS_IMETHODIMP nsRootAccessible::Change(nsIDOMEvent* aEvent)
{
  // get change events when the form elements changes its state, checked->not,
  //  deleted text, new text, change in selection for list/combo boxes
  // this may be the event that we have the individual Accessible objects
  //  handle themselves -- have list/combos figure out the change in selection
  //  have textareas and inputs fire a change of state etc...
  return HandleEvent(aEvent);
}

// gets Select events when text is selected in a textarea or input
NS_IMETHODIMP nsRootAccessible::Select(nsIDOMEvent* aEvent) { return NS_OK; }

// gets Input events when text is entered or deleted in a textarea or input
NS_IMETHODIMP nsRootAccessible::Input(nsIDOMEvent* aEvent) { return NS_OK; }



