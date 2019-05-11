/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventListenerService.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/EventListenerManager.h"
#include "mozilla/JSEventHandler.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/EventListenerBinding.h"
#include "mozilla/dom/ScriptSettings.h"
#include "nsArrayUtils.h"
#include "nsCOMArray.h"
#include "nsIXPConnect.h"
#include "nsJSUtils.h"
#include "nsMemory.h"
#include "nsServiceManagerUtils.h"
#include "nsArray.h"
#include "nsThreadUtils.h"

namespace mozilla {

using namespace dom;

/******************************************************************************
 * mozilla::EventListenerChange
 ******************************************************************************/

NS_IMPL_ISUPPORTS(EventListenerChange, nsIEventListenerChange)

EventListenerChange::~EventListenerChange() {}

EventListenerChange::EventListenerChange(EventTarget* aTarget)
    : mTarget(aTarget) {}

void EventListenerChange::AddChangedListenerName(nsAtom* aEventName) {
  mChangedListenerNames.AppendElement(aEventName);
}

NS_IMETHODIMP
EventListenerChange::GetTarget(EventTarget** aTarget) {
  NS_ENSURE_ARG_POINTER(aTarget);
  NS_ADDREF(*aTarget = mTarget);
  return NS_OK;
}

NS_IMETHODIMP
EventListenerChange::GetCountOfEventListenerChangesAffectingAccessibility(
    uint32_t* aCount) {
  *aCount = 0;

  size_t length = mChangedListenerNames.Length();
  for (size_t i = 0; i < length; i++) {
    RefPtr<nsAtom> listenerName = mChangedListenerNames[i];

    // These are the event listener changes which may make an element
    // accessible or inaccessible.
    if (listenerName == nsGkAtoms::onclick ||
        listenerName == nsGkAtoms::onmousedown ||
        listenerName == nsGkAtoms::onmouseup) {
      *aCount += 1;
    }
  }

  return NS_OK;
}

/******************************************************************************
 * mozilla::EventListenerInfo
 ******************************************************************************/

EventListenerInfo::EventListenerInfo(
    const nsAString& aType, JS::Handle<JSObject*> aScriptedListener,
    JS::Handle<JSObject*> aScriptedListenerGlobal, bool aCapturing,
    bool aAllowsUntrusted, bool aInSystemEventGroup)
    : mType(aType),
      mScriptedListener(aScriptedListener),
      mScriptedListenerGlobal(aScriptedListenerGlobal),
      mCapturing(aCapturing),
      mAllowsUntrusted(aAllowsUntrusted),
      mInSystemEventGroup(aInSystemEventGroup) {
  if (aScriptedListener) {
    MOZ_ASSERT(JS_IsGlobalObject(aScriptedListenerGlobal));
    js::AssertSameCompartment(aScriptedListener, aScriptedListenerGlobal);
  }

  HoldJSObjects(this);
}

EventListenerInfo::~EventListenerInfo() { DropJSObjects(this); }

NS_IMPL_CYCLE_COLLECTION_CLASS(EventListenerInfo)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(EventListenerInfo)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(EventListenerInfo)
  tmp->mScriptedListener = nullptr;
  tmp->mScriptedListenerGlobal = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(EventListenerInfo)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScriptedListener)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mScriptedListenerGlobal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(EventListenerInfo)
  NS_INTERFACE_MAP_ENTRY(nsIEventListenerInfo)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(EventListenerInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(EventListenerInfo)

NS_IMETHODIMP
EventListenerInfo::GetType(nsAString& aType) {
  aType = mType;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerInfo::GetCapturing(bool* aCapturing) {
  *aCapturing = mCapturing;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerInfo::GetAllowsUntrusted(bool* aAllowsUntrusted) {
  *aAllowsUntrusted = mAllowsUntrusted;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerInfo::GetInSystemEventGroup(bool* aInSystemEventGroup) {
  *aInSystemEventGroup = mInSystemEventGroup;
  return NS_OK;
}

NS_IMETHODIMP
EventListenerInfo::GetListenerObject(JSContext* aCx,
                                     JS::MutableHandle<JS::Value> aObject) {
  Maybe<JSAutoRealm> ar;
  GetJSVal(aCx, ar, aObject);
  return NS_OK;
}

/******************************************************************************
 * mozilla::EventListenerService
 ******************************************************************************/

NS_IMPL_ISUPPORTS(EventListenerService, nsIEventListenerService)

bool EventListenerInfo::GetJSVal(JSContext* aCx, Maybe<JSAutoRealm>& aAr,
                                 JS::MutableHandle<JS::Value> aJSVal) {
  if (mScriptedListener) {
    aJSVal.setObject(*mScriptedListener);
    aAr.emplace(aCx, mScriptedListenerGlobal);
    return true;
  }

  aJSVal.setNull();
  return false;
}

NS_IMETHODIMP
EventListenerInfo::ToSource(nsAString& aResult) {
  aResult.SetIsVoid(true);

  AutoSafeJSContext cx;
  Maybe<JSAutoRealm> ar;
  JS::Rooted<JS::Value> v(cx);
  if (GetJSVal(cx, ar, &v)) {
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

EventListenerService* EventListenerService::sInstance = nullptr;

EventListenerService::EventListenerService() {
  MOZ_ASSERT(!sInstance);
  sInstance = this;
}

EventListenerService::~EventListenerService() {
  MOZ_ASSERT(sInstance == this);
  sInstance = nullptr;
}

NS_IMETHODIMP
EventListenerService::GetListenerInfoFor(
    EventTarget* aEventTarget,
    nsTArray<RefPtr<nsIEventListenerInfo>>& aOutArray) {
  NS_ENSURE_ARG_POINTER(aEventTarget);

  EventListenerManager* elm = aEventTarget->GetExistingListenerManager();
  if (elm) {
    elm->GetListenerInfo(aOutArray);
  }

  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::GetEventTargetChainFor(
    EventTarget* aEventTarget, bool aComposed,
    nsTArray<RefPtr<EventTarget>>& aOutArray) {
  NS_ENSURE_ARG(aEventTarget);
  WidgetEvent event(true, eVoidEvent);
  event.SetComposed(aComposed);
  nsTArray<EventTarget*> targets;
  nsresult rv = EventDispatcher::Dispatch(aEventTarget, nullptr, &event,
                                          nullptr, nullptr, nullptr, &targets);
  NS_ENSURE_SUCCESS(rv, rv);
  aOutArray.AppendElements(targets);
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::HasListenersFor(EventTarget* aEventTarget,
                                      const nsAString& aType, bool* aRetVal) {
  NS_ENSURE_TRUE(aEventTarget, NS_ERROR_UNEXPECTED);

  EventListenerManager* elm = aEventTarget->GetExistingListenerManager();
  *aRetVal = elm && elm->HasListenersFor(aType);
  return NS_OK;
}

static already_AddRefed<EventListener> ToEventListener(
    JSContext* aCx, JS::Handle<JS::Value> aValue) {
  if (NS_WARN_IF(!aValue.isObject())) {
    return nullptr;
  }

  JS::Rooted<JSObject*> obj(aCx, &aValue.toObject());
  JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
  RefPtr<EventListener> listener =
      new EventListener(aCx, obj, global, GetIncumbentGlobal());
  return listener.forget();
}

NS_IMETHODIMP
EventListenerService::AddSystemEventListener(EventTarget* aTarget,
                                             const nsAString& aType,
                                             JS::Handle<JS::Value> aListener,
                                             bool aUseCapture, JSContext* aCx) {
  MOZ_ASSERT(aTarget, "Missing target");

  NS_ENSURE_TRUE(aTarget, NS_ERROR_UNEXPECTED);

  RefPtr<EventListener> listener = ToEventListener(aCx, aListener);
  if (!listener) {
    return NS_ERROR_UNEXPECTED;
  }

  EventListenerManager* manager = aTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(manager);

  EventListenerFlags flags = aUseCapture ? TrustedEventsAtSystemGroupCapture()
                                         : TrustedEventsAtSystemGroupBubble();
  manager->AddEventListenerByType(listener, aType, flags);
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::RemoveSystemEventListener(EventTarget* aTarget,
                                                const nsAString& aType,
                                                JS::Handle<JS::Value> aListener,
                                                bool aUseCapture,
                                                JSContext* aCx) {
  MOZ_ASSERT(aTarget, "Missing target");

  NS_ENSURE_TRUE(aTarget, NS_ERROR_UNEXPECTED);

  RefPtr<EventListener> listener = ToEventListener(aCx, aListener);
  if (!listener) {
    return NS_ERROR_UNEXPECTED;
  }

  EventListenerManager* manager = aTarget->GetExistingListenerManager();
  if (manager) {
    EventListenerFlags flags = aUseCapture ? TrustedEventsAtSystemGroupCapture()
                                           : TrustedEventsAtSystemGroupBubble();
    manager->RemoveEventListenerByType(listener, aType, flags);
  }

  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::AddListenerForAllEvents(
    EventTarget* aTarget, JS::Handle<JS::Value> aListener, bool aUseCapture,
    bool aWantsUntrusted, bool aSystemEventGroup, JSContext* aCx) {
  NS_ENSURE_STATE(aTarget);

  RefPtr<EventListener> listener = ToEventListener(aCx, aListener);
  if (!listener) {
    return NS_ERROR_UNEXPECTED;
  }

  EventListenerManager* manager = aTarget->GetOrCreateListenerManager();
  NS_ENSURE_STATE(manager);
  manager->AddListenerForAllEvents(listener, aUseCapture, aWantsUntrusted,
                                   aSystemEventGroup);
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::RemoveListenerForAllEvents(
    EventTarget* aTarget, JS::Handle<JS::Value> aListener, bool aUseCapture,
    bool aSystemEventGroup, JSContext* aCx) {
  NS_ENSURE_STATE(aTarget);

  RefPtr<EventListener> listener = ToEventListener(aCx, aListener);
  if (!listener) {
    return NS_ERROR_UNEXPECTED;
  }

  EventListenerManager* manager = aTarget->GetExistingListenerManager();
  if (manager) {
    manager->RemoveListenerForAllEvents(listener, aUseCapture,
                                        aSystemEventGroup);
  }
  return NS_OK;
}

NS_IMETHODIMP
EventListenerService::AddListenerChangeListener(
    nsIListenerChangeListener* aListener) {
  if (!mChangeListeners.Contains(aListener)) {
    mChangeListeners.AppendElement(aListener);
  }
  return NS_OK;
};

NS_IMETHODIMP
EventListenerService::RemoveListenerChangeListener(
    nsIListenerChangeListener* aListener) {
  mChangeListeners.RemoveElement(aListener);
  return NS_OK;
};

void EventListenerService::NotifyAboutMainThreadListenerChangeInternal(
    dom::EventTarget* aTarget, nsAtom* aName) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTarget);
  if (mChangeListeners.IsEmpty()) {
    return;
  }

  if (!mPendingListenerChanges) {
    mPendingListenerChanges = nsArrayBase::Create();
    nsCOMPtr<nsIRunnable> runnable =
        NewRunnableMethod("EventListenerService::NotifyPendingChanges", this,
                          &EventListenerService::NotifyPendingChanges);
    if (nsCOMPtr<nsIGlobalObject> global = aTarget->GetOwnerGlobal()) {
      global->Dispatch(TaskCategory::Other, runnable.forget());
    } else if (nsCOMPtr<nsINode> node = do_QueryInterface(aTarget)) {
      node->OwnerDoc()->Dispatch(TaskCategory::Other, runnable.forget());
    } else {
      NS_DispatchToCurrentThread(runnable);
    }
  }

  RefPtr<EventListenerChange> changes =
      mPendingListenerChangesSet.LookupForAdd(aTarget).OrInsert(
          [this, aTarget]() {
            EventListenerChange* c = new EventListenerChange(aTarget);
            mPendingListenerChanges->AppendElement(c);
            return c;
          });
  changes->AddChangedListenerName(aName);
}

void EventListenerService::NotifyPendingChanges() {
  nsCOMPtr<nsIMutableArray> changes;
  mPendingListenerChanges.swap(changes);
  mPendingListenerChangesSet.Clear();

  nsTObserverArray<nsCOMPtr<nsIListenerChangeListener>>::EndLimitedIterator
      iter(mChangeListeners);
  while (iter.HasMore()) {
    nsCOMPtr<nsIListenerChangeListener> listener = iter.GetNext();
    listener->ListenersChanged(changes);
  }
}

}  // namespace mozilla

nsresult NS_NewEventListenerService(nsIEventListenerService** aResult) {
  *aResult = new mozilla::EventListenerService();
  NS_ADDREF(*aResult);
  return NS_OK;
}
