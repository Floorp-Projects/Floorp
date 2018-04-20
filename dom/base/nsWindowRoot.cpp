/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/WindowRootBinding.h"
#include "nsCOMPtr.h"
#include "nsWindowRoot.h"
#include "nsPIDOMWindow.h"
#include "nsPresContext.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsString.h"
#include "nsGlobalWindow.h"
#include "nsFocusManager.h"
#include "nsIContent.h"
#include "nsIControllers.h"
#include "nsIController.h"
#include "xpcpublic.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/dom/TabParent.h"
#include "mozilla/dom/HTMLTextAreaElement.h"
#include "mozilla/dom/HTMLInputElement.h"

#ifdef MOZ_XUL
#include "nsXULElement.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

nsWindowRoot::nsWindowRoot(nsPIDOMWindowOuter* aWindow)
{
  mWindow = aWindow;

  // Keyboard indicators are not shown on Mac by default.
#if defined(XP_MACOSX)
  mShowAccelerators = false;
  mShowFocusRings = false;
#else
  mShowAccelerators = true;
  mShowFocusRings = true;
#endif
}

nsWindowRoot::~nsWindowRoot()
{
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(nsWindowRoot,
                                      mWindow,
                                      mListenerManager,
                                      mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsWindowRoot)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsPIWindowRoot)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsWindowRoot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsWindowRoot)

bool
nsWindowRoot::DispatchEvent(Event& aEvent, CallerType aCallerType,
                            ErrorResult& aRv)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =  EventDispatcher::DispatchDOMEvent(
    static_cast<EventTarget*>(this), nullptr, &aEvent, nullptr, &status);
  bool retval = !aEvent.DefaultPrevented(aCallerType);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
  }
  return retval;
}

bool
nsWindowRoot::ComputeDefaultWantsUntrusted(ErrorResult& aRv)
{
  return false;
}

EventListenerManager*
nsWindowRoot::GetOrCreateListenerManager()
{
  if (!mListenerManager) {
    mListenerManager =
      new EventListenerManager(static_cast<EventTarget*>(this));
  }

  return mListenerManager;
}

EventListenerManager*
nsWindowRoot::GetExistingListenerManager() const
{
  return mListenerManager;
}

void
nsWindowRoot::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mForceContentDispatch = true; //FIXME! Bug 329119
  // To keep mWindow alive
  aVisitor.mItemData = static_cast<nsISupports *>(mWindow);
  aVisitor.SetParentTarget(mParent, false);
}

nsresult
nsWindowRoot::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

nsPIDOMWindowOuter*
nsWindowRoot::GetOwnerGlobalForBindings()
{
  return GetWindow();
}

nsIGlobalObject*
nsWindowRoot::GetOwnerGlobal() const
{
  nsCOMPtr<nsIGlobalObject> global =
    do_QueryInterface(mWindow->GetCurrentInnerWindow());
  // We're still holding a ref to it, so returning the raw pointer is ok...
  return global;
}

nsPIDOMWindowOuter*
nsWindowRoot::GetWindow()
{
  return mWindow;
}

nsresult
nsWindowRoot::GetControllers(bool aForVisibleWindow,
                             nsIControllers** aResult)
{
  *aResult = nullptr;

  // XXX: we should fix this so there's a generic interface that
  // describes controllers, so this code would have no special
  // knowledge of what object might have controllers.

  nsFocusManager::SearchRange searchRange =
    aForVisibleWindow ? nsFocusManager::eIncludeVisibleDescendants :
                        nsFocusManager::eIncludeAllDescendants;
  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsIContent* focusedContent =
    nsFocusManager::GetFocusedDescendant(mWindow, searchRange,
                                         getter_AddRefs(focusedWindow));
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
    if (htmlTextArea)
      return htmlTextArea->GetControllers(aResult);

    HTMLInputElement* htmlInputElement =
      HTMLInputElement::FromNode(focusedContent);
    if (htmlInputElement)
      return htmlInputElement->GetControllers(aResult);

    if (focusedContent->IsEditable() && focusedWindow)
      return focusedWindow->GetControllers(aResult);
  }
  else {
    return focusedWindow->GetControllers(aResult);
  }

  return NS_OK;
}

nsresult
nsWindowRoot::GetControllerForCommand(const char* aCommand,
                                      bool aForVisibleWindow,
                                      nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  {
    nsCOMPtr<nsIControllers> controllers;
    GetControllers(aForVisibleWindow, getter_AddRefs(controllers));
    if (controllers) {
      nsCOMPtr<nsIController> controller;
      controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
      if (controller) {
        controller.forget(_retval);
        return NS_OK;
      }
    }
  }

  nsFocusManager::SearchRange searchRange =
    aForVisibleWindow ? nsFocusManager::eIncludeVisibleDescendants :
                        nsFocusManager::eIncludeAllDescendants;
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
    nsGlobalWindowOuter *win = nsGlobalWindowOuter::Cast(focusedWindow);
    focusedWindow = win->GetPrivateParent();
  }

  return NS_OK;
}

