/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsContentUtils.h"
#include "nsIDocument.h"
#include "prprf.h"
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
      tmp->mOwnerWindow->GetExtantDoc()->GetDocumentURI(uri);
    }
    PR_snprintf(name, sizeof(name), "DOMEventTargetHelper %s",
                NS_ConvertUTF16toUTF8(uri).get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(DOMEventTargetHelper, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListenerManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListenerManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(DOMEventTargetHelper)
  if (tmp->IsBlack()) {
    if (tmp->mListenerManager) {
      tmp->mListenerManager->MarkForCC();
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
  if (nsPIDOMWindow* owner = GetOwner()) {
    static_cast<nsGlobalWindow*>(owner)->RemoveEventTargetObject(this);
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
  ReleaseWrapper(this);
}

void
DOMEventTargetHelper::BindToOwner(nsPIDOMWindow* aOwner)
{
  MOZ_ASSERT(!aOwner || aOwner->IsInnerWindow());
  nsCOMPtr<nsIGlobalObject> glob = do_QueryInterface(aOwner);
  BindToOwner(glob);
}

void
DOMEventTargetHelper::BindToOwner(nsIGlobalObject* aOwner)
{
  if (mParentObject) {
    if (mOwnerWindow) {
      static_cast<nsGlobalWindow*>(mOwnerWindow)->RemoveEventTargetObject(this);
      mOwnerWindow = nullptr;
    }
    mParentObject = nullptr;
    mHasOrHasHadOwnerWindow = false;
  }
  if (aOwner) {
    mParentObject = aOwner;
    // Let's cache the result of this QI for fast access and off main thread usage
    mOwnerWindow = nsCOMPtr<nsPIDOMWindow>(do_QueryInterface(aOwner)).get();
    if (mOwnerWindow) {
      mHasOrHasHadOwnerWindow = true;
      static_cast<nsGlobalWindow*>(mOwnerWindow)->AddEventTargetObject(this);
    }
  }
}

void
DOMEventTargetHelper::BindToOwner(DOMEventTargetHelper* aOther)
{
  if (mOwnerWindow) {
    static_cast<nsGlobalWindow*>(mOwnerWindow)->RemoveEventTargetObject(this);
    mOwnerWindow = nullptr;
    mParentObject = nullptr;
    mHasOrHasHadOwnerWindow = false;
  }
  if (aOther) {
    mHasOrHasHadOwnerWindow = aOther->HasOrHasHadOwner();
    if (aOther->GetParentObject()) {
      mParentObject = aOther->GetParentObject();
      // Let's cache the result of this QI for fast access and off main thread usage
      mOwnerWindow = nsCOMPtr<nsPIDOMWindow>(do_QueryInterface(mParentObject)).get();
      if (mOwnerWindow) {
        mHasOrHasHadOwnerWindow = true;
        static_cast<nsGlobalWindow*>(mOwnerWindow)->AddEventTargetObject(this);
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
                                       bool aUseCapture,
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
  elm->AddEventListener(aType, aListener, aUseCapture, wantsUntrusted);
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
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  nsresult rv = event->InitEvent(aEventName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

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
  nsRefPtr<EventHandlerNonNull> handler;
  JS::Rooted<JSObject*> callable(aCx);
  if (aValue.isObject() &&
      JS_ObjectIsCallable(aCx, callable = &aValue.toObject())) {
    handler = new EventHandlerNonNull(callable, dom::GetIncumbentGlobal());
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
    *aValue = JS::ObjectValue(*handler->Callable());
  } else {
    *aValue = JS::NullValue();
  }
}

nsresult
DOMEventTargetHelper::PreHandleEvent(EventChainPreVisitor& aVisitor)
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
  nsPIDOMWindow* owner = GetOwner();
  return owner ? static_cast<nsGlobalWindow*>(owner)->GetContextInternal()
               : nullptr;
}

nsresult
DOMEventTargetHelper::WantsUntrusted(bool* aRetVal)
{
  nsresult rv;
  nsIScriptContext* context = GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDocument> doc =
    nsContentUtils::GetDocumentFromScriptContext(context);
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
