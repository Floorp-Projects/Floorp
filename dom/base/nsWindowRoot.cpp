/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/dom/WindowRootBinding.h"
#include "nsCOMPtr.h"
#include "nsWindowRoot.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsString.h"
#include "nsFrameLoaderOwner.h"
#include "nsFrameLoader.h"
#include "nsQueryActor.h"
#include "nsGlobalWindow.h"
#include "nsFocusManager.h"
#include "nsIContent.h"
#include "nsIControllers.h"
#include "nsIController.h"
#include "xpcpublic.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/HTMLInputElement.h"
#include "mozilla/dom/JSWindowActorService.h"

#ifdef MOZ_XUL
#  include "nsXULElement.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsWindowRoot::nsWindowRoot(nsPIDOMWindowOuter* aWindow) {
  mWindow = aWindow;
  mShowFocusRings = StaticPrefs::browser_display_show_focus_rings();
}

nsWindowRoot::~nsWindowRoot() {
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }

  if (XRE_IsContentProcess()) {
    JSWindowActorService::UnregisterChromeEventTarget(this);
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsWindowRoot, mWindow, mListenerManager,
                                      mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsWindowRoot)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsPIWindowRoot)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsWindowRoot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsWindowRoot)

bool nsWindowRoot::DispatchEvent(Event& aEvent, CallerType aCallerType,
                                 ErrorResult& aRv) {
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv = EventDispatcher::DispatchDOMEvent(
      static_cast<EventTarget*>(this), nullptr, &aEvent, nullptr, &status);
  bool retval = !aEvent.DefaultPrevented(aCallerType);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return retval;
}

bool nsWindowRoot::ComputeDefaultWantsUntrusted(ErrorResult& aRv) {
  return false;
}

EventListenerManager* nsWindowRoot::GetOrCreateListenerManager() {
  if (!mListenerManager) {
    mListenerManager =
        new EventListenerManager(static_cast<EventTarget*>(this));
  }

  return mListenerManager;
}

EventListenerManager* nsWindowRoot::GetExistingListenerManager() const {
  return mListenerManager;
}

void nsWindowRoot::GetEventTargetParent(EventChainPreVisitor& aVisitor) {
  aVisitor.mCanHandle = true;
  aVisitor.mForceContentDispatch = true;  // FIXME! Bug 329119
  // To keep mWindow alive
  aVisitor.mItemData = static_cast<nsISupports*>(mWindow);
  aVisitor.SetParentTarget(mParent, false);
}

nsresult nsWindowRoot::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  return NS_OK;
}

nsPIDOMWindowOuter* nsWindowRoot::GetOwnerGlobalForBindingsInternal() {
  return mWindow;
}

nsIGlobalObject* nsWindowRoot::GetOwnerGlobal() const {
  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(mWindow->GetCurrentInnerWindow());
  // We're still holding a ref to it, so returning the raw pointer is ok...
  return global;
}

nsPIDOMWindowOuter* nsWindowRoot::GetWindow() { return mWindow; }

nsresult nsWindowRoot::GetControllers(bool aForVisibleWindow,
                                      nsIControllers** aResult) {
  *aResult = nullptr;

  // XXX: we should fix this so there's a generic interface that
  // describes controllers, so this code would have no special
  // knowledge of what object might have controllers.

  nsFocusManager::SearchRange searchRange =
      aForVisibleWindow ? nsFocusManager::eIncludeVisibleDescendants
                        : nsFocusManager::eIncludeAllDescendants;
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsIContent* focusedContent = nsFocusManager::GetFocusedDescendant(
      mWindow, searchRange, getter_AddRefs(focusedWindow));
  if (focusedContent) {
#ifdef MOZ_XUL
    RefPtr<nsXULElement> xulElement = nsXULElement::FromNode(focusedContent);
    if (xulElement) {
      ErrorResult rv;
      *aResult = xulElement->GetControllers(rv);
      NS_IF_ADDREF(*aResult);
      return rv.StealNSResult();
    }
#endif

    HTMLTextAreaElement* htmlTextArea =
        HTMLTextAreaElement::FromNode(focusedContent);
    if (htmlTextArea) return htmlTextArea->GetControllers(aResult);

    HTMLInputElement* htmlInputElement =
        HTMLInputElement::FromNode(focusedContent);
    if (htmlInputElement) return htmlInputElement->GetControllers(aResult);

    if (focusedContent->IsEditable() && focusedWindow)
      return focusedWindow->GetControllers(aResult);
  } else {
    return focusedWindow->GetControllers(aResult);
  }

  return NS_OK;
}

