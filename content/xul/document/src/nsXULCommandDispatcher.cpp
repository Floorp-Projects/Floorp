/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  This file provides the implementation for the XUL Command Dispatcher.

 */

#include "nsIContent.h"
#include "nsFocusManager.h"
#include "nsIControllers.h"
#include "nsIDOMDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsRDFCID.h"
#include "nsXULCommandDispatcher.h"
#include "prlog.h"
#include "nsIDOMEventTarget.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsDOMError.h"
#include "nsEventDispatcher.h"

#ifdef PR_LOGGING
static PRLogModuleInfo* gLog;
#endif

////////////////////////////////////////////////////////////////////////

nsXULCommandDispatcher::nsXULCommandDispatcher(nsIDocument* aDocument)
    : mDocument(aDocument), mUpdaters(nsnull)
{

#ifdef PR_LOGGING
  if (! gLog)
    gLog = PR_NewLogModule("nsXULCommandDispatcher");
#endif
}

nsXULCommandDispatcher::~nsXULCommandDispatcher()
{
  Disconnect();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULCommandDispatcher)

// QueryInterface implementation for nsXULCommandDispatcher

DOMCI_DATA(XULCommandDispatcher, nsXULCommandDispatcher)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULCommandDispatcher)
    NS_INTERFACE_MAP_ENTRY(nsIDOMXULCommandDispatcher)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXULCommandDispatcher)
    NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(XULCommandDispatcher)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULCommandDispatcher)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULCommandDispatcher)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULCommandDispatcher)
  tmp->Disconnect();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULCommandDispatcher)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mDocument)
  Updater* updater = tmp->mUpdaters;
  while (updater) {
    cb.NoteXPCOMChild(updater->mElement);
    updater = updater->mNext;
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void
nsXULCommandDispatcher::Disconnect()
{
  while (mUpdaters) {
    Updater* doomed = mUpdaters;
    mUpdaters = mUpdaters->mNext;
    delete doomed;
  }
  mDocument = nsnull;
}

already_AddRefed<nsPIWindowRoot>
nsXULCommandDispatcher::GetWindowRoot()
{
  if (mDocument) {
    nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(mDocument->GetScriptGlobalObject()));
    if (window) {
      return window->GetTopWindowRoot();
    }
  }

  return nsnull;
}

