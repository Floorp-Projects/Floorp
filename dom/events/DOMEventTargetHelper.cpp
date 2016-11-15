/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "mozilla/Sprintf.h"
#include "nsGlobalWindow.h"
#include "ScriptSettings.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/Likely.h"

namespace mozilla {

using namespace dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(DOMEventTargetHelper)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    nsAutoString uri;
    if (tmp->mOwnerWindow && tmp->mOwnerWindow->GetExtantDoc()) {
      Unused << tmp->mOwnerWindow->GetExtantDoc()->GetDocumentURI(uri);
    }

    nsXPCOMCycleCollectionParticipant* participant = nullptr;
    CallQueryInterface(tmp, &participant);

    SprintfLiteral(name, "%s %s",
                   participant->ClassName(),
                   NS_ConvertUTF16toUTF8(uri).get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(DOMEventTargetHelper, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListenerManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListenerManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(DOMEventTargetHelper)
  if (tmp->IsBlack() || tmp->IsCertainlyAliveForCC()) {
    if (tmp->mListenerManager) {
      tmp->mListenerManager->MarkForCC();
    }
    if (!tmp->IsBlack() && tmp->PreservingWrapper()) {
      // This marks the wrapper black.
      tmp->GetWrapper();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(DOMEventTargetHelper)
  return tmp->IsBlackAndDoesNotNeedTracing(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(DOMEventTargetHelper)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMEventTargetHelper)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY(DOMEventTargetHelper)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(DOMEventTargetHelper,
                                                   LastRelease())

NS_IMPL_DOMTARGET_DEFAULTS(DOMEventTargetHelper)

DOMEventTargetHelper::~DOMEventTargetHelper()
{
  if (nsPIDOMWindowInner* owner = GetOwner()) {
    nsGlobalWindow::Cast(owner)->RemoveEventTargetObject(this);
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
  ReleaseWrapper(this);
}

void
DOMEventTargetHelper::BindToOwner(nsPIDOMWindowInner* aOwner)
{
  nsCOMPtr<nsIGlobalObject> glob = do_QueryInterface(aOwner);
  BindToOwner(glob);
}

void
DOMEventTargetHelper::BindToOwner(nsIGlobalObject* aOwner)
{
  nsCOMPtr<nsIGlobalObject> parentObject = do_QueryReferent(mParentObject);
  if (parentObject) {
    if (mOwnerWindow) {
      nsGlobalWindow::Cast(mOwnerWindow)->RemoveEventTargetObject(this);
      mOwnerWindow = nullptr;
    }
    mParentObject = nullptr;
    mHasOrHasHadOwnerWindow = false;
  }
  if (aOwner) {
    mParentObject = do_GetWeakReference(aOwner);
    MOZ_ASSERT(mParentObject, "All nsIGlobalObjects must support nsISupportsWeakReference");
    // Let's cache the result of this QI for fast access and off main thread usage
    mOwnerWindow = nsCOMPtr<nsPIDOMWindowInner>(do_QueryInterface(aOwner)).get();
    if (mOwnerWindow) {
      mHasOrHasHadOwnerWindow = true;
      nsGlobalWindow::Cast(mOwnerWindow)->AddEventTargetObject(this);
    }
  }
}

void
DOMEventTargetHelper::BindToOwner(DOMEventTargetHelper* aOther)
{
  if (mOwnerWindow) {
    nsGlobalWindow::Cast(mOwnerWindow)->RemoveEventTargetObject(this);
    mOwnerWindow = nullptr;
    mParentObject = nullptr;
    mHasOrHasHadOwnerWindow = false;
  }
  if (aOther) {
    mHasOrHasHadOwnerWindow = aOther->HasOrHasHadOwner();
    if (aOther->GetParentObject()) {
      mParentObject = do_GetWeakReference(aOther->GetParentObject());
      MOZ_ASSERT(mParentObject, "All nsIGlobalObjects must support nsISupportsWeakReference");
      // Let's cache the result of this QI for fast access and off main thread usage
      mOwnerWindow = nsCOMPtr<nsPIDOMWindowInner>(do_QueryInterface(aOther->GetParentObject())).get();
      if (mOwnerWindow) {
        mHasOrHasHadOwnerWindow = true;
        nsGlobalWindow::Cast(mOwnerWindow)->AddEventTargetObject(this);
      }
    }
  }
}

void
DOMEventTargetHelper::DisconnectFromOwner()
{
  mOwnerWindow = nullptr;
  mParentObject = nullptr;
  // Event listeners can't be handled anymore, so we can release them here.
  if (mListenerManager) {
    mListenerManager->Disconnect();
    mListenerManager = nullptr;
  }
}

nsPIDOMWindowInner*
DOMEventTargetHelper::GetWindowIfCurrent() const
{
  if (NS_FAILED(CheckInnerWindowCorrectness())) {
    return nullptr;
  }

  return GetOwner();
}

nsIDocument*
DOMEventTargetHelper::GetDocumentIfCurrent() const
{
  nsPIDOMWindowInner* win = GetWindowIfCurrent();
  if (!win) {
    return nullptr;
  }

  return win->GetDoc();
}

NS_IMETHODIMP
DOMEventTargetHelper::RemoveEventListener(const nsAString& aType,
                                          nsIDOMEventListener* aListener,
                                          bool aUseCapture)
{
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }

  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(DOMEventTargetHelper)

NS_IMETHODIMP
DOMEventTargetHelper::AddEventListener(const nsAString& aType,
                                       nsIDOMEventListener* aListener,
                                       bool aUseCapture,
                                       bool aWantsUntrusted,
                                       uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making aOptionalArgc non-zero.");

  if (aOptionalArgc < 2) {
    nsresult rv = WantsUntrusted(&aWantsUntrusted);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  EventListenerManager* elm = GetOrCreateListenerManager();
  NS_ENSURE_STATE(elm);
  elm->AddEventListener(aType, aListener, aUseCapture, aWantsUntrusted);
  return NS_OK;
}

void
DOMEventTargetHelper::AddEventListener(const nsAString& aType,
                                       EventListener* aListener,
                                       const AddEventListenerOptionsOrBoolean& aOptions,
                                       const Nullable<bool>& aWantsUntrusted,
                                       ErrorResult& aRv)
{
  bool wantsUntrusted;
  if (aWantsUntrusted.IsNull()) {
    nsresult rv = WantsUntrusted(&wantsUntrusted);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
  } else {
    wantsUntrusted = aWantsUntrusted.Value();
  }

  EventListenerManager* elm = GetOrCreateListenerManager();
  if (!elm) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  elm->AddEventListener(aType, aListener, aOptions, wantsUntrusted);
}

NS_IMETHODIMP
DOMEventTargetHelper::AddSystemEventListener(const nsAString& aType,
                                             nsIDOMEventListener* aListener,
                                             bool aUseCapture,
                                             bool aWantsUntrusted,
                                             uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making aOptionalArgc non-zero.");

  if (aOptionalArgc < 2) {
    nsresult rv = WantsUntrusted(&aWantsUntrusted);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_AddSystemEventListener(this, aType, aListener, aUseCapture,
                                   aWantsUntrusted);
}

NS_IMETHODIMP
DOMEventTargetHelper::DispatchEvent(nsIDOMEvent* aEvent, bool* aRetVal)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    EventDispatcher::DispatchDOMEvent(this, nullptr, aEvent, nullptr, &status);

  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

nsresult
DOMEventTargetHelper::DispatchTrustedEvent(const nsAString& aEventName)
{
  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(aEventName, false, false);

  return DispatchTrustedEvent(event);
}

nsresult
DOMEventTargetHelper::DispatchTrustedEvent(nsIDOMEvent* event)
{
  event->SetTrusted(true);

  bool dummy;
  return DispatchEvent(event, &dummy);
}

nsresult
DOMEventTargetHelper::SetEventHandler(nsIAtom* aType,
                                      JSContext* aCx,
                                      const JS::Value& aValue)
{
  RefPtr<EventHandlerNonNull> handler;
  JS::Rooted<JSObject*> callable(aCx);
  if (aValue.isObject() && JS::IsCallable(callable = &aValue.toObject())) {
    handler = new EventHandlerNonNull(aCx, callable, dom::GetIncumbentGlobal());
  }
  SetEventHandler(aType, EmptyString(), handler);
  return NS_OK;
}

void
DOMEventTargetHelper::GetEventHandler(nsIAtom* aType,
                                      JSContext* aCx,
                                      JS::Value* aValue)
{
  EventHandlerNonNull* handler = GetEventHandler(aType, EmptyString());
  if (handler) {
    *aValue = JS::ObjectOrNullValue(handler->CallableOrNull());
  } else {
    *aValue = JS::NullValue();
  }
}

nsresult
DOMEventTargetHelper::GetEventTargetParent(EventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = nullptr;
  return NS_OK;
}

nsresult
DOMEventTargetHelper::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

nsresult
DOMEventTargetHelper::DispatchDOMEvent(WidgetEvent* aEvent,
                                       nsIDOMEvent* aDOMEvent,
                                       nsPresContext* aPresContext,
                                       nsEventStatus* aEventStatus)
{
  return EventDispatcher::DispatchDOMEvent(this, aEvent, aDOMEvent,
                                           aPresContext, aEventStatus);
}

EventListenerManager*
DOMEventTargetHelper::GetOrCreateListenerManager()
{
  if (!mListenerManager) {
    mListenerManager = new EventListenerManager(this);
  }

  return mListenerManager;
}

EventListenerManager*
DOMEventTargetHelper::GetExistingListenerManager() const
{
  return mListenerManager;
}

nsIScriptContext*
DOMEventTargetHelper::GetContextForEventHandlers(nsresult* aRv)
{
  *aRv = CheckInnerWindowCorrectness();
  if (NS_FAILED(*aRv)) {
    return nullptr;
  }
  nsPIDOMWindowInner* owner = GetOwner();
  return owner ? nsGlobalWindow::Cast(owner)->GetContextInternal()
               : nullptr;
}

nsresult
DOMEventTargetHelper::WantsUntrusted(bool* aRetVal)
{
  nsresult rv = CheckInnerWindowCorrectness();
  NS_ENSURE_SUCCESS(rv, rv);
  
  nsCOMPtr<nsIDocument> doc = GetDocumentIfCurrent();
  // We can let listeners on workers to always handle all the events.
  *aRetVal = (doc && !nsContentUtils::IsChromeDoc(doc)) || !NS_IsMainThread();
  return rv;
}

void
DOMEventTargetHelper::EventListenerAdded(nsIAtom* aType)
{
  ErrorResult rv;
  EventListenerWasAdded(Substring(nsDependentAtomString(aType), 2), rv);
}

void
DOMEventTargetHelper::EventListenerRemoved(nsIAtom* aType)
{
  ErrorResult rv;
  EventListenerWasRemoved(Substring(nsDependentAtomString(aType), 2), rv);
}

} // namespace mozilla
