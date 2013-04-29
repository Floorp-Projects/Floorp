/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMEventTargetHelper.h"
#include "nsContentUtils.h"
#include "nsEventDispatcher.h"
#include "nsGUIEvent.h"
#include "nsIDocument.h"
#include "nsDOMJSUtils.h"
#include "prprf.h"
#include "nsGlobalWindow.h"
#include "nsDOMEvent.h"
#include "mozilla/Likely.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INTERNAL(nsDOMEventTargetHelper)
  if (MOZ_UNLIKELY(cb.WantDebugInfo())) {
    char name[512];
    nsAutoString uri;
    if (tmp->mOwnerWindow && tmp->mOwnerWindow->GetExtantDoc()) {
      tmp->mOwnerWindow->GetExtantDoc()->GetDocumentURI(uri);
    }
    PR_snprintf(name, sizeof(name), "nsDOMEventTargetHelper %s",
                NS_ConvertUTF16toUTF8(uri).get());
    cb.DescribeRefCountedNode(tmp->mRefCnt.get(), name);
  } else {
    NS_IMPL_CYCLE_COLLECTION_DESCRIBE(nsDOMEventTargetHelper, tmp->mRefCnt.get())
  }

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mListenerManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mListenerManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_BEGIN(nsDOMEventTargetHelper)
  if (tmp->IsBlack()) {
    if (tmp->mListenerManager) {
      tmp->mListenerManager->MarkForCC();
    }
    return true;
  }
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_BEGIN(nsDOMEventTargetHelper)
  return tmp->IsBlackAndDoesNotNeedTracing(tmp);
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_IN_CC_END

NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_BEGIN(nsDOMEventTargetHelper)
  return tmp->IsBlack();
NS_IMPL_CYCLE_COLLECTION_CAN_SKIP_THIS_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsDOMEventTargetHelper)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventTarget)
  NS_INTERFACE_MAP_ENTRY(mozilla::dom::EventTarget)
  NS_INTERFACE_MAP_ENTRY(nsDOMEventTargetHelper)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsDOMEventTargetHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsDOMEventTargetHelper)

NS_IMPL_DOMTARGET_DEFAULTS(nsDOMEventTargetHelper)

nsDOMEventTargetHelper::~nsDOMEventTargetHelper()
{
  if (nsPIDOMWindow* owner = GetOwner()) {
    static_cast<nsGlobalWindow*>(owner)->RemoveEventTargetObject(this);
  }
  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
  nsContentUtils::ReleaseWrapper(this, this);
}

void
nsDOMEventTargetHelper::BindToOwner(nsPIDOMWindow* aOwner)
{
  nsCOMPtr<nsIGlobalObject> glob = do_QueryInterface(aOwner);
  BindToOwner(glob);
}

void
nsDOMEventTargetHelper::BindToOwner(nsIGlobalObject* aOwner)
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
nsDOMEventTargetHelper::BindToOwner(nsDOMEventTargetHelper* aOther)
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
nsDOMEventTargetHelper::DisconnectFromOwner()
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
nsDOMEventTargetHelper::RemoveEventListener(const nsAString& aType,
                                            nsIDOMEventListener* aListener,
                                            bool aUseCapture)
{
  nsEventListenerManager* elm = GetListenerManager(false);
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }

  return NS_OK;
}

NS_IMPL_REMOVE_SYSTEM_EVENT_LISTENER(nsDOMEventTargetHelper)

NS_IMETHODIMP
nsDOMEventTargetHelper::AddEventListener(const nsAString& aType,
                                         nsIDOMEventListener *aListener,
                                         bool aUseCapture,
                                         bool aWantsUntrusted,
                                         uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making aOptionalArgc non-zero.");

  if (aOptionalArgc < 2) {
    nsresult rv;
    nsIScriptContext* context = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(context);
    aWantsUntrusted = doc && !nsContentUtils::IsChromeDoc(doc);
  }

  nsEventListenerManager* elm = GetListenerManager(true);
  NS_ENSURE_STATE(elm);
  elm->AddEventListener(aType, aListener, aUseCapture, aWantsUntrusted);
  return NS_OK;
}

void
nsDOMEventTargetHelper::AddEventListener(const nsAString& aType,
                                         nsIDOMEventListener* aListener,
                                         bool aUseCapture,
                                         const Nullable<bool>& aWantsUntrusted,
                                         ErrorResult& aRv)
{
  bool wantsUntrusted;
  if (aWantsUntrusted.IsNull()) {
    nsresult rv;
    nsIScriptContext* context = GetContextForEventHandlers(&rv);
    if (NS_FAILED(rv)) {
      aRv.Throw(rv);
      return;
    }
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(context);
    wantsUntrusted = doc && !nsContentUtils::IsChromeDoc(doc);
  } else {
    wantsUntrusted = aWantsUntrusted.Value();
  }

  nsEventListenerManager* elm = GetListenerManager(true);
  if (!elm) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }
  elm->AddEventListener(aType, aListener, aUseCapture, wantsUntrusted);
}

