/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 *   Dan Rosen <dr@netscape.com>
 */

#include "nsIContent.h"
#include "nsIControllers.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsFocusController.h"
#include "prlog.h"
#include "nsIDOMEventTarget.h"
#include "nsIEventStateManager.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"

#ifdef INCLUDE_XUL
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#endif

////////////////////////////////////////////////////////////////////////

nsFocusController::nsFocusController(void)
: mSuppressFocus(0), 
  mSuppressFocusScroll(PR_FALSE), 
	mActive(PR_FALSE)
{
	NS_INIT_REFCNT();
}

nsFocusController::~nsFocusController(void)
{
}

NS_IMPL_ISUPPORTS4(nsFocusController, nsIFocusController,
                   nsIDOMFocusListener, nsIDOMEventListener, nsSupportsWeakReference)

NS_IMETHODIMP
nsFocusController::Create(nsIFocusController** aResult)
{
  nsFocusController* controller = new nsFocusController();
  if (!controller)
    return NS_ERROR_OUT_OF_MEMORY;

  *aResult = controller;
  NS_ADDREF(*aResult);
  return NS_OK;
}


////////////////////////////////////////////////////////////////
// nsIFocusController Interface

NS_IMETHODIMP
nsFocusController::GetFocusedElement(nsIDOMElement** aElement)
{
  *aElement = mCurrentElement;
  NS_IF_ADDREF(*aElement);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetFocusedWindow(nsIDOMWindowInternal** aWindow)
{
  *aWindow = mCurrentWindow;
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetFocusedElement(nsIDOMElement* aElement)
{
  mCurrentElement = aElement;

  if (!mSuppressFocus) {
    // Need to update focus commands when focus switches from
    // an element to no element, so don't test mCurrentElement
    // before updating.
    UpdateCommands(NS_LITERAL_STRING("focus"));
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetFocusedWindow(nsIDOMWindowInternal* aWindow)
{
  if (aWindow && (mCurrentWindow != aWindow)) {
    nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface(aWindow);
    if (sgo) {
      nsCOMPtr<nsIDocShell> docShell;
      sgo->GetDocShell(getter_AddRefs(docShell));
      nsCOMPtr<nsIBaseWindow> basewin = do_QueryInterface(docShell);
      if (basewin)
        basewin->SetFocus();
    }
  }
  mCurrentWindow = aWindow;
  return NS_OK;
}


NS_IMETHODIMP
nsFocusController::UpdateCommands(const nsAReadableString& aEventName)
{
  if (mCurrentWindow) {
    mCurrentWindow->UpdateCommands(aEventName);
  }
  else if (mCurrentElement) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    mCurrentElement->GetOwnerDocument(getter_AddRefs(domDoc));
    if (domDoc) {
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc));
 
      nsCOMPtr<nsIScriptGlobalObject> global;
      doc->GetScriptGlobalObject(getter_AddRefs(global));

      nsCOMPtr<nsIDOMWindowInternal> window(do_QueryInterface(global));
      if (window)
        window->UpdateCommands(aEventName);
    }
  }
  return NS_OK;
}

  
NS_IMETHODIMP
nsFocusController::GetControllers(nsIControllers** aResult)
{
  //XXX: we should fix this so there's a generic interface that describes controllers, 
  //     so this code would have no special knowledge of what object might have controllers.
  if (mCurrentElement) {

#ifdef INCLUDE_XUL
    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(mCurrentElement));
    if (xulElement)
      return xulElement->GetControllers(aResult);
#endif

    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> htmlTextArea(do_QueryInterface(mCurrentElement));
    if (htmlTextArea)
      return htmlTextArea->GetControllers(aResult);

    nsCOMPtr<nsIDOMNSHTMLInputElement> htmlInputElement(do_QueryInterface(mCurrentElement));
    if (htmlInputElement)
      return htmlInputElement->GetControllers(aResult);
  }
  else if (mCurrentWindow) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(mCurrentWindow));
    if (domWindow)
      return domWindow->GetControllers(aResult);
  }

  *aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::MoveFocus(PRBool aForward, nsIDOMElement* aElt)
{
  // Obtain the doc that we'll be shifting focus inside.
  nsCOMPtr<nsIDocument> doc;
  nsCOMPtr<nsIContent> content;
  if (aElt) {
    content = do_QueryInterface(aElt);
    content->GetDocument(*getter_AddRefs(doc));
  }
  else {
    if (mCurrentElement) {
      content = do_QueryInterface(mCurrentElement);
      content->GetDocument(*getter_AddRefs(doc));
      content = nsnull;
    }
    else if (mCurrentWindow) {
      nsCOMPtr<nsIDOMDocument> domDoc;
      mCurrentWindow->GetDocument(getter_AddRefs(domDoc));
      doc = do_QueryInterface(domDoc);
    }
  }

  if (!doc)
    // No way to obtain an event state manager.  Give up.
    return NS_OK;


  // Obtain a presentation context
  PRInt32 count = doc->GetNumberOfShells();
  if (count == 0)
    return NS_OK;

  nsCOMPtr<nsIPresShell> shell;
  doc->GetShellAt(0, getter_AddRefs(shell));
  if (!shell)
    return NS_OK;

  // Retrieve the context
  nsCOMPtr<nsIPresContext> presContext;
  shell->GetPresContext(getter_AddRefs(presContext));

  nsCOMPtr<nsIEventStateManager> esm;
  presContext->GetEventStateManager(getter_AddRefs(esm));
  if (esm)
    // Make this ESM shift the focus per our instructions.
    esm->ShiftFocus(aForward, content);

  return NS_OK;
}