nsIContent*
nsXULCommandDispatcher::GetRootFocusedContentAndWindow(nsPIDOMWindow** aWindow)
{
  *aWindow = nsnull;

  if (mDocument) {
    nsCOMPtr<nsPIDOMWindow> win = mDocument->GetWindow();
    if (win) {
      nsCOMPtr<nsPIDOMWindow> rootWindow = win->GetPrivateRoot();
      if (rootWindow) {
        return nsFocusManager::GetFocusedDescendant(rootWindow, true, aWindow);
      }
    }
  }

  return nsnull;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetFocusedElement(nsIDOMElement** aElement)
{
  *aElement = nsnull;

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsIContent* focusedContent =
    GetRootFocusedContentAndWindow(getter_AddRefs(focusedWindow));
  if (focusedContent) {
    CallQueryInterface(focusedContent, aElement);

    // Make sure the caller can access the focused element.
    if (!nsContentUtils::CanCallerAccess(*aElement)) {
      // XXX This might want to return null, but we use that return value
      // to mean "there is no focused element," so to be clear, throw an
      // exception.
      NS_RELEASE(*aElement);
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetFocusedWindow(nsIDOMWindow** aWindow)
{
  *aWindow = nsnull;

  nsCOMPtr<nsPIDOMWindow> window;
  GetRootFocusedContentAndWindow(getter_AddRefs(window));
  if (!window)
    return NS_OK;

  // Make sure the caller can access this window. The caller can access this
  // window iff it can access the document.
  nsCOMPtr<nsIDOMDocument> domdoc;
  nsresult rv = window->GetDocument(getter_AddRefs(domdoc));
  NS_ENSURE_SUCCESS(rv, rv);

  // Note: If there is no document, then this window has been cleared and
  // there's nothing left to protect, so let the window pass through.
  if (domdoc && !nsContentUtils::CanCallerAccess(domdoc))
    return NS_ERROR_DOM_SECURITY_ERR;

  CallQueryInterface(window, aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::SetFocusedElement(nsIDOMElement* aElement)
{
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, NS_ERROR_FAILURE);

  if (aElement)
    return fm->SetFocus(aElement, 0);

  // if aElement is null, clear the focus in the currently focused child window
  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  GetRootFocusedContentAndWindow(getter_AddRefs(focusedWindow));
  return fm->ClearFocus(focusedWindow);
}

NS_IMETHODIMP
nsXULCommandDispatcher::SetFocusedWindow(nsIDOMWindow* aWindow)
{
  NS_ENSURE_TRUE(aWindow, NS_OK); // do nothing if set to null

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, NS_ERROR_FAILURE);

  // get the containing frame for the window, and set it as focused. This will
  // end up focusing whatever is currently focused inside the frame. Since
  // setting the command dispatcher's focused window doesn't raise the window,
  // setting it to a top-level window doesn't need to do anything.
  nsCOMPtr<nsIDOMElement> frameElement = window->GetFrameElementInternal();
  if (frameElement)
    return fm->SetFocus(frameElement, 0);

  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::AdvanceFocus()
{
  return AdvanceFocusIntoSubtree(nsnull);
}

NS_IMETHODIMP
nsXULCommandDispatcher::RewindFocus()
{
  nsCOMPtr<nsPIDOMWindow> win;
  GetRootFocusedContentAndWindow(getter_AddRefs(win));

  nsCOMPtr<nsIDOMElement> result;
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    return fm->MoveFocus(win, nsnull, nsIFocusManager::MOVEFOCUS_BACKWARD,
                         0, getter_AddRefs(result));
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::AdvanceFocusIntoSubtree(nsIDOMElement* aElt)
{
  nsCOMPtr<nsPIDOMWindow> win;
  GetRootFocusedContentAndWindow(getter_AddRefs(win));

  nsCOMPtr<nsIDOMElement> result;
  nsIFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm)
    return fm->MoveFocus(win, aElt, nsIFocusManager::MOVEFOCUS_FORWARD,
                         0, getter_AddRefs(result));
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::AddCommandUpdater(nsIDOMElement* aElement,
                                          const nsAString& aEvents,
                                          const nsAString& aTargets)
{
  NS_PRECONDITION(aElement != nsnull, "null ptr");
  if (! aElement)
    return NS_ERROR_NULL_POINTER;

  NS_ENSURE_TRUE(mDocument, NS_ERROR_UNEXPECTED);

  nsresult rv = nsContentUtils::CheckSameOrigin(mDocument, aElement);

  if (NS_FAILED(rv)) {
    return rv;
  }

  Updater* updater = mUpdaters;
  Updater** link = &mUpdaters;

  while (updater) {
    if (updater->mElement == aElement) {

#ifdef DEBUG
      if (PR_LOG_TEST(gLog, PR_LOG_NOTICE)) {
        nsCAutoString eventsC, targetsC, aeventsC, atargetsC; 
        eventsC.AssignWithConversion(updater->mEvents);
        targetsC.AssignWithConversion(updater->mTargets);
        CopyUTF16toUTF8(aEvents, aeventsC);
        CopyUTF16toUTF8(aTargets, atargetsC);
        PR_LOG(gLog, PR_LOG_NOTICE,
               ("xulcmd[%p] replace %p(events=%s targets=%s) with (events=%s targets=%s)",
                this, aElement,
                eventsC.get(),
                targetsC.get(),
                aeventsC.get(),
                atargetsC.get()));
      }
#endif

      // If the updater was already in the list, then replace
      // (?) the 'events' and 'targets' filters with the new
      // specification.
      updater->mEvents  = aEvents;
      updater->mTargets = aTargets;
      return NS_OK;
    }

    link = &(updater->mNext);
    updater = updater->mNext;
  }
#ifdef DEBUG
  if (PR_LOG_TEST(gLog, PR_LOG_NOTICE)) {
    nsCAutoString aeventsC, atargetsC; 
    CopyUTF16toUTF8(aEvents, aeventsC);
    CopyUTF16toUTF8(aTargets, atargetsC);

    PR_LOG(gLog, PR_LOG_NOTICE,
           ("xulcmd[%p] add     %p(events=%s targets=%s)",
            this, aElement,
            aeventsC.get(),
            atargetsC.get()));
  }
#endif

  // If we get here, this is a new updater. Append it to the list.
  updater = new Updater(aElement, aEvents, aTargets);
  if (! updater)
      return NS_ERROR_OUT_OF_MEMORY;

  *link = updater;
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::RemoveCommandUpdater(nsIDOMElement* aElement)
{
  NS_PRECONDITION(aElement != nsnull, "null ptr");
  if (! aElement)
    return NS_ERROR_NULL_POINTER;

  Updater* updater = mUpdaters;
  Updater** link = &mUpdaters;

  while (updater) {
    if (updater->mElement == aElement) {
#ifdef DEBUG
      if (PR_LOG_TEST(gLog, PR_LOG_NOTICE)) {
        nsCAutoString eventsC, targetsC; 
        eventsC.AssignWithConversion(updater->mEvents);
        targetsC.AssignWithConversion(updater->mTargets);
        PR_LOG(gLog, PR_LOG_NOTICE,
               ("xulcmd[%p] remove  %p(events=%s targets=%s)",
                this, aElement,
                eventsC.get(),
                targetsC.get()));
      }
#endif

      *link = updater->mNext;
      delete updater;
      return NS_OK;
    }

    link = &(updater->mNext);
    updater = updater->mNext;
  }

  // Hmm. Not found. Oh well.
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::UpdateCommands(const nsAString& aEventName)
{
  nsAutoString id;
  nsCOMPtr<nsIDOMElement> element;
  GetFocusedElement(getter_AddRefs(element));
  if (element) {
    nsresult rv = element->GetAttribute(NS_LITERAL_STRING("id"), id);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element's id");
    if (NS_FAILED(rv)) return rv;
  }

  nsCOMArray<nsIContent> updaters;

  for (Updater* updater = mUpdaters; updater != nsnull; updater = updater->mNext) {
    // Skip any nodes that don't match our 'events' or 'targets'
    // filters.
    if (! Matches(updater->mEvents, aEventName))
      continue;

    if (! Matches(updater->mTargets, id))
      continue;

    nsCOMPtr<nsIContent> content = do_QueryInterface(updater->mElement);
    NS_ASSERTION(content != nsnull, "not an nsIContent");
    if (! content)
      return NS_ERROR_UNEXPECTED;

    updaters.AppendObject(content);
  }

  for (PRInt32 u = 0; u < updaters.Count(); u++) {
    nsIContent* content = updaters[u];

    nsCOMPtr<nsIDocument> document = content->GetDocument();

    NS_ASSERTION(document != nsnull, "element has no document");
    if (! document)
      continue;

#ifdef DEBUG
    if (PR_LOG_TEST(gLog, PR_LOG_NOTICE)) {
      nsCAutoString aeventnameC; 
      CopyUTF16toUTF8(aEventName, aeventnameC);
      PR_LOG(gLog, PR_LOG_NOTICE,
             ("xulcmd[%p] update %p event=%s",
              this, content,
              aeventnameC.get()));
    }
#endif

    nsCOMPtr<nsIPresShell> shell = document->GetShell();
    if (shell) {
      // Retrieve the context in which our DOM event will fire.
      nsRefPtr<nsPresContext> context = shell->GetPresContext();

      // Handle the DOM event
      nsEventStatus status = nsEventStatus_eIgnore;

      nsEvent event(true, NS_XUL_COMMAND_UPDATE);

      nsEventDispatcher::Dispatch(content, context, &event, nsnull, &status);
    }
  }
  return NS_OK;
}

bool
nsXULCommandDispatcher::Matches(const nsString& aList, 
                                const nsAString& aElement)
{
  if (aList.EqualsLiteral("*"))
    return true; // match _everything_!

  PRInt32 indx = aList.Find(PromiseFlatString(aElement));
  if (indx == -1)
    return false; // not in the list at all

  // okay, now make sure it's not a substring snafu; e.g., 'ur'
  // found inside of 'blur'.
  if (indx > 0) {
    PRUnichar ch = aList[indx - 1];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar(','))
      return false;
  }

  if (indx + aElement.Length() < aList.Length()) {
    PRUnichar ch = aList[indx + aElement.Length()];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar(','))
      return false;
  }

  return true;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetControllers(nsIControllers** aResult)
{
  nsCOMPtr<nsPIWindowRoot> root = GetWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  return root->GetControllers(aResult);
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetControllerForCommand(const char *aCommand, nsIController** _retval)
{
  nsCOMPtr<nsPIWindowRoot> root = GetWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  return root->GetControllerForCommand(aCommand, _retval);
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetSuppressFocusScroll(bool* aSuppressFocusScroll)
{
  *aSuppressFocusScroll = false;
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::SetSuppressFocusScroll(bool aSuppressFocusScroll)
{
  return NS_OK;
}

