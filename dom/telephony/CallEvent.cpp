/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CallEvent.h"

#include "nsDOMClassInfo.h"

#include "Telephony.h"
#include "TelephonyCall.h"

USING_TELEPHONY_NAMESPACE

// static
already_AddRefed<CallEvent>
CallEvent::Create(TelephonyCall* aCall)
{
  NS_ASSERTION(aCall, "Null pointer!");

  nsRefPtr<CallEvent> event = new CallEvent();

  event->mCall = aCall;

  return event.forget();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(CallEvent)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(CallEvent,
                                                  nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_PTR(tmp->mCall->ToISupports(),
                                               TelephonyCall, "mCall")
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(CallEvent,
                                                nsDOMEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mCall)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(CallEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCallEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CallEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(CallEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(CallEvent, nsDOMEvent)

DOMCI_DATA(CallEvent, CallEvent)

NS_IMETHODIMP
CallEvent::GetCall(nsIDOMTelephonyCall** aCall)
{
  nsCOMPtr<nsIDOMTelephonyCall> call = mCall.get();
  call.forget(aCall);
  return NS_OK;
}
