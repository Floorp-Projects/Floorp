/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventListenerService.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/JSEventHandler.h"
#include "mozilla/Maybe.h"
#include "nsCOMArray.h"
#include "nsDOMClassInfoID.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsMemory.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::EventListenerInfo
 ******************************************************************************/

NS_IMPL_CYCLE_COLLECTION(EventListenerInfo, mListener)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EventListenerInfo)
  NS_INTERFACE_MAP_ENTRY(nsIEventListenerInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(EventListenerInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EventListenerInfo)

NS_IMETHODIMP
EventListenerInfo::GetType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerInfo::GetCapturing(bool* aCapturing)
{
  *aCapturing = mCapturing;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerInfo::GetAllowsUntrusted(bool* aAllowsUntrusted)
{
  *aAllowsUntrusted = mAllowsUntrusted;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerInfo::GetInSystemEventGroup(bool* aInSystemEventGroup)
{
  *aInSystemEventGroup = mInSystemEventGroup;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerInfo::GetListenerObject(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aObject)
{
  Maybe<JSAutoCompartment> ac;
  GetJSVal(aCx, ac, aObject);
  return NS_OK;
}

/******************************************************************************
 * mozilla::EventListenerService
 ******************************************************************************/

NS_IMPL_ISUPPORTS(EventListenerService, nsIEventListenerService)

bool
EventListenerInfo::GetJSVal(JSContext* aCx,
                            Maybe<JSAutoCompartment>& aAc,
                            JS::MutableHandle<JS::Value> aJSVal)
{
  aJSVal.setNull();
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(mListener);
  if (wrappedJS) {
    JS::Rooted<JSObject*> object(aCx, wrappedJS->GetJSObject());
    if (!object) {
      return false;
    }
    aAc.emplace(aCx, object);
    aJSVal.setObject(*object);
    return true;
  }

  nsCOMPtr<JSEventHandler> jsHandler = do_QueryInterface(mListener);
  if (jsHandler && jsHandler->GetTypedEventHandler().HasEventHandler()) {
    JS::Handle<JSObject*> handler =
      jsHandler->GetTypedEventHandler().Ptr()->Callable();
    if (handler) {
      aAc.emplace(aCx, handler);
      aJSVal.setObject(*handler);
      return true;
    }
  }
  return false;
}

NS_IMETHODIMP
EventListenerInfo::ToSource(nsAString& aResult)
{
  aResult.SetIsVoid(true);

  AutoSafeJSContext cx;
  Maybe<JSAutoCompartment> ac;
  JS::Rooted<JS::Value> v(cx);
  if (GetJSVal(cx, ac, &v)) {
    JSString* str = JS_ValueToSource(cx, v);
    if (str) {
      nsAutoJSString autoStr;
      if (autoStr.init(cx, str)) {
        aResult.Assign(autoStr);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::GetListenerInfoFor(nsIDOMEventTarget* aEventTarget,
                                         uint32_t* aCount,
                                         nsIEventListenerInfo*** aOutArray)
{
  NS_ENSURE_ARG_POINTER(aEventTarget);
  *aCount = 0;
  *aOutArray = nullptr;
  nsCOMArray<nsIEventListenerInfo> listenerInfos;

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aEventTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  EventListenerManager* elm = eventTarget->GetExistingListenerManager();
  if (elm) {
    elm->GetListenerInfo(&listenerInfos);
  }

  int32_t count = listenerInfos.Count();
  if (count == 0) {
    return NS_OK;
  }

  *aOutArray =
    static_cast<nsIEventListenerInfo**>(
      nsMemory::Alloc(sizeof(nsIEventListenerInfo*) * count));
  NS_ENSURE_TRUE(*aOutArray, NS_ERROR_OUT_OF_MEMORY);

  for (int32_t i = 0; i < count; ++i) {
    NS_ADDREF((*aOutArray)[i] = listenerInfos[i]);
  }
  *aCount = count;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::GetEventTargetChainFor(nsIDOMEventTarget* aEventTarget,
                                             uint32_t* aCount,
                                             nsIDOMEventTarget*** aOutArray)
{
  *aCount = 0;
  *aOutArray = nullptr;
  NS_ENSURE_ARG(aEventTarget);
  WidgetEvent event(true, NS_EVENT_NULL);
  nsTArray<EventTarget*> targets;
  nsresult rv = EventDispatcher::Dispatch(aEventTarget, nullptr, &event,
                                          nullptr, nullptr, nullptr, &targets);
  NS_ENSURE_SUCCESS(rv, rv);
  int32_t count = targets.Length();
  if (count == 0) {
    return NS_OK;
  }

  *aOutArray =
    static_cast<nsIDOMEventTarget**>(
      nsMemory::Alloc(sizeof(nsIDOMEventTarget*) * count));
  NS_ENSURE_TRUE(*aOutArray, NS_ERROR_OUT_OF_MEMORY);

  for (int32_t i = 0; i < count; ++i) {
    NS_ADDREF((*aOutArray)[i] = targets[i]);
  }
  *aCount = count;

  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::HasListenersFor(nsIDOMEventTarget* aEventTarget,
                                      const nsAString& aType,
                                      bool* aRetVal)
{
  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aEventTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  EventListenerManager* elm = eventTarget->GetExistingListenerManager();
  *aRetVal = elm && elm->HasListenersFor(aType);
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::AddSystemEventListener(nsIDOMEventTarget *aTarget,
                                             const nsAString& aType,
                                             nsIDOMEventListener* aListener,
                                             bool aUseCapture)
{
  NS_PRECONDITION(aTarget, "Missing target");
  NS_PRECONDITION(aListener, "Missing listener");

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  EventListenerManager* manager = eventTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(manager);

  EventListenerFlags flags =
    aUseCapture ? TrustedEventsAtSystemGroupCapture() :
                  TrustedEventsAtSystemGroupBubble();
  manager->AddEventListenerByType(aListener, aType, flags);
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::RemoveSystemEventListener(nsIDOMEventTarget *aTarget,
                                                const nsAString& aType,
                                                nsIDOMEventListener* aListener,
                                                bool aUseCapture)
{
  NS_PRECONDITION(aTarget, "Missing target");
  NS_PRECONDITION(aListener, "Missing listener");

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  EventListenerManager* manager = eventTarget->GetExistingListenerManager();
  if (manager) {
    EventListenerFlags flags =
      aUseCapture ? TrustedEventsAtSystemGroupCapture() :
                    TrustedEventsAtSystemGroupBubble();
    manager->RemoveEventListenerByType(aListener, aType, flags);
  }

  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::AddListenerForAllEvents(nsIDOMEventTarget* aTarget,
                                              nsIDOMEventListener* aListener,
                                              bool aUseCapture,
                                              bool aWantsUntrusted,
                                              bool aSystemEventGroup)
{
  NS_ENSURE_STATE(aTarget && aListener);

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  EventListenerManager* manager = eventTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(manager);
  manager->AddListenerForAllEvents(aListener, aUseCapture, aWantsUntrusted,
                               aSystemEventGroup);
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::RemoveListenerForAllEvents(nsIDOMEventTarget* aTarget,
                                                 nsIDOMEventListener* aListener,
                                                 bool aUseCapture,
                                                 bool aSystemEventGroup)
{
  NS_ENSURE_STATE(aTarget && aListener);

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(aTarget);
  NS_ENSURE_TRUE(eventTarget, NS_ERROR_NO_INTERFACE);

  EventListenerManager* manager = eventTarget->GetExistingListenerManager();
  if (manager) {
    manager->RemoveListenerForAllEvents(aListener, aUseCapture, aSystemEventGroup);
  }
  return NS_OK;
}

} // namespace mozilla

nsresult
NS_NewEventListenerService(nsIEventListenerService** aResult)
{
  *aResult = new mozilla::EventListenerService();
  NS_ADDREF(*aResult);
  return NS_OK;
}