nsresult nsWindowRoot::GetControllerForCommand(const char* aCommand,
                                               bool aForVisibleWindow,
                                               nsIController** _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  // If this is the parent process, check if a child browsing context from
  // another process is focused, and ask if it has a controller actor that
  // supports the command.
  if (XRE_IsParentProcess()) {
    nsFocusManager* fm = nsFocusManager::GetFocusManager();
    if (!fm) {
      return NS_ERROR_FAILURE;
    }

    // Unfortunately, messages updating the active/focus state in the focus
    // manager don't happen fast enough in the case when switching focus between
    // processes when clicking on a chrome UI element while a child tab is
    // focused, so we need to check whether the focus manager thinks a child
    // frame is focused as well.
    nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
    nsIContent* focusedContent = nsFocusManager::GetFocusedDescendant(
        mWindow, nsFocusManager::eIncludeAllDescendants,
        getter_AddRefs(focusedWindow));
    RefPtr<nsFrameLoaderOwner> loaderOwner = do_QueryObject(focusedContent);
    if (loaderOwner) {
      // Only check browsing contexts if a remote frame is focused. If chrome is
      // focused, just check the controllers directly below.
      RefPtr<nsFrameLoader> frameLoader = loaderOwner->GetFrameLoader();
      if (frameLoader && frameLoader->IsRemoteFrame()) {
        // GetActiveBrowsingContextInChrome actually returns the top-level
        // browsing context if the focus is in a child process tab, or null if
        // the focus is in chrome.
        BrowsingContext* focusedBC =
            fm->GetActiveBrowsingContextInChrome()
                ? fm->GetFocusedBrowsingContextInChrome()
                : nullptr;
        CanonicalBrowsingContext* canonicalFocusedBC =
            CanonicalBrowsingContext::Cast(focusedBC);
        if (canonicalFocusedBC) {
          // At this point, it is known that a child process is focused, so ask
          // its Controllers actor if the command is supported.
          nsCOMPtr<nsIController> controller =
              do_QueryActor("Controllers", canonicalFocusedBC);
          if (controller) {
            bool supported;
            controller->SupportsCommand(aCommand, &supported);
            if (supported) {
              controller.forget(_retval);
              return NS_OK;
            }
          }
        }
      }
    }
  }

  {
    nsCOMPtr<nsIControllers> controllers;
    GetControllers(aForVisibleWindow, getter_AddRefs(controllers));
    if (controllers) {
      nsCOMPtr<nsIController> controller;
      controllers->GetControllerForCommand(aCommand,
                                           getter_AddRefs(controller));
      if (controller) {
        controller.forget(_retval);
        return NS_OK;
      }
    }
  }

  nsFocusManager::SearchRange searchRange =
      aForVisibleWindow ? nsFocusManager::eIncludeVisibleDescendants
                        : nsFocusManager::eIncludeAllDescendants;
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsFocusManager::GetFocusedDescendant(mWindow, searchRange,
                                       getter_AddRefs(focusedWindow));
  while (focusedWindow) {
    nsCOMPtr<nsIControllers> controllers;
    focusedWindow->GetControllers(getter_AddRefs(controllers));
    if (controllers) {
      nsCOMPtr<nsIController> controller;
      controllers->GetControllerForCommand(aCommand,
                                           getter_AddRefs(controller));
      if (controller) {
        controller.forget(_retval);
        return NS_OK;
      }
    }

    // XXXndeakin P3 is this casting safe?
    nsGlobalWindowOuter* win = nsGlobalWindowOuter::Cast(focusedWindow);
    focusedWindow = win->GetPrivateParent();
  }

  return NS_OK;
}

