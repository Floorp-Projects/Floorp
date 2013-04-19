/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsWindowRoot.h"
#include "nsPIDOMWindow.h"
#include "nsEventListenerManager.h"
#include "nsPresContext.h"
#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsString.h"
#include "nsEventDispatcher.h"
#include "nsGUIEvent.h"
#include "nsGlobalWindow.h"
#include "nsFocusManager.h"
#include "nsIContent.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLTextAreaElement.h"
#include "nsIControllers.h"

#include "nsCycleCollectionParticipant.h"

#ifdef MOZ_XUL
#include "nsIDOMXULElement.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

static NS_DEFINE_CID(kEventListenerManagerCID,    NS_EVENTLISTENERMANAGER_CID);

nsWindowRoot::nsWindowRoot(nsPIDOMWindow* aWindow)
{
  mWindow = aWindow;
}

nsWindowRoot::~nsWindowRoot()
{
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_3(nsWindowRoot,
                                        mListenerManager,
                                        mPopupNode,
                                        mParent)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsWindowRoot)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(nsPIWindowRoot)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsWindowRoot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsWindowRoot)

NS_IMPL_DOMTARGET_DEFAULTS(nsWindowRoot)

NS_IMETHODIMP
nsWindowRoot::RemoveEventListener(const nsAString& aType, nsIDOMEventListener* aListener, bool aUseCapture)
{
  nsRefPtr<nsEventListenerManager> elm = GetListenerManager(false);
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsWindowRoot)

NS_IMETHODIMP
nsWindowRoot::DispatchEvent(nsIDOMEvent* aEvt, bool *aRetVal)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =  nsEventDispatcher::DispatchDOMEvent(
    static_cast<EventTarget*>(this), nullptr, aEvt, nullptr, &status);
  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

nsresult
nsWindowRoot::DispatchDOMEvent(nsEvent* aEvent,
                               nsIDOMEvent* aDOMEvent,
                               nsPresContext* aPresContext,
                               nsEventStatus* aEventStatus)
{
  return nsEventDispatcher::DispatchDOMEvent(static_cast<EventTarget*>(this),
                                             aEvent, aDOMEvent,
                                             aPresContext, aEventStatus);
}

NS_IMETHODIMP
nsWindowRoot::AddEventListener(const nsAString& aType,
                               nsIDOMEventListener *aListener,
                               bool aUseCapture, bool aWantsUntrusted,
                               uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making optional_argc non-zero.");

  nsEventListenerManager* elm = GetListenerManager(true);
  NS_ENSURE_STATE(elm);
  elm->AddEventListener(aType, aListener, aUseCapture, aWantsUntrusted);
  return NS_OK;
}

void
nsWindowRoot::AddEventListener(const nsAString& aType,
                                nsIDOMEventListener* aListener,
                                bool aUseCapture,
                                const Nullable<bool>& aWantsUntrusted,
                                ErrorResult& aRv)
{
  bool wantsUntrusted = !aWantsUntrusted.IsNull() && aWantsUntrusted.Value();
  nsEventListenerManager* elm = GetListenerManager(true);
  if (!elm) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  elm->AddEventListener(aType, aListener, aUseCapture, wantsUntrusted);
}


NS_IMETHODIMP
nsWindowRoot::AddSystemEventListener(const nsAString& aType,
                                     nsIDOMEventListener *aListener,
                                     bool aUseCapture,
                                     bool aWantsUntrusted,
                                     uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making optional_argc non-zero.");

  return NS_AddSystemEventListener(this, aType, aListener, aUseCapture,
                                   aWantsUntrusted);
}

nsEventListenerManager*
nsWindowRoot::GetListenerManager(bool aCreateIfNotFound)
{
  if (!mListenerManager && aCreateIfNotFound) {
    mListenerManager =
      new nsEventListenerManager(static_cast<EventTarget*>(this));
  }

  return mListenerManager;
}

nsIScriptContext*
nsWindowRoot::GetContextForEventHandlers(nsresult* aRv)
{
  *aRv = NS_OK;
  return nullptr;
}

nsresult
nsWindowRoot::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mForceContentDispatch = true; //FIXME! Bug 329119
  // To keep mWindow alive
  aVisitor.mItemData = static_cast<nsISupports *>(mWindow);
  aVisitor.mParentTarget = mParent;
  return NS_OK;
}

nsresult
nsWindowRoot::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

nsPIDOMWindow*
nsWindowRoot::GetWindow()
{
  return mWindow;
}

nsresult
nsWindowRoot::GetControllers(nsIControllers** aResult)
{
  *aResult = nullptr;

  // XXX: we should fix this so there's a generic interface that
  // describes controllers, so this code would have no special
  // knowledge of what object might have controllers.

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsIContent* focusedContent =
    nsFocusManager::GetFocusedDescendant(mWindow, true, getter_AddRefs(focusedWindow));
  if (focusedContent) {
#ifdef MOZ_XUL
    nsCOMPtr<nsIDOMXULElement> xulElement(do_QueryInterface(focusedContent));
    if (xulElement)
      return xulElement->GetControllers(aResult);
#endif

    nsCOMPtr<nsIDOMHTMLTextAreaElement> htmlTextArea =
      do_QueryInterface(focusedContent);
    if (htmlTextArea)
      return htmlTextArea->GetControllers(aResult);

    nsCOMPtr<nsIDOMHTMLInputElement> htmlInputElement =
      do_QueryInterface(focusedContent);
    if (htmlInputElement)
      return htmlInputElement->GetControllers(aResult);

    if (focusedContent->IsEditable() && focusedWindow)
      return focusedWindow->GetControllers(aResult);
  }
  else {
    nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(focusedWindow);
    if (domWindow)
      return domWindow->GetControllers(aResult);
  }

  return NS_OK;
}

nsresult
nsWindowRoot::GetControllerForCommand(const char * aCommand,
                                      nsIController** _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = nullptr;

  {
    nsCOMPtr<nsIControllers> controllers;
    GetControllers(getter_AddRefs(controllers));
    if (controllers) {
      nsCOMPtr<nsIController> controller;
      controllers->GetControllerForCommand(aCommand, getter_AddRefs(controller));
      if (controller) {
        controller.forget(_retval);
        return NS_OK;
      }
    }
  }

  nsCOMPtr<nsPIDOMWindow> focusedWindow;
  nsFocusManager::GetFocusedDescendant(mWindow, true, getter_AddRefs(focusedWindow));
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
    nsGlobalWindow *win = static_cast<nsGlobalWindow*>(focusedWindow.get());
    focusedWindow = win->GetPrivateParent();
  }
  
  return NS_OK;
}

nsIDOMNode*
nsWindowRoot::GetPopupNode()
{
  return mPopupNode;
}

void
nsWindowRoot::SetPopupNode(nsIDOMNode* aNode)
{
  mPopupNode = aNode;
}

///////////////////////////////////////////////////////////////////////////////////

already_AddRefed<EventTarget>
NS_NewWindowRoot(nsPIDOMWindow* aWindow)
{
  nsCOMPtr<EventTarget> result = new nsWindowRoot(aWindow);
  return result.forget();
}