NS_IMETHODIMP
nsDOMEventTargetHelper::AddSystemEventListener(const nsAString& aType,
                                               nsIDOMEventListener *aListener,
                                               bool aUseCapture,
                                               bool aWantsUntrusted,
                                               uint8_t aOptionalArgc)
{
  NS_ASSERTION(!aWantsUntrusted || aOptionalArgc > 1,
               "Won't check if this is chrome, you want to set "
               "aWantsUntrusted to false or make the aWantsUntrusted "
               "explicit by making aOptionalArgc non-zero.");

  if (aOptionalArgc < 2) {
    nsresult rv;
    nsIScriptContext* context = GetContextForEventHandlers(&rv);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> doc =
      nsContentUtils::GetDocumentFromScriptContext(context);
    aWantsUntrusted = doc && !nsContentUtils::IsChromeDoc(doc);
  }

  return NS_AddSystemEventListener(this, aType, aListener, aUseCapture,
                                   aWantsUntrusted);
}

NS_IMETHODIMP
nsDOMEventTargetHelper::DispatchEvent(nsIDOMEvent* aEvent, bool* aRetVal)
{
  nsEventStatus status = nsEventStatus_eIgnore;
  nsresult rv =
    nsEventDispatcher::DispatchDOMEvent(this, nullptr, aEvent, nullptr, &status);

  *aRetVal = (status != nsEventStatus_eConsumeNoDefault);
  return rv;
}

nsresult
nsDOMEventTargetHelper::DispatchTrustedEvent(const nsAString& aEventName)
{
  nsCOMPtr<nsIDOMEvent> event;
  NS_NewDOMEvent(getter_AddRefs(event), this, nullptr, nullptr);
  nsresult rv = event->InitEvent(aEventName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return DispatchTrustedEvent(event);
}

nsresult
nsDOMEventTargetHelper::DispatchTrustedEvent(nsIDOMEvent* event)
{
  event->SetTrusted(true);

  bool dummy;
  return DispatchEvent(event, &dummy);
}

nsresult
nsDOMEventTargetHelper::SetEventHandler(nsIAtom* aType,
                                        JSContext* aCx,
                                        const JS::Value& aValue)
{
  nsRefPtr<EventHandlerNonNull> handler;
  JSObject* callable;
  if (aValue.isObject() &&
      JS_ObjectIsCallable(aCx, callable = &aValue.toObject())) {
    handler = new EventHandlerNonNull(callable);
  }
  ErrorResult rv;
  SetEventHandler(aType, handler, rv);
  return rv.ErrorCode();
}

void
nsDOMEventTargetHelper::GetEventHandler(nsIAtom* aType,
                                        JSContext* aCx,
                                        JS::Value* aValue)
{
  EventHandlerNonNull* handler = GetEventHandler(aType);
  if (handler) {
    *aValue = JS::ObjectValue(*handler->Callable());
  } else {
    *aValue = JS::NullValue();
  }
}

nsresult
nsDOMEventTargetHelper::PreHandleEvent(nsEventChainPreVisitor& aVisitor)
{
  aVisitor.mCanHandle = true;
  aVisitor.mParentTarget = nullptr;
  return NS_OK;
}

nsresult
nsDOMEventTargetHelper::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return NS_OK;
}

nsresult
nsDOMEventTargetHelper::DispatchDOMEvent(nsEvent* aEvent,
                                         nsIDOMEvent* aDOMEvent,
                                         nsPresContext* aPresContext,
                                         nsEventStatus* aEventStatus)
{
  return
    nsEventDispatcher::DispatchDOMEvent(this, aEvent, aDOMEvent, aPresContext,
                                        aEventStatus);
}

nsEventListenerManager*
nsDOMEventTargetHelper::GetListenerManager(bool aCreateIfNotFound)
{
  if (!mListenerManager && aCreateIfNotFound) {
    mListenerManager = new nsEventListenerManager(this);
  }

  return mListenerManager;
}

nsIScriptContext*
nsDOMEventTargetHelper::GetContextForEventHandlers(nsresult* aRv)
{
  *aRv = CheckInnerWindowCorrectness();
  if (NS_FAILED(*aRv)) {
    return nullptr;
  }
  nsPIDOMWindow* owner = GetOwner();
  return owner ? static_cast<nsGlobalWindow*>(owner)->GetContextInternal()
               : nullptr;
}