/////
// nsIDOMFocusListener
/////

nsresult 
nsFocusController::Focus(nsIDOMEvent* aEvent)
{
  if (mSuppressFocus)
    return NS_OK;

  nsCOMPtr<nsIDOMEventTarget> t;
  aEvent->GetOriginalTarget(getter_AddRefs(t));
  
  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(t);
  if (domElement && (domElement != mCurrentElement)) {
    SetFocusedElement(domElement);

    // Also set focus to our innermost window.
    // XXX Must be done for the Ender case, since ender causes a blur,
    // but we don't hear the subsequent focus to the Ender window.
    nsCOMPtr<nsIDOMDocument> ownerDoc;
    domElement->GetOwnerDocument(getter_AddRefs(ownerDoc));
    nsCOMPtr<nsIDOMWindowInternal> domWindow;
    GetParentWindowFromDocument(ownerDoc, getter_AddRefs(domWindow));
    if (domWindow)
      SetFocusedWindow(domWindow);
  }
  else {
    // We're focusing a window.  We only want to do an update commands
    // if no element is focused.
    nsCOMPtr<nsIDOMWindowInternal> domWindow;
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(t);
    if (domDoc) {
      GetParentWindowFromDocument(domDoc, getter_AddRefs(domWindow));
      if (domWindow) {
        SetFocusedWindow(domWindow);
        if (mCurrentElement) {
          // Make sure this element is in our window. If not, we
          // should clear this field.
          nsCOMPtr<nsIDOMDocument> ownerDoc;
          mCurrentElement->GetOwnerDocument(getter_AddRefs(ownerDoc));
          nsCOMPtr<nsIDOMDocument> windowDoc;
          mCurrentWindow->GetDocument(getter_AddRefs(windowDoc));
          if (ownerDoc != windowDoc)
            mCurrentElement = nsnull;
        }

        if (!mCurrentElement)
          UpdateCommands(NS_LITERAL_STRING("focus"));
      }
    }
  }

  return NS_OK;
}

nsresult 
nsFocusController::Blur(nsIDOMEvent* aEvent)
{
  if (mSuppressFocus)
    return NS_OK;

  nsCOMPtr<nsIDOMEventTarget> t;
  aEvent->GetOriginalTarget(getter_AddRefs(t));

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(t);
  if (domElement) {
    SetFocusedElement(nsnull);
  }
  
  nsCOMPtr<nsIDOMWindowInternal> domWindow;
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(t);
  if (domDoc) {
    GetParentWindowFromDocument(domDoc, getter_AddRefs(domWindow));
    if (domWindow)
      SetFocusedWindow(nsnull);
  }

  return NS_OK;
}

