/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/ConstructibleEventTarget.h"
#include "mozilla/dom/Nullable.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "nsGlobalWindowInner.h"
#include "nsGlobalWindowOuter.h"
#include "nsIGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"

namespace mozilla::dom {

#ifndef NS_BUILD_REFCNT_LOGGING
MozExternalRefCountType EventTarget::NonVirtualAddRef() {
  return mRefCnt.incr(this);
}

MozExternalRefCountType EventTarget::NonVirtualRelease() {
  if (mRefCnt.get() == 1) {
    return Release();
  }
  return mRefCnt.decr(this);
}
#endif

NS_IMETHODIMP_(MozExternalRefCountType) EventTarget::AddRef() {
  MOZ_ASSERT_UNREACHABLE("EventTarget::AddRef should not be called");
  return 0;
}

NS_IMETHODIMP_(MozExternalRefCountType) EventTarget::Release() {
  MOZ_ASSERT_UNREACHABLE("EventTarget::Release should not be called");
  return 0;
}

NS_IMETHODIMP EventTarget::QueryInterface(REFNSIID aIID, void** aInstancePtr) {
  MOZ_ASSERT_UNREACHABLE("EventTarget::QueryInterface should not be called");
  *aInstancePtr = nullptr;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP_(void) EventTarget::DeleteCycleCollectable() {
  MOZ_ASSERT_UNREACHABLE(
      "EventTarget::DeleteCycleCollectable should not be called");
}

/* static */
already_AddRefed<EventTarget> EventTarget::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  RefPtr<EventTarget> target = new ConstructibleEventTarget(global);
  return target.forget();
}

bool EventTarget::ComputeWantsUntrusted(
    const Nullable<bool>& aWantsUntrusted,
    const AddEventListenerOptionsOrBoolean* aOptions, ErrorResult& aRv) {
  if (aOptions && aOptions->IsAddEventListenerOptions()) {
    const auto& options = aOptions->GetAsAddEventListenerOptions();
    if (options.mWantUntrusted.WasPassed()) {
      return options.mWantUntrusted.Value();
    }
  }

  if (!aWantsUntrusted.IsNull()) {
    return aWantsUntrusted.Value();
  }

  bool defaultWantsUntrusted = ComputeDefaultWantsUntrusted(aRv);
  if (aRv.Failed()) {
    return false;
  }

  return defaultWantsUntrusted;
}

void EventTarget::AddEventListener(
    const nsAString& aType, EventListener* aCallback,
    const AddEventListenerOptionsOrBoolean& aOptions,
    const Nullable<bool>& aWantsUntrusted, ErrorResult& aRv) {
  bool wantsUntrusted = ComputeWantsUntrusted(aWantsUntrusted, &aOptions, aRv);
  if (aRv.Failed()) {
    return;
  }

  EventListenerManager* elm = GetOrCreateListenerManager();
  if (!elm) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  elm->AddEventListener(aType, aCallback, aOptions, wantsUntrusted);
}

nsresult EventTarget::AddEventListener(const nsAString& aType,
                                       nsIDOMEventListener* aListener,
                                       bool aUseCapture,
                                       const Nullable<bool>& aWantsUntrusted) {
  ErrorResult rv;
  bool wantsUntrusted = ComputeWantsUntrusted(aWantsUntrusted, nullptr, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  EventListenerManager* elm = GetOrCreateListenerManager();
  NS_ENSURE_STATE(elm);
  elm->AddEventListener(aType, aListener, aUseCapture, wantsUntrusted);
  return NS_OK;
}

void EventTarget::RemoveEventListener(
    const nsAString& aType, EventListener* aListener,
    const EventListenerOptionsOrBoolean& aOptions, ErrorResult& aRv) {
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aOptions);
  }
}

void EventTarget::RemoveEventListener(const nsAString& aType,
                                      nsIDOMEventListener* aListener,
                                      bool aUseCapture) {
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
}

nsresult EventTarget::AddSystemEventListener(
    const nsAString& aType, nsIDOMEventListener* aListener, bool aUseCapture,
    const Nullable<bool>& aWantsUntrusted) {
  ErrorResult rv;
  bool wantsUntrusted = ComputeWantsUntrusted(aWantsUntrusted, nullptr, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  EventListenerManager* elm = GetOrCreateListenerManager();
  NS_ENSURE_STATE(elm);

  EventListenerFlags flags;
  flags.mInSystemGroup = true;
  flags.mCapture = aUseCapture;
  flags.mAllowUntrustedEvents = wantsUntrusted;
  elm->AddEventListenerByType(aListener, aType, flags);
  return NS_OK;
}

void EventTarget::RemoveSystemEventListener(const nsAString& aType,
                                            nsIDOMEventListener* aListener,
                                            bool aUseCapture) {
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    EventListenerFlags flags;
    flags.mInSystemGroup = true;
    flags.mCapture = aUseCapture;
    elm->RemoveEventListenerByType(aListener, aType, flags);
  }
}

