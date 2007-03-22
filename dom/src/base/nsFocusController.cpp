/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 *   Dan Rosen <dr@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIContent.h"
#include "nsIControllers.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMNSEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsFocusController.h"
#include "prlog.h"
#include "nsIDOMEventTarget.h"
#include "nsIEventStateManager.h"
#include "nsIDocShell.h"
#include "nsIBaseWindow.h"
#include "nsIWindowWatcher.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsGlobalWindow.h"

#ifdef MOZ_XUL
#include "nsIDOMXULDocument.h"
#include "nsIDOMXULElement.h"
#endif

////////////////////////////////////////////////////////////////////////

nsFocusController::nsFocusController(void)
: mSuppressFocus(0), 
  mSuppressFocusScroll(PR_FALSE), 
  mActive(PR_FALSE),
  mUpdateWindowWatcher(PR_FALSE),
  mNeedUpdateCommands(PR_FALSE)
{
}

nsFocusController::~nsFocusController(void)
{
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsFocusController)

NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsFocusController, nsIFocusController)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsFocusController,
                                           nsIFocusController)

NS_INTERFACE_MAP_BEGIN(nsFocusController)
  NS_INTERFACE_MAP_ENTRY(nsIFocusController)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
  NS_INTERFACE_MAP_ENTRY(nsSupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIFocusController)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsFocusController)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_0(nsFocusController)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsFocusController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCurrentElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mCurrentWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPopupNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mPopupEvent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

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
  mNeedUpdateCommands = mNeedUpdateCommands || mCurrentElement != aElement;
  mCurrentElement = aElement;

  if (!mSuppressFocus) {
    // Need to update focus commands when focus switches from
    // an element to no element, so don't test mCurrentElement
    // before updating.
    UpdateCommands();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetFocusedWindow(nsIDOMWindowInternal* aWindow)
{
  nsCOMPtr<nsPIDOMWindow> win = do_QueryInterface(aWindow);

  if (win) {
    win = win->GetOuterWindow();
  }

  NS_ASSERTION(!win || !win->IsInnerWindow(),
               "Uh, inner window can't have focus!");

  if (win && (mCurrentWindow != win)) {
    nsCOMPtr<nsIBaseWindow> basewin = do_QueryInterface(win->GetDocShell());
    if (basewin)
      basewin->SetFocus();
  }

  mNeedUpdateCommands = mNeedUpdateCommands || mCurrentWindow != win;
  mCurrentWindow = win;

  if (mUpdateWindowWatcher) {
    NS_ASSERTION(mActive, "This shouldn't happen");
    if (mCurrentWindow)
      UpdateWWActiveWindow();
    mUpdateWindowWatcher = PR_FALSE;
  }

  return NS_OK;
}


void
nsFocusController::UpdateCommands()
{
  if (!mNeedUpdateCommands) {
    return;
  }
  nsCOMPtr<nsIDOMWindowInternal> window;
  nsCOMPtr<nsIDocument> doc;
  if (mCurrentWindow) {
    window = mCurrentWindow;
    nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(window));
    nsCOMPtr<nsIDOMDocument> domDoc;
    domWin->GetDocument(getter_AddRefs(domDoc));
    doc = do_QueryInterface(domDoc);
  }
  else if (mCurrentElement) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    mCurrentElement->GetOwnerDocument(getter_AddRefs(domDoc));
    if (domDoc) {
      doc = do_QueryInterface(domDoc);
      window = do_QueryInterface(doc->GetScriptGlobalObject());
    }
  }

  // If there is no presshell, it's a zombie document which can't handle the command updates
  if (window && doc && doc->GetNumberOfShells()) {
    // Not a zombie document, so we can handle the command update
    window->UpdateCommands(NS_LITERAL_STRING("focus"));
    mNeedUpdateCommands = PR_FALSE;
  }
}

  
NS_IMETHODIMP
nsFocusController::GetControllers(nsIControllers** aResult)
{
  // XXX: we should fix this so there's a generic interface that
  // describes controllers, so this code would have no special
  // knowledge of what object might have controllers.
  if (mCurrentElement) {

#ifdef MOZ_XUL
    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(mCurrentElement));
    if (xulElement)
      return xulElement->GetControllers(aResult);
#endif

    nsCOMPtr<nsIDOMNSHTMLTextAreaElement> htmlTextArea =
      do_QueryInterface(mCurrentElement);
    if (htmlTextArea)
      return htmlTextArea->GetControllers(aResult);

    nsCOMPtr<nsIDOMNSHTMLInputElement> htmlInputElement =
      do_QueryInterface(mCurrentElement);
    if (htmlInputElement)
      return htmlInputElement->GetControllers(aResult);
  }
  else if (mCurrentWindow) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow =
      do_QueryInterface(mCurrentWindow);
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
    doc = content->GetDocument();
  }
  else {
    if (mCurrentElement) {
      content = do_QueryInterface(mCurrentElement);
      doc = content->GetDocument();
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

  nsIPresShell *shell = doc->GetShellAt(0);
  if (!shell)
    return NS_OK;

  // Make sure frames have been constructed before shifting focus, bug 273092.
  shell->FlushPendingNotifications(Flush_Frames);

  // Retrieve the context
  nsCOMPtr<nsPresContext> presContext = shell->GetPresContext();

  // Make this ESM shift the focus per our instructions.
  presContext->EventStateManager()->ShiftFocus(aForward, content);

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

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));
  if (nsevent) {
    nsevent->GetOriginalTarget(getter_AddRefs(t));
  }

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(t);
  if (domElement && (domElement != mCurrentElement)) {
    SetFocusedElement(domElement);

    // Also set focus to our innermost window.
    // XXX Must be done for the Ender case, since ender causes a blur,
    // but we don't hear the subsequent focus to the Ender window.
    nsCOMPtr<nsIDOMDocument> ownerDoc;
    domElement->GetOwnerDocument(getter_AddRefs(ownerDoc));
    nsCOMPtr<nsIDOMWindowInternal> domWindow = GetWindowFromDocument(ownerDoc);
    if (domWindow)
      SetFocusedWindow(domWindow);
  }
  else {
    // We're focusing a window.  We only want to do an update commands
    // if no element is focused.
    nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(t);
    if (domDoc) {
      nsCOMPtr<nsIDOMWindowInternal> domWindow = GetWindowFromDocument(domDoc);
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

        if (!mCurrentElement) {
          UpdateCommands();
        }
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

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));

  if (nsevent) {
    nsevent->GetOriginalTarget(getter_AddRefs(t));
  }

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(t);
  if (domElement) {
    SetFocusedElement(nsnull);
  }
  
  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(t);
  if (domDoc) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow = GetWindowFromDocument(domDoc);
    if (domWindow)
      SetFocusedWindow(nsnull);
  }

  return NS_OK;
}