void nsWindowRoot::GetEnabledDisabledCommandsForControllers(
    nsIControllers* aControllers,
    nsTHashtable<nsCStringHashKey>& aCommandsHandled,
    nsTArray<nsCString>& aEnabledCommands,
    nsTArray<nsCString>& aDisabledCommands) {
  uint32_t controllerCount;
  aControllers->GetControllerCount(&controllerCount);
  for (uint32_t c = 0; c < controllerCount; c++) {
    nsCOMPtr<nsIController> controller;
    aControllers->GetControllerAt(c, getter_AddRefs(controller));

    nsCOMPtr<nsICommandController> commandController(
        do_QueryInterface(controller));
    if (commandController) {
      // All of our default command controllers have 20-60 commands.  Let's just
      // leave enough space here for all of them so we probably don't need to
      // heap-allocate.
      AutoTArray<nsCString, 64> commands;
      if (NS_SUCCEEDED(commandController->GetSupportedCommands(commands))) {
        for (auto& commandStr : commands) {
          // Use a hash to determine which commands have already been handled by
          // earlier controllers, as the earlier controller's result should get
          // priority.
          if (aCommandsHandled.EnsureInserted(commandStr)) {
            // We inserted a new entry into aCommandsHandled.
            bool enabled = false;
            controller->IsCommandEnabled(commandStr.get(), &enabled);

            if (enabled) {
              aEnabledCommands.AppendElement(commandStr);
            } else {
              aDisabledCommands.AppendElement(commandStr);
            }
          }
        }
      }
    }
  }
}

void nsWindowRoot::GetEnabledDisabledCommands(
    nsTArray<nsCString>& aEnabledCommands,
    nsTArray<nsCString>& aDisabledCommands) {
  nsTHashtable<nsCStringHashKey> commandsHandled;

  nsCOMPtr<nsIControllers> controllers;
  GetControllers(false, getter_AddRefs(controllers));
  if (controllers) {
    GetEnabledDisabledCommandsForControllers(
        controllers, commandsHandled, aEnabledCommands, aDisabledCommands);
  }

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsFocusManager::GetFocusedDescendant(mWindow,
                                       nsFocusManager::eIncludeAllDescendants,
                                       getter_AddRefs(focusedWindow));
  while (focusedWindow) {
    focusedWindow->GetControllers(getter_AddRefs(controllers));
    if (controllers) {
      GetEnabledDisabledCommandsForControllers(
          controllers, commandsHandled, aEnabledCommands, aDisabledCommands);
    }

    nsGlobalWindowOuter* win = nsGlobalWindowOuter::Cast(focusedWindow);
    focusedWindow = win->GetPrivateParent();
  }
}

already_AddRefed<nsINode> nsWindowRoot::GetPopupNode() {
  nsCOMPtr<nsINode> popupNode = do_QueryReferent(mPopupNode);
  return popupNode.forget();
}

void nsWindowRoot::SetPopupNode(nsINode* aNode) {
  mPopupNode = do_GetWeakReference(aNode);
}

nsIGlobalObject* nsWindowRoot::GetParentObject() {
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject* nsWindowRoot::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return mozilla::dom::WindowRoot_Binding::Wrap(aCx, this, aGivenProto);
}

void nsWindowRoot::AddBrowser(nsIRemoteTab* aBrowser) {
  nsWeakPtr weakBrowser = do_GetWeakReference(aBrowser);
  mWeakBrowsers.PutEntry(weakBrowser);
}

void nsWindowRoot::RemoveBrowser(nsIRemoteTab* aBrowser) {
  nsWeakPtr weakBrowser = do_GetWeakReference(aBrowser);
  mWeakBrowsers.RemoveEntry(weakBrowser);
}

void nsWindowRoot::EnumerateBrowsers(BrowserEnumerator aEnumFunc, void* aArg) {
  // Collect strong references to all browsers in a separate array in
  // case aEnumFunc alters mWeakBrowsers.
  nsTArray<nsCOMPtr<nsIRemoteTab>> remoteTabs;
  for (auto iter = mWeakBrowsers.ConstIter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsIRemoteTab> remoteTab(do_QueryReferent(iter.Get()->GetKey()));
    if (remoteTab) {
      remoteTabs.AppendElement(remoteTab);
    }
  }

  for (uint32_t i = 0; i < remoteTabs.Length(); ++i) {
    aEnumFunc(remoteTabs[i], aArg);
  }
}

///////////////////////////////////////////////////////////////////////////////////

already_AddRefed<EventTarget> NS_NewWindowRoot(nsPIDOMWindowOuter* aWindow) {
  nsCOMPtr<EventTarget> result = new nsWindowRoot(aWindow);

  if (XRE_IsContentProcess()) {
    RefPtr<JSWindowActorService> wasvc = JSWindowActorService::GetSingleton();
    wasvc->RegisterChromeEventTarget(result);
  }

  return result.forget();
}
