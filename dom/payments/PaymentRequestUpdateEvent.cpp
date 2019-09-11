/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PaymentRequestUpdateEvent.h"
#include "mozilla/dom/RootedDictionary.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(PaymentRequestUpdateEvent, Event, mRequest)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PaymentRequestUpdateEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaymentRequestUpdateEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(PaymentRequestUpdateEvent, Event)
NS_IMPL_RELEASE_INHERITED(PaymentRequestUpdateEvent, Event)

already_AddRefed<PaymentRequestUpdateEvent>
PaymentRequestUpdateEvent::Constructor(
    mozilla::dom::EventTarget* aOwner, const nsAString& aType,
    const PaymentRequestUpdateEventInit& aEventInitDict) {
  RefPtr<PaymentRequestUpdateEvent> e = new PaymentRequestUpdateEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

already_AddRefed<PaymentRequestUpdateEvent>
PaymentRequestUpdateEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const PaymentRequestUpdateEventInit& aEventInitDict) {
  nsCOMPtr<mozilla::dom::EventTarget> owner =
      do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aEventInitDict);
}

PaymentRequestUpdateEvent::PaymentRequestUpdateEvent(EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr),
      mWaitForUpdate(false),
      mRequest(nullptr) {
  MOZ_ASSERT(aOwner);
}

void PaymentRequestUpdateEvent::ResolvedCallback(JSContext* aCx,
                                                 JS::Handle<JS::Value> aValue) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(mRequest);
  if (!mRequest->InFullyActiveDocument()) {
    return;
  }

  if (NS_WARN_IF(!aValue.isObject()) || !mWaitForUpdate) {
    return;
  }

  // Converting value to a PaymentDetailsUpdate dictionary
  RootedDictionary<PaymentDetailsUpdate> details(aCx);
  if (!details.Init(aCx, aValue)) {
    mRequest->AbortUpdate(NS_ERROR_TYPE_ERR);
    JS_ClearPendingException(aCx);
    return;
  }

  // Validate and canonicalize the details
  // requestShipping must be true here. PaymentRequestUpdateEvent is only
  // dispatched when shippingAddress/shippingOption is changed, and it also
  // means Options.RequestShipping must be true while creating the corresponding
  // PaymentRequest.
  nsresult rv =
      mRequest->IsValidDetailsUpdate(details, true /*aRequestShipping*/);
  if (NS_FAILED(rv)) {
    mRequest->AbortUpdate(rv);
    return;
  }

  // Update the PaymentRequest with the new details
  if (NS_FAILED(mRequest->UpdatePayment(aCx, details))) {
    mRequest->AbortUpdate(NS_ERROR_DOM_ABORT_ERR);
    return;
  }
  mWaitForUpdate = false;
  mRequest->SetUpdating(false);
}

void PaymentRequestUpdateEvent::RejectedCallback(JSContext* aCx,
                                                 JS::Handle<JS::Value> aValue) {
  MOZ_ASSERT(mRequest);
  if (!mRequest->InFullyActiveDocument()) {
    return;
  }

  mRequest->AbortUpdate(NS_ERROR_DOM_ABORT_ERR);
  mWaitForUpdate = false;
  mRequest->SetUpdating(false);
}

void PaymentRequestUpdateEvent::UpdateWith(Promise& aPromise,
                                           ErrorResult& aRv) {
  if (!IsTrusted()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  MOZ_ASSERT(mRequest);
  if (!mRequest->InFullyActiveDocument()) {
    return;
  }

  if (mWaitForUpdate || !mRequest->ReadyForUpdate()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return;
  }

  aPromise.AppendNativeHandler(this);

  StopPropagation();
  StopImmediatePropagation();
  mWaitForUpdate = true;
  mRequest->SetUpdating(true);
}

void PaymentRequestUpdateEvent::SetRequest(PaymentRequest* aRequest) {
  MOZ_ASSERT(IsTrusted());
  MOZ_ASSERT(!mRequest);
  MOZ_ASSERT(aRequest);

  mRequest = aRequest;
}

PaymentRequestUpdateEvent::~PaymentRequestUpdateEvent() {}

JSObject* PaymentRequestUpdateEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return PaymentRequestUpdateEvent_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace dom
}  // namespace mozilla