nsPIDOMWindow *
nsFocusController::GetWindowFromDocument(nsIDOMDocument* aDocument)
{
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDocument);
  if (!doc)
    return NS_OK;

  return doc->GetWindow();
}

NS_IMETHODIMP
nsFocusController::GetControllerForCommand(const char * aCommand,
                                           nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);	
  *_retval = nsnull;

  nsCOMPtr<nsIControllers> controllers;
  nsCOMPtr<nsIController> controller;

  GetControllers(getter_AddRefs(controllers));
  if(controllers) {
    controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
    if(controller) {
      controller.swap(*_retval);
      return NS_OK;
    }
  }
  
  nsCOMPtr<nsPIDOMWindow> currentWindow;
  if (mCurrentElement) {
    // Move up to the window.
    nsCOMPtr<nsIDOMDocument> domDoc;
    mCurrentElement->GetOwnerDocument(getter_AddRefs(domDoc));
    currentWindow = do_QueryInterface(GetWindowFromDocument(domDoc));
  }
  else if (mCurrentWindow) {
    nsGlobalWindow *win =
      NS_STATIC_CAST(nsGlobalWindow *,
                     NS_STATIC_CAST(nsIDOMWindowInternal *, mCurrentWindow));
    currentWindow = win->GetPrivateParent();
  }
  else return NS_OK;

  while(currentWindow) {
    nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(currentWindow));

    nsCOMPtr<nsIControllers> controllers2;
    domWindow->GetControllers(getter_AddRefs(controllers2));
    if(controllers2) {
      controllers2->GetControllerForCommand(aCommand,
                                            getter_AddRefs(controller));
      if(controller) {
        controller.swap(*_retval);
        return NS_OK;
      }
    }

    nsGlobalWindow *win =
      NS_STATIC_CAST(nsGlobalWindow *,
                     NS_STATIC_CAST(nsIDOMWindowInternal *, currentWindow));
    currentWindow = win->GetPrivateParent();
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
    //#ifdef DEBUG_hyatt
    //printf("[%p] SuppressFocus incremented to %d. The reason is %s.\n", this, mSuppressFocus, aReason);
    //#endif
  }
  else if(mSuppressFocus > 0) {
    --mSuppressFocus;
    //#ifdef DEBUG_hyatt
    //printf("[%p] SuppressFocus decremented to %d. The reason is %s.\n", this, mSuppressFocus, aReason);
    //#endif
  }
  else 
    NS_ASSERTION(PR_FALSE, "Attempt to decrement focus controller's suppression when no suppression active!\n");

  // we are unsuppressing after activating, so update focus-related commands
  // we need this to update command, including the case where there is no element
  // because nsPresShell::UnsuppressPainting may have just now unsuppressed
  // focus on the currently focused window
  if (!mSuppressFocus) {
    // Always update commands if we have a current element
    // and mNeedUpdateCommands is true (checked in nsFC::UpdateCommands)
    UpdateCommands();
  }
  
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

  // We may be activated before we ever have a focused window set.
  // This happens on window creation, where the FocusController
  // is activated just prior to setting the focused window.
  // (see nsEventStateManager::PreHandleEvent/NS_ACTIVATE)
  // If this is the case, we need to queue a notification of the
  // WindowWatcher until SetFocusedWindow is called.
  if (mActive) {
    if (mCurrentWindow)
      UpdateWWActiveWindow();
    else
      mUpdateWindowWatcher = PR_TRUE;
  } else
    mUpdateWindowWatcher = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::ResetElementFocus()
{
  mCurrentElement = nsnull;
  return NS_OK;
}

void
nsFocusController::UpdateWWActiveWindow()
{
  // Inform the window watcher of the new active window.
  nsCOMPtr<nsIWindowWatcher> wwatch = do_GetService("@mozilla.org/embedcomp/window-watcher;1");
  if (!wwatch) return;

  // This gets the toplevel DOMWindow
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem =
    do_QueryInterface(mCurrentWindow->GetDocShell());
  if (!docShellAsItem) return;

  nsCOMPtr<nsIDocShellTreeItem> rootItem;
  docShellAsItem->GetRootTreeItem(getter_AddRefs(rootItem));
  NS_ASSERTION(rootItem, "Invalid docshell tree - no root!");

  nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(rootItem);
  wwatch->SetActiveWindow(domWin);
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

NS_IMETHODIMP
nsFocusController::GetPopupEvent(nsIDOMEvent** aEvent)
{
  *aEvent = mPopupEvent;
  NS_IF_ADDREF(*aEvent);
  return NS_OK;
}

NS_IMETHODIMP
nsFocusController::SetPopupEvent(nsIDOMEvent* aEvent)
{
  mPopupEvent = aEvent;
  return NS_OK;
}

  
