/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PaymentRequestUpdateEvent.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(PaymentRequestUpdateEvent, Event, mRequest)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PaymentRequestUpdateEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(PaymentRequestUpdateEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(PaymentRequestUpdateEvent, Event)
NS_IMPL_RELEASE_INHERITED(PaymentRequestUpdateEvent, Event)

already_AddRefed<PaymentRequestUpdateEvent>
PaymentRequestUpdateEvent::Constructor(mozilla::dom::EventTarget* aOwner,
                                       const nsAString& aType,
                                       const PaymentRequestUpdateEventInit& aEventInitDict)
{
  RefPtr<PaymentRequestUpdateEvent> e = new PaymentRequestUpdateEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

already_AddRefed<PaymentRequestUpdateEvent>
PaymentRequestUpdateEvent::Constructor(const GlobalObject& aGlobal,
                                       const nsAString& aType,
                                       const PaymentRequestUpdateEventInit& aEventInitDict,
                                       ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aEventInitDict);
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(EventTarget* aOwner)
  : Event(aOwner, nullptr, nullptr)
  , mWaitForUpdate(false)
  , mRequest(nullptr)
{
  MOZ_ASSERT(aOwner);
}

void
PaymentRequestUpdateEvent::ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(mRequest);

  if (NS_WARN_IF(!aValue.isObject()) || !mWaitForUpdate) {
    return;
  }

  // Converting value to a PaymentDetailsUpdate dictionary
  PaymentDetailsUpdate details;
  if (!details.Init(aCx, aValue)) {
    return;
  }

  // Validate and canonicalize the details
  if (!mRequest->IsValidDetailsUpdate(details)) {
    mRequest->AbortUpdate(NS_ERROR_TYPE_ERR);
    return;
  }

  // [TODO]
  // If the data member of modifier is present,
  // let serializedData be the result of JSON-serializing modifier.data into a string.
  // null if it is not.

  // Update the PaymentRequest with the new details
  if (NS_FAILED(mRequest->UpdatePayment(details))) {
    mRequest->AbortUpdate(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
  mWaitForUpdate = false;
  mRequest->SetUpdating(false);
}

void
PaymentRequestUpdateEvent::RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue)
{
  MOZ_ASSERT(mRequest);

  mRequest->AbortUpdate(NS_ERROR_DOM_ABORT_ERR);
  mWaitForUpdate = false;
  mRequest->SetUpdating(false);
}

void
PaymentRequestUpdateEvent::UpdateWith(Promise& aPromise, ErrorResult& aRv)
{
  MOZ_ASSERT(mRequest);

  if (mWaitForUpdate || !mRequest->ReadyForUpdate() ||
      !mEvent->mFlags.mIsBeingDispatched) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  aPromise.AppendNativeHandler(this);

  StopPropagation();
  StopImmediatePropagation();
  mWaitForUpdate = true;
  mRequest->SetUpdating(true);
}

void
PaymentRequestUpdateEvent::SetRequest(PaymentRequest* aRequest)
{
  MOZ_ASSERT(IsTrusted());
  MOZ_ASSERT(!mRequest);
  MOZ_ASSERT(aRequest);

  mRequest = aRequest;
}

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent()
{
}

JSObject*
PaymentRequestUpdateEvent::WrapObjectInternal(JSContext* aCx,
                                              JS::Handle<JSObject*> aGivenProto)
{
  return PaymentRequestUpdateEventBinding::Wrap(aCx, this, aGivenProto);
}


} // namespace dom
} // namespace mozilla
