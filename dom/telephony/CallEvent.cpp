/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallEvent.h"
#include "mozilla/dom/CallEventBinding.h"

#include "TelephonyCall.h"

using namespace mozilla::dom;
using mozilla::ErrorResult;

/* static */
already_AddRefed<CallEvent>
CallEvent::Create(EventTarget* aOwner, const nsAString& aType,
                  TelephonyCall* aCall, bool aCanBubble,
                  bool aCancelable)
{
  nsRefPtr<CallEvent> event = new CallEvent(aOwner, nullptr, nullptr);
  event->mCall = aCall;
  event->InitEvent(aType, aCanBubble, aCancelable);
  return event.forget();
}

JSObject*
CallEvent::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return CallEventBinding::Wrap(aCx, aScope, this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(CallEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CallEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCall)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CallEvent, Event)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCall)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(CallEvent, Event)
NS_IMPL_RELEASE_INHERITED(CallEvent, Event)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CallEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

// WebIDL

/* static */
already_AddRefed<CallEvent>
CallEvent::Constructor(const GlobalObject& aGlobal, const nsAString& aType,
                       const CallEventInit& aOptions, ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> target = do_QueryInterface(aGlobal.GetAsSupports());

  if (!target) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsRefPtr<CallEvent> event = Create(target, aType, aOptions.mCall, false, false);

  return event.forget();
}

already_AddRefed<TelephonyCall>
CallEvent::GetCall() const
{
  nsRefPtr<TelephonyCall> call = mCall;
  return call.forget();
}