EventHandlerNonNull* EventTarget::GetEventHandler(nsAtom* aType) {
  EventListenerManager* elm = GetExistingListenerManager();
  return elm ? elm->GetEventHandler(aType) : nullptr;
}

void EventTarget::SetEventHandler(const nsAString& aType,
                                  EventHandlerNonNull* aHandler,
                                  ErrorResult& aRv) {
  if (!StringBeginsWith(aType, u"on"_ns)) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  RefPtr<nsAtom> type = NS_Atomize(aType);
  SetEventHandler(type, aHandler);
}

void EventTarget::SetEventHandler(nsAtom* aType,
                                  EventHandlerNonNull* aHandler) {
  GetOrCreateListenerManager()->SetEventHandler(aType, aHandler);
}

bool EventTarget::HasNonSystemGroupListenersForUntrustedKeyEvents() const {
  EventListenerManager* elm = GetExistingListenerManager();
  return elm && elm->HasNonSystemGroupListenersForUntrustedKeyEvents();
}

bool EventTarget::HasNonPassiveNonSystemGroupListenersForUntrustedKeyEvents()
    const {
  EventListenerManager* elm = GetExistingListenerManager();
  return elm &&
         elm->HasNonPassiveNonSystemGroupListenersForUntrustedKeyEvents();
}

bool EventTarget::IsApzAware() const {
  EventListenerManager* elm = GetExistingListenerManager();
  return elm && elm->HasApzAwareListeners();
}

void EventTarget::DispatchEvent(Event& aEvent) {
  // The caller type doesn't really matter if we don't care about the
  // return value, but let's be safe and pass NonSystem.
  Unused << DispatchEvent(aEvent, CallerType::NonSystem, IgnoreErrors());
}

void EventTarget::DispatchEvent(Event& aEvent, ErrorResult& aRv) {
  // The caller type doesn't really matter if we don't care about the
  // return value, but let's be safe and pass NonSystem.
  Unused << DispatchEvent(aEvent, CallerType::NonSystem, IgnoreErrors());
}

Nullable<WindowProxyHolder> EventTarget::GetOwnerGlobalForBindings() {
  nsPIDOMWindowOuter* win = GetOwnerGlobalForBindingsInternal();
  if (!win) {
    return nullptr;
  }

  return WindowProxyHolder(win->GetBrowsingContext());
}

nsPIDOMWindowInner* EventTarget::GetAsInnerWindow() {
  return IsInnerWindow() ? static_cast<nsGlobalWindowInner*>(this) : nullptr;
}

const nsPIDOMWindowInner* EventTarget::GetAsInnerWindow() const {
  return IsInnerWindow() ? static_cast<const nsGlobalWindowInner*>(this)
                         : nullptr;
}

nsPIDOMWindowOuter* EventTarget::GetAsOuterWindow() {
  return IsOuterWindow() ? static_cast<nsGlobalWindowOuter*>(this) : nullptr;
}

const nsPIDOMWindowOuter* EventTarget::GetAsOuterWindow() const {
  return IsOuterWindow() ? static_cast<const nsGlobalWindowOuter*>(this)
                         : nullptr;
}

nsPIDOMWindowInner* EventTarget::AsInnerWindow() {
  MOZ_DIAGNOSTIC_ASSERT(IsInnerWindow());
  return static_cast<nsGlobalWindowInner*>(this);
}

const nsPIDOMWindowInner* EventTarget::AsInnerWindow() const {
  MOZ_DIAGNOSTIC_ASSERT(IsInnerWindow());
  return static_cast<const nsGlobalWindowInner*>(this);
}

nsPIDOMWindowOuter* EventTarget::AsOuterWindow() {
  MOZ_DIAGNOSTIC_ASSERT(IsOuterWindow());
  return static_cast<nsGlobalWindowOuter*>(this);
}

const nsPIDOMWindowOuter* EventTarget::AsOuterWindow() const {
  MOZ_DIAGNOSTIC_ASSERT(IsOuterWindow());
  return static_cast<const nsGlobalWindowOuter*>(this);
}

}  // namespace mozilla::dom
