/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DebuggerNotificationObserver.h"

#include "DebuggerNotification.h"
#include "nsIGlobalObject.h"
#include "WrapperFactory.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DebuggerNotificationObserver,
                                      mOwnerGlobal, mEventListenerCallbacks)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DebuggerNotificationObserver)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DebuggerNotificationObserver)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DebuggerNotificationObserver)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END

/* static */ already_AddRefed<DebuggerNotificationObserver>
DebuggerNotificationObserver::Constructor(GlobalObject& aGlobal,
                                          ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> globalInterface(
      do_QueryInterface(aGlobal.GetAsSupports()));
  if (NS_WARN_IF(!globalInterface)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DebuggerNotificationObserver> observer(
      new DebuggerNotificationObserver(globalInterface));
  return observer.forget();
}

DebuggerNotificationObserver::DebuggerNotificationObserver(
    nsIGlobalObject* aOwnerGlobal)
    : mOwnerGlobal(aOwnerGlobal) {}

JSObject* DebuggerNotificationObserver::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return DebuggerNotificationObserver_Binding::Wrap(aCx, this, aGivenProto);
}

static already_AddRefed<DebuggerNotificationManager> GetManager(
    JSContext* aCx, JS::Handle<JSObject*> aDebuggeeGlobal) {
  // The debuggee global here is likely a debugger-compartment cross-compartment
  // wrapper for the debuggee global object, so we need to unwrap it to get
  // the real debuggee-compartment global object.
  JS::Rooted<JSObject*> debuggeeGlobalRooted(
      aCx, js::UncheckedUnwrap(aDebuggeeGlobal, false));

  if (!debuggeeGlobalRooted) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> debuggeeGlobalObject(
      xpc::NativeGlobal(debuggeeGlobalRooted));
  if (!debuggeeGlobalObject) {
    return nullptr;
  }

  RefPtr<DebuggerNotificationManager> manager(
      debuggeeGlobalObject->GetOrCreateDebuggerNotificationManager());
  return manager.forget();
}

bool DebuggerNotificationObserver::Connect(
    JSContext* aCx, JS::Handle<JSObject*> aDebuggeeGlobal, ErrorResult& aRv) {
  RefPtr<DebuggerNotificationManager> manager(GetManager(aCx, aDebuggeeGlobal));

  if (!manager) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return manager->Attach(this);
}

bool DebuggerNotificationObserver::Disconnect(
    JSContext* aCx, JS::Handle<JSObject*> aDebuggeeGlobal, ErrorResult& aRv) {
  RefPtr<DebuggerNotificationManager> manager(GetManager(aCx, aDebuggeeGlobal));

  if (!manager) {
    aRv.Throw(NS_ERROR_FAILURE);
    return false;
  }

  return manager->Detach(this);
}

bool DebuggerNotificationObserver::AddListener(
    DebuggerNotificationCallback& aHandlerFn) {
  const auto [begin, end] = mEventListenerCallbacks.NonObservingRange();
  if (std::any_of(begin, end,
                  [&](const RefPtr<DebuggerNotificationCallback>& callback) {
                    return *callback == aHandlerFn;
                  })) {
    return false;
  }

  RefPtr<DebuggerNotificationCallback> handlerFn(&aHandlerFn);
  mEventListenerCallbacks.AppendElement(handlerFn);
  return true;
}

bool DebuggerNotificationObserver::RemoveListener(
    DebuggerNotificationCallback& aHandlerFn) {
  for (nsTObserverArray<RefPtr<DebuggerNotificationCallback>>::ForwardIterator
           iter(mEventListenerCallbacks);
       iter.HasMore();) {
    if (*iter.GetNext().get() == aHandlerFn) {
      iter.Remove();
      return true;
    }
  }

  return false;
}

bool DebuggerNotificationObserver::HasListeners() {
  return !mEventListenerCallbacks.IsEmpty();
}

void DebuggerNotificationObserver::NotifyListeners(
    DebuggerNotification* aNotification) {
  if (!HasListeners()) {
    return;
  }

  // Since we want the notification objects to live in the same compartment
  // as the observer, we create a new instance of the notification before
  // an observer dispatches the event listeners.
  RefPtr<DebuggerNotification> debuggerNotification(
      aNotification->CloneInto(mOwnerGlobal));

  for (RefPtr<DebuggerNotificationCallback> callback :
       mEventListenerCallbacks.ForwardRange()) {
    callback->Call(*debuggerNotification);
  }
}

}  // namespace mozilla::dom
