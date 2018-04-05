/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventListenerManager.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/EventTargetBinding.h"
#include "mozilla/dom/ConstructibleEventTarget.h"
#include "nsIGlobalObject.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {

/* static */
already_AddRefed<EventTarget>
EventTarget::Constructor(const GlobalObject& aGlobal, ErrorResult& aRv)
{
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  RefPtr<EventTarget> target = new ConstructibleEventTarget(global);
  return target.forget();
}

bool
EventTarget::ComputeWantsUntrusted(const Nullable<bool>& aWantsUntrusted,
                                   ErrorResult& aRv)
{
  if (!aWantsUntrusted.IsNull()) {
    return aWantsUntrusted.Value();
  }

  bool defaultWantsUntrusted = ComputeDefaultWantsUntrusted(aRv);
  if (aRv.Failed()) {
    return false;
  }

  return defaultWantsUntrusted;
}

void
EventTarget::AddEventListener(const nsAString& aType,
                              EventListener* aCallback,
                              const AddEventListenerOptionsOrBoolean& aOptions,
                              const Nullable<bool>& aWantsUntrusted,
                              ErrorResult& aRv)
{
  bool wantsUntrusted = ComputeWantsUntrusted(aWantsUntrusted, aRv);
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

nsresult
EventTarget::AddEventListener(const nsAString& aType,
                              nsIDOMEventListener* aListener,
                              bool aUseCapture,
                              const Nullable<bool>& aWantsUntrusted)
{
  ErrorResult rv;
  bool wantsUntrusted = ComputeWantsUntrusted(aWantsUntrusted, rv);
  if (rv.Failed()) {
    return rv.StealNSResult();
  }

  EventListenerManager* elm = GetOrCreateListenerManager();
  NS_ENSURE_STATE(elm);
  elm->AddEventListener(aType, aListener, aUseCapture, wantsUntrusted);
  return NS_OK;
}

void
EventTarget::RemoveEventListener(const nsAString& aType,
                                 EventListener* aListener,
                                 const EventListenerOptionsOrBoolean& aOptions,
                                 ErrorResult& aRv)
{
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aOptions);
  }
}

void
EventTarget::RemoveEventListener(const nsAString& aType,
                                 nsIDOMEventListener* aListener,
                                 bool aUseCapture)
{
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    elm->RemoveEventListener(aType, aListener, aUseCapture);
  }
}

nsresult
EventTarget::AddSystemEventListener(const nsAString& aType,
                                    nsIDOMEventListener* aListener,
                                    bool aUseCapture,
                                    const Nullable<bool>& aWantsUntrusted)
{
  ErrorResult rv;
  bool wantsUntrusted = ComputeWantsUntrusted(aWantsUntrusted, rv);
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

void
EventTarget::RemoveSystemEventListener(const nsAString& aType,
                                       nsIDOMEventListener *aListener,
                                       bool aUseCapture)
{
  EventListenerManager* elm = GetExistingListenerManager();
  if (elm) {
    EventListenerFlags flags;
    flags.mInSystemGroup = true;
    flags.mCapture = aUseCapture;
    elm->RemoveEventListenerByType(aListener, aType, flags);
  }
}

EventHandlerNonNull*
EventTarget::GetEventHandler(nsAtom* aType, const nsAString& aTypeString)
{
  EventListenerManager* elm = GetExistingListenerManager();
  return elm ? elm->GetEventHandler(aType, aTypeString) : nullptr;
}

void
EventTarget::SetEventHandler(const nsAString& aType,
                             EventHandlerNonNull* aHandler,
                             ErrorResult& aRv)
{
  if (!StringBeginsWith(aType, NS_LITERAL_STRING("on"))) {
    aRv.Throw(NS_ERROR_INVALID_ARG);
    return;
  }
  if (NS_IsMainThread()) {
    RefPtr<nsAtom> type = NS_Atomize(aType);
    SetEventHandler(type, EmptyString(), aHandler);
    return;
  }
  SetEventHandler(nullptr,
                  Substring(aType, 2), // Remove "on"
                  aHandler);
}

void
EventTarget::SetEventHandler(nsAtom* aType, const nsAString& aTypeString,
                             EventHandlerNonNull* aHandler)
{
  GetOrCreateListenerManager()->SetEventHandler(aType, aTypeString, aHandler);
}

bool
EventTarget::HasNonSystemGroupListenersForUntrustedKeyEvents() const
{
  EventListenerManager* elm = GetExistingListenerManager();
  return elm && elm->HasNonSystemGroupListenersForUntrustedKeyEvents();
}

bool
EventTarget::HasNonPassiveNonSystemGroupListenersForUntrustedKeyEvents() const
{
  EventListenerManager* elm = GetExistingListenerManager();
  return elm && elm->HasNonPassiveNonSystemGroupListenersForUntrustedKeyEvents();
}

bool
EventTarget::IsApzAware() const
{
  EventListenerManager* elm = GetExistingListenerManager();
  return elm && elm->HasApzAwareListeners();
}

void
EventTarget::DispatchEvent(Event& aEvent)
{
  // The caller type doesn't really matter if we don't care about the
  // return value, but let's be safe and pass NonSystem.
  Unused << DispatchEvent(aEvent, CallerType::NonSystem, IgnoreErrors());
}

void
EventTarget::DispatchEvent(Event& aEvent, ErrorResult& aRv)
{
  // The caller type doesn't really matter if we don't care about the
  // return value, but let's be safe and pass NonSystem.
  Unused << DispatchEvent(aEvent, CallerType::NonSystem, IgnoreErrors());
}

} // namespace dom
} // namespace mozilla
