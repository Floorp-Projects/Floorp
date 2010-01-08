/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

/*

  This file provides the implementation for the XUL Command Dispatcher.

 */

#include "nsIContent.h"
#include "nsIFocusController.h"
#include "nsFocusManager.h"
#include "nsIControllers.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMNSHTMLInputElement.h"
#include "nsIDOMNSHTMLTextAreaElement.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULElement.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
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
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULCommandDispatcher)
    NS_INTERFACE_MAP_ENTRY(nsIDOMXULCommandDispatcher)
    NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXULCommandDispatcher)
    NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULCommandDispatcher)
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

nsIFocusController*
nsXULCommandDispatcher::GetFocusController()
{
  if (!mDocument) {
    return nsnull;
  }

  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(mDocument->GetScriptGlobalObject()));
  return win ? win->GetRootFocusController() : nsnull;
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
        return nsFocusManager::GetFocusedDescendant(rootWindow, PR_TRUE, aWindow);
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

  nsCOMPtr<nsIDOMNode> doc(do_QueryInterface(mDocument));

  nsresult rv = nsContentUtils::CheckSameOrigin(doc, aElement);

  if (NS_FAILED(rv)) {
    return rv;
  }

  Updater* updater = mUpdaters;
  Updater** link = &mUpdaters;

  while (updater) {
    if (updater->mElement == aElement) {

#ifdef NS_DEBUG
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
#ifdef NS_DEBUG
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
#ifdef NS_DEBUG
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
  nsIFocusController* fc = GetFocusController();
  NS_ENSURE_TRUE(fc, NS_ERROR_FAILURE);

  nsAutoString id;
  nsCOMPtr<nsIDOMElement> element;
  GetFocusedElement(getter_AddRefs(element));
  if (element) {
    nsresult rv = element->GetAttribute(NS_LITERAL_STRING("id"), id);
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get element's id");
    if (NS_FAILED(rv)) return rv;
  }

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

    nsCOMPtr<nsIDocument> document = content->GetDocument();

    NS_ASSERTION(document != nsnull, "element has no document");
    if (! document)
      continue;

#ifdef NS_DEBUG
    if (PR_LOG_TEST(gLog, PR_LOG_NOTICE)) {
      nsCAutoString aeventnameC; 
      CopyUTF16toUTF8(aEventName, aeventnameC);
      PR_LOG(gLog, PR_LOG_NOTICE,
             ("xulcmd[%p] update %p event=%s",
              this, updater->mElement.get(),
              aeventnameC.get()));
    }
#endif

    nsCOMPtr<nsIPresShell> shell = document->GetPrimaryShell();
    if (shell) {

      // Retrieve the context in which our DOM event will fire.
      nsCOMPtr<nsPresContext> context = shell->GetPresContext();

      // Handle the DOM event
      nsEventStatus status = nsEventStatus_eIgnore;

      nsEvent event(PR_TRUE, NS_XUL_COMMAND_UPDATE);

      nsEventDispatcher::Dispatch(content, context, &event, nsnull, &status);
    }
  }
  return NS_OK;
}

PRBool
nsXULCommandDispatcher::Matches(const nsString& aList, 
                                const nsAString& aElement)
{
  if (aList.EqualsLiteral("*"))
    return PR_TRUE; // match _everything_!

  PRInt32 indx = aList.Find(PromiseFlatString(aElement));
  if (indx == -1)
    return PR_FALSE; // not in the list at all

  // okay, now make sure it's not a substring snafu; e.g., 'ur'
  // found inside of 'blur'.
  if (indx > 0) {
    PRUnichar ch = aList[indx - 1];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar(','))
      return PR_FALSE;
  }

  if (indx + aElement.Length() < aList.Length()) {
    PRUnichar ch = aList[indx + aElement.Length()];
    if (! nsCRT::IsAsciiSpace(ch) && ch != PRUnichar(','))
      return PR_FALSE;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetControllers(nsIControllers** aResult)
{
  nsIFocusController* fc = GetFocusController();
  NS_ENSURE_TRUE(fc, NS_ERROR_FAILURE);

  return fc->GetControllers(mDocument->GetWindow(), aResult);
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetControllerForCommand(const char *aCommand, nsIController** _retval)
{
  nsIFocusController* fc = GetFocusController();
  NS_ENSURE_TRUE(fc, NS_ERROR_FAILURE);

  return fc->GetControllerForCommand(mDocument->GetWindow(), aCommand, _retval);
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetSuppressFocusScroll(PRBool* aSuppressFocusScroll)
{
  *aSuppressFocusScroll = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::SetSuppressFocusScroll(PRBool aSuppressFocusScroll)
{
  return NS_OK;
}

