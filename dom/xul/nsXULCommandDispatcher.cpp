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
#include "mozilla/dom/Document.h"
#include "nsPresContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsPIWindowRoot.h"
#include "nsXULCommandDispatcher.h"
#include "mozilla/Logging.h"
#include "nsContentUtils.h"
#include "nsReadableUtils.h"
#include "nsCRT.h"
#include "nsError.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ElementBinding.h"

using namespace mozilla;
using mozilla::dom::Document;
using mozilla::dom::Element;

#ifdef DEBUG
static LazyLogModule gCommandLog("nsXULCommandDispatcher");
#endif

////////////////////////////////////////////////////////////////////////

nsXULCommandDispatcher::nsXULCommandDispatcher(Document* aDocument)
    : mDocument(aDocument), mUpdaters(nullptr), mLocked(false) {}

nsXULCommandDispatcher::~nsXULCommandDispatcher() { Disconnect(); }

// QueryInterface implementation for nsXULCommandDispatcher

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXULCommandDispatcher)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXULCommandDispatcher)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMXULCommandDispatcher)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXULCommandDispatcher)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXULCommandDispatcher)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsXULCommandDispatcher)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsXULCommandDispatcher)
  tmp->Disconnect();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_WEAK_REFERENCE
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsXULCommandDispatcher)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDocument)
  Updater* updater = tmp->mUpdaters;
  while (updater) {
    cb.NoteXPCOMChild(updater->mElement);
    updater = updater->mNext;
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

void nsXULCommandDispatcher::Disconnect() {
  while (mUpdaters) {
    Updater* doomed = mUpdaters;
    mUpdaters = mUpdaters->mNext;
    delete doomed;
  }
  mDocument = nullptr;
}

already_AddRefed<nsPIWindowRoot> nsXULCommandDispatcher::GetWindowRoot() {
  if (mDocument) {
    if (nsCOMPtr<nsPIDOMWindowOuter> window = mDocument->GetWindow()) {
      return window->GetTopWindowRoot();
    }
  }

  return nullptr;
}

Element* nsXULCommandDispatcher::GetRootFocusedContentAndWindow(
    nsPIDOMWindowOuter** aWindow) {
  *aWindow = nullptr;

  if (!mDocument) {
    return nullptr;
  }

  if (nsCOMPtr<nsPIDOMWindowOuter> win = mDocument->GetWindow()) {
    if (nsCOMPtr<nsPIDOMWindowOuter> rootWindow = win->GetPrivateRoot()) {
      return nsFocusManager::GetFocusedDescendant(
          rootWindow, nsFocusManager::eIncludeAllDescendants, aWindow);
    }
  }

  return nullptr;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetFocusedElement(Element** aElement) {
  *aElement = nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  RefPtr<Element> focusedContent =
      GetRootFocusedContentAndWindow(getter_AddRefs(focusedWindow));
  if (focusedContent) {
    // Make sure the caller can access the focused element.
    if (!nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller()->Subsumes(
            focusedContent->NodePrincipal())) {
      // XXX This might want to return null, but we use that return value
      // to mean "there is no focused element," so to be clear, throw an
      // exception.
      return NS_ERROR_DOM_SECURITY_ERR;
    }
  }

  focusedContent.forget(aElement);
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetFocusedWindow(mozIDOMWindowProxy** aWindow) {
  *aWindow = nullptr;

  nsCOMPtr<nsPIDOMWindowOuter> window;
  GetRootFocusedContentAndWindow(getter_AddRefs(window));
  if (!window) return NS_OK;

  // Make sure the caller can access this window. The caller can access this
  // window iff it can access the document.
  nsCOMPtr<Document> doc = window->GetDoc();

  // Note: If there is no document, then this window has been cleared and
  // there's nothing left to protect, so let the window pass through.
  if (doc && !nsContentUtils::CanCallerAccess(doc))
    return NS_ERROR_DOM_SECURITY_ERR;

  window.forget(aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::SetFocusedElement(Element* aElement) {
  RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, NS_ERROR_FAILURE);

  if (aElement) {
    return fm->SetFocus(aElement, 0);
  }

  // if aElement is null, clear the focus in the currently focused child window
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  GetRootFocusedContentAndWindow(getter_AddRefs(focusedWindow));
  return fm->ClearFocus(focusedWindow);
}

NS_IMETHODIMP
nsXULCommandDispatcher::SetFocusedWindow(mozIDOMWindowProxy* aWindow) {
  NS_ENSURE_TRUE(aWindow, NS_OK);  // do nothing if set to null

  nsCOMPtr<nsPIDOMWindowOuter> window = nsPIDOMWindowOuter::From(aWindow);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

  RefPtr<nsFocusManager> fm = nsFocusManager::GetFocusManager();
  NS_ENSURE_TRUE(fm, NS_ERROR_FAILURE);

  // get the containing frame for the window, and set it as focused. This will
  // end up focusing whatever is currently focused inside the frame. Since
  // setting the command dispatcher's focused window doesn't raise the window,
  // setting it to a top-level window doesn't need to do anything.
  RefPtr<Element> frameElement = window->GetFrameElementInternal();
  if (frameElement) {
    return fm->SetFocus(frameElement, 0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::AdvanceFocus() {
  return AdvanceFocusIntoSubtree(nullptr);
}

NS_IMETHODIMP
nsXULCommandDispatcher::AdvanceFocusIntoSubtree(Element* aElt) {
  return MoveFocusIntoSubtree(aElt, /* aForward = */ true);
}

NS_IMETHODIMP
nsXULCommandDispatcher::RewindFocus() {
  return MoveFocusIntoSubtree(nullptr, /* aForward = */ false);
}

nsresult nsXULCommandDispatcher::MoveFocusIntoSubtree(Element* aElt,
                                                      bool aForward) {
  nsCOMPtr<nsPIDOMWindowOuter> win;
  GetRootFocusedContentAndWindow(getter_AddRefs(win));

  RefPtr<Element> result;
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (!fm) {
    return NS_OK;
  }
  auto flags = nsFocusManager::ProgrammaticFocusFlags(dom::FocusOptions()) |
               nsIFocusManager::FLAG_BYMOVEFOCUS;
  auto type = aForward ? nsIFocusManager::MOVEFOCUS_FORWARD
                       : nsIFocusManager::MOVEFOCUS_BACKWARD;
  return fm->MoveFocus(win, aElt, type, flags, getter_AddRefs(result));
}

NS_IMETHODIMP
nsXULCommandDispatcher::AddCommandUpdater(Element* aElement,
                                          const nsAString& aEvents,
                                          const nsAString& aTargets) {
  MOZ_ASSERT(aElement != nullptr, "null ptr");
  if (!aElement) return NS_ERROR_NULL_POINTER;

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
      if (MOZ_LOG_TEST(gCommandLog, LogLevel::Debug)) {
        nsAutoCString eventsC, targetsC, aeventsC, atargetsC;
        LossyCopyUTF16toASCII(updater->mEvents, eventsC);
        LossyCopyUTF16toASCII(updater->mTargets, targetsC);
        CopyUTF16toUTF8(aEvents, aeventsC);
        CopyUTF16toUTF8(aTargets, atargetsC);
        MOZ_LOG(gCommandLog, LogLevel::Debug,
                ("xulcmd[%p] replace %p(events=%s targets=%s) with (events=%s "
                 "targets=%s)",
                 this, aElement, eventsC.get(), targetsC.get(), aeventsC.get(),
                 atargetsC.get()));
      }
#endif

      // If the updater was already in the list, then replace
      // (?) the 'events' and 'targets' filters with the new
      // specification.
      updater->mEvents = aEvents;
      updater->mTargets = aTargets;
      return NS_OK;
    }

    link = &(updater->mNext);
    updater = updater->mNext;
  }
#ifdef DEBUG
  if (MOZ_LOG_TEST(gCommandLog, LogLevel::Debug)) {
    nsAutoCString aeventsC, atargetsC;
    CopyUTF16toUTF8(aEvents, aeventsC);
    CopyUTF16toUTF8(aTargets, atargetsC);

    MOZ_LOG(gCommandLog, LogLevel::Debug,
            ("xulcmd[%p] add     %p(events=%s targets=%s)", this, aElement,
             aeventsC.get(), atargetsC.get()));
  }
#endif

  // If we get here, this is a new updater. Append it to the list.
  *link = new Updater(aElement, aEvents, aTargets);
  return NS_OK;
}

NS_IMETHODIMP
nsXULCommandDispatcher::RemoveCommandUpdater(Element* aElement) {
  MOZ_ASSERT(aElement != nullptr, "null ptr");
  if (!aElement) return NS_ERROR_NULL_POINTER;

  Updater* updater = mUpdaters;
  Updater** link = &mUpdaters;

  while (updater) {
    if (updater->mElement == aElement) {
#ifdef DEBUG
      if (MOZ_LOG_TEST(gCommandLog, LogLevel::Debug)) {
        nsAutoCString eventsC, targetsC;
        LossyCopyUTF16toASCII(updater->mEvents, eventsC);
        LossyCopyUTF16toASCII(updater->mTargets, targetsC);
        MOZ_LOG(gCommandLog, LogLevel::Debug,
                ("xulcmd[%p] remove  %p(events=%s targets=%s)", this, aElement,
                 eventsC.get(), targetsC.get()));
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
nsXULCommandDispatcher::UpdateCommands(const nsAString& aEventName) {
  if (mLocked) {
    if (!mPendingUpdates.Contains(aEventName)) {
      mPendingUpdates.AppendElement(aEventName);
    }

    return NS_OK;
  }

  nsAutoString id;
  RefPtr<Element> element;
  GetFocusedElement(getter_AddRefs(element));
  if (element) {
    element->GetAttr(nsGkAtoms::id, id);
  }

  nsCOMArray<nsIContent> updaters;

  for (Updater* updater = mUpdaters; updater != nullptr;
       updater = updater->mNext) {
    // Skip any nodes that don't match our 'events' or 'targets'
    // filters.
    if (!Matches(updater->mEvents, aEventName)) continue;

    if (!Matches(updater->mTargets, id)) continue;

    nsIContent* content = updater->mElement;
    NS_ASSERTION(content != nullptr, "mElement is null");
    if (!content) return NS_ERROR_UNEXPECTED;

    updaters.AppendObject(content);
  }

  for (nsIContent* content : updaters) {
#ifdef DEBUG
    if (MOZ_LOG_TEST(gCommandLog, LogLevel::Debug)) {
      nsAutoCString aeventnameC;
      CopyUTF16toUTF8(aEventName, aeventnameC);
      MOZ_LOG(
          gCommandLog, LogLevel::Debug,
          ("xulcmd[%p] update %p event=%s", this, content, aeventnameC.get()));
    }
#endif

    WidgetEvent event(true, eXULCommandUpdate);
    EventDispatcher::Dispatch(MOZ_KnownLive(content), nullptr, &event);
  }
  return NS_OK;
}

bool nsXULCommandDispatcher::Matches(const nsString& aList,
                                     const nsAString& aElement) {
  if (aList.EqualsLiteral("*")) return true;  // match _everything_!

  int32_t indx = aList.Find(PromiseFlatString(aElement));
  if (indx == -1) return false;  // not in the list at all

  // okay, now make sure it's not a substring snafu; e.g., 'ur'
  // found inside of 'blur'.
  if (indx > 0) {
    char16_t ch = aList[indx - 1];
    if (!nsCRT::IsAsciiSpace(ch) && ch != char16_t(',')) return false;
  }

  if (indx + aElement.Length() < aList.Length()) {
    char16_t ch = aList[indx + aElement.Length()];
    if (!nsCRT::IsAsciiSpace(ch) && ch != char16_t(',')) return false;
  }

  return true;
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetControllers(nsIControllers** aResult) {
  nsCOMPtr<nsPIWindowRoot> root = GetWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  return root->GetControllers(false /* for any window */, aResult);
}

NS_IMETHODIMP
nsXULCommandDispatcher::GetControllerForCommand(const char* aCommand,
                                                nsIController** _retval) {
  nsCOMPtr<nsPIWindowRoot> root = GetWindowRoot();
  NS_ENSURE_TRUE(root, NS_ERROR_FAILURE);

  return root->GetControllerForCommand(aCommand, false /* for any window */,
                                       _retval);
}

NS_IMETHODIMP
nsXULCommandDispatcher::Lock() {
  // Since locking is used only as a performance optimization, we don't worry
  // about nested lock calls. If that does happen, it just means we will unlock
  // and process updates earlier.
  mLocked = true;
  return NS_OK;
}

// TODO: Convert this to MOZ_CAN_RUN_SCRIPT (bug 1415230)
MOZ_CAN_RUN_SCRIPT_BOUNDARY NS_IMETHODIMP nsXULCommandDispatcher::Unlock() {
  if (mLocked) {
    mLocked = false;

    // Handle any pending updates one at a time. In the unlikely case where a
    // lock is added during the update, break out.
    while (!mLocked && mPendingUpdates.Length() > 0) {
      nsString name = mPendingUpdates.ElementAt(0);
      mPendingUpdates.RemoveElementAt(0);
      UpdateCommands(name);
    }
  }

  return NS_OK;
}