nsresult
nsFocusController::GetParentWindowFromDocument(nsIDOMDocument* aDocument, nsIDOMWindowInternal** aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);

  nsCOMPtr<nsIDocument> objectOwner = do_QueryInterface(aDocument);
  if(!objectOwner) return NS_OK;

  nsCOMPtr<nsIScriptGlobalObject> globalObject;
  objectOwner->GetScriptGlobalObject(getter_AddRefs(globalObject));
  if(!globalObject) return NS_OK;

  nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(globalObject);
  *aWindow = domWindow;
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetControllerForCommand(const nsAReadableString& aCommand, nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);	
  *_retval = nsnull;

  nsCOMPtr<nsIControllers> controllers;
  GetControllers(getter_AddRefs(controllers));
  if(controllers) {
    nsCOMPtr<nsIController> controller;
    controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
    if(controller) {
      *_retval = controller;
      NS_ADDREF(*_retval);
      return NS_OK;
    }
  }
  
  nsCOMPtr<nsPIDOMWindow> currentWindow;
  if (mCurrentElement) {
    // Move up to the window.
    nsCOMPtr<nsIDOMDocument> domDoc;
    mCurrentElement->GetOwnerDocument(getter_AddRefs(domDoc));
    nsCOMPtr<nsIDOMWindowInternal> domWindow;
    GetParentWindowFromDocument(domDoc, getter_AddRefs(domWindow));
    currentWindow = do_QueryInterface(domWindow);
  }
  else if (mCurrentWindow) {
    nsCOMPtr<nsPIDOMWindow> privateWin = do_QueryInterface(mCurrentWindow);
    privateWin->GetPrivateParent(getter_AddRefs(currentWindow));
  }
  else return NS_OK;

  while(currentWindow) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(currentWindow);
    if(domWindow) {
      nsCOMPtr<nsIControllers> controllers2;
      domWindow->GetControllers(getter_AddRefs(controllers2));
      if(controllers2) {
        nsCOMPtr<nsIController> controller;
        controllers2->GetControllerForCommand(aCommand, getter_AddRefs(controller));
        if(controller) {
          *_retval = controller;
          NS_ADDREF(*_retval);
          return NS_OK;
        }
      }
    } 
    nsCOMPtr<nsPIDOMWindow> parentPWindow = currentWindow;
    parentPWindow->GetPrivateParent(getter_AddRefs(currentWindow));
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetSuppressFocusScroll(PRBool* aSuppressFocusScroll)
{
  *aSuppressFocusScroll = mSuppressFocusScroll;
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetSuppressFocusScroll(PRBool aSuppressFocusScroll)
{
  mSuppressFocusScroll = aSuppressFocusScroll;
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetSuppressFocus(PRBool* aSuppressFocus)
{
  *aSuppressFocus = (mSuppressFocus > 0);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetSuppressFocus(PRBool aSuppressFocus, const char* aReason)
{
  if(aSuppressFocus) {
    ++mSuppressFocus;
#ifdef DEBUG_hyatt
    printf("[%p] SuppressFocus incremented to %d. The reason is %s.\n", this, mSuppressFocus, aReason);
#endif
  }
  else if(mSuppressFocus > 0) {
    --mSuppressFocus;
#ifdef DEBUG_hyatt
    printf("[%p] SuppressFocus decremented to %d. The reason is %s.\n", this, mSuppressFocus, aReason);
#endif
  }
  else 
    NS_ASSERTION(PR_FALSE, "Attempt to decrement focus controller's suppression when no suppression active!\n");

  // we are unsuppressing after activating, so update focus-related commands
  // we need this to update commands in the case where an element is focused.
  if (!mSuppressFocus && mCurrentElement)
    UpdateCommands(NS_LITERAL_STRING("focus"));
  
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetActive(PRBool* aActive)
{
  *aActive = mActive;
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetActive(PRBool aActive)
{
  mActive = aActive;
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::GetPopupNode(nsIDOMNode** aNode)
{
#ifdef DEBUG_dr
  printf("dr :: nsFocusController::GetPopupNode\n");
#endif

  *aNode = mPopupNode;
  NS_IF_ADDREF(*aNode);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetPopupNode(nsIDOMNode* aNode)
{
#ifdef DEBUG_dr
  printf("dr :: nsFocusController::SetPopupNode\n");
#endif

  mPopupNode = aNode;
  return NS_OK;
}

  