void
nsWindowRoot::GetEnabledDisabledCommandsForControllers(nsIControllers* aControllers,
                                                       nsTHashtable<nsCharPtrHashKey>& aCommandsHandled,
                                                       nsTArray<nsCString>& aEnabledCommands,
                                                       nsTArray<nsCString>& aDisabledCommands)
{
  uint32_t controllerCount;
  aControllers->GetControllerCount(&controllerCount);
  for (uint32_t c = 0; c < controllerCount; c++) {
    nsCOMPtr<nsIController> controller;
    aControllers->GetControllerAt(c, getter_AddRefs(controller));

    nsCOMPtr<nsICommandController> commandController(do_QueryInterface(controller));
    if (commandController) {
      uint32_t commandsCount;
      char** commands;
      if (NS_SUCCEEDED(commandController->GetSupportedCommands(&commandsCount, &commands))) {
        for (uint32_t e = 0; e < commandsCount; e++) {
          // Use a hash to determine which commands have already been handled by
          // earlier controllers, as the earlier controller's result should get
          // priority.
          if (aCommandsHandled.EnsureInserted(commands[e])) {
            // We inserted a new entry into aCommandsHandled.
            bool enabled = false;
            controller->IsCommandEnabled(commands[e], &enabled);

            const nsDependentCSubstring commandStr(commands[e], strlen(commands[e]));
            if (enabled) {
              aEnabledCommands.AppendElement(commandStr);
            } else {
              aDisabledCommands.AppendElement(commandStr);
            }
          }
        }

        NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(commandsCount, commands);
      }
    }
  }
}

void
nsWindowRoot::GetEnabledDisabledCommands(nsTArray<nsCString>& aEnabledCommands,
                                         nsTArray<nsCString>& aDisabledCommands)
{
  nsTHashtable<nsCharPtrHashKey> commandsHandled;

  nsCOMPtr<nsIControllers> controllers;
  GetControllers(false, getter_AddRefs(controllers));
  if (controllers) {
    GetEnabledDisabledCommandsForControllers(controllers, commandsHandled,
                                             aEnabledCommands, aDisabledCommands);
  }

  nsCOMPtr<nsPIDOMWindowOuter> focusedWindow;
  nsFocusManager::GetFocusedDescendant(mWindow,
                                       nsFocusManager::eIncludeAllDescendants,
                                       getter_AddRefs(focusedWindow));
  while (focusedWindow) {
    focusedWindow->GetControllers(getter_AddRefs(controllers));
    if (controllers) {
      GetEnabledDisabledCommandsForControllers(controllers, commandsHandled,
                                               aEnabledCommands, aDisabledCommands);
    }

    nsGlobalWindowOuter* win = nsGlobalWindowOuter::Cast(focusedWindow);
    focusedWindow = win->GetPrivateParent();
  }
}

already_AddRefed<nsINode>
nsWindowRoot::GetPopupNode()
{
  nsCOMPtr<nsINode> popupNode = do_QueryReferent(mPopupNode);
  return popupNode.forget();
}

void
nsWindowRoot::SetPopupNode(nsINode* aNode)
{
  mPopupNode = do_GetWeakReference(aNode);
}

nsIGlobalObject*
nsWindowRoot::GetParentObject()
{
  return xpc::NativeGlobal(xpc::PrivilegedJunkScope());
}

JSObject*
nsWindowRoot::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return mozilla::dom::WindowRootBinding::Wrap(aCx, this, aGivenProto);
}

void
nsWindowRoot::AddBrowser(mozilla::dom::TabParent* aBrowser)
{
  nsWeakPtr weakBrowser = do_GetWeakReference(static_cast<nsITabParent*>(aBrowser));
  mWeakBrowsers.PutEntry(weakBrowser);
}

void
nsWindowRoot::RemoveBrowser(mozilla::dom::TabParent* aBrowser)
{
  nsWeakPtr weakBrowser = do_GetWeakReference(static_cast<nsITabParent*>(aBrowser));
  mWeakBrowsers.RemoveEntry(weakBrowser);
}

void
nsWindowRoot::EnumerateBrowsers(BrowserEnumerator aEnumFunc, void* aArg)
{
  // Collect strong references to all browsers in a separate array in
  // case aEnumFunc alters mWeakBrowsers.
  nsTArray<RefPtr<TabParent>> tabParents;
  for (auto iter = mWeakBrowsers.ConstIter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsITabParent> tabParent(do_QueryReferent(iter.Get()->GetKey()));
    if (TabParent* tab = TabParent::GetFrom(tabParent)) {
      tabParents.AppendElement(tab);
    }
  }

  for (uint32_t i = 0; i < tabParents.Length(); ++i) {
    aEnumFunc(tabParents[i], aArg);
  }
}

///////////////////////////////////////////////////////////////////////////////////

already_AddRefed<EventTarget>
NS_NewWindowRoot(nsPIDOMWindowOuter* aWindow)
{
  nsCOMPtr<EventTarget> result = new nsWindowRoot(aWindow);
  return result.forget();
}
