/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MerchantValidationEvent.h"
#include "nsNetCID.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/PaymentRequest.h"
#include "mozilla/dom/Location.h"
#include "mozilla/dom/URL.h"
#include "mozilla/ResultExtensions.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(MerchantValidationEvent, Event,
                                   mValidationURL, mRequest)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MerchantValidationEvent, Event)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MerchantValidationEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(MerchantValidationEvent, Event)
NS_IMPL_RELEASE_INHERITED(MerchantValidationEvent, Event)

// User-land code constructor
already_AddRefed<MerchantValidationEvent> MerchantValidationEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const MerchantValidationEventInit& aEventInitDict, ErrorResult& aRv) {
  // validate passed URL
  nsCOMPtr<mozilla::dom::EventTarget> owner =
      do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aEventInitDict, aRv);
}

// Internal JS object constructor
already_AddRefed<MerchantValidationEvent> MerchantValidationEvent::Constructor(
    EventTarget* aOwner, const nsAString& aType,
    const MerchantValidationEventInit& aEventInitDict, ErrorResult& aRv) {
  RefPtr<MerchantValidationEvent> e = new MerchantValidationEvent(aOwner);
  bool trusted = e->Init(aOwner);
  e->InitEvent(aType, aEventInitDict.mBubbles, aEventInitDict.mCancelable);
  e->init(aEventInitDict, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }
  e->SetTrusted(trusted);
  e->SetComposed(aEventInitDict.mComposed);
  return e.forget();
}

void MerchantValidationEvent::init(
    const MerchantValidationEventInit& aEventInitDict, ErrorResult& aRv) {
  // Check methodName is valid
  if (!aEventInitDict.mMethodName.IsEmpty()) {
    PaymentRequest::IsValidPaymentMethodIdentifier(aEventInitDict.mMethodName,
                                                   aRv);
    if (aRv.Failed()) {
      return;
    }
  }
  SetMethodName(aEventInitDict.mMethodName);
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(GetParentObject());
  auto doc = window->GetExtantDoc();
  if (!doc) {
    aRv.ThrowAbortError("The owner document does not exist");
    return;
  }

  Result<OwningNonNull<nsIURI>, nsresult> rv =
      doc->ResolveWithBaseURI(aEventInitDict.mValidationURL);
  if (rv.isErr()) {
    aRv.ThrowTypeError("validationURL cannot be parsed");
    return;
  }
  mValidationURL = rv.unwrap();
}

MerchantValidationEvent::MerchantValidationEvent(EventTarget* aOwner)
    : Event(aOwner, nullptr, nullptr), mWaitForUpdate(false) {
  MOZ_ASSERT(aOwner);
}

void MerchantValidationEvent::ResolvedCallback(JSContext* aCx,
                                               JS::Handle<JS::Value> aValue,
                                               ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(mRequest);

  if (!mWaitForUpdate) {
    return;
  }
  mWaitForUpdate = false;

  // If we eventually end up supporting merchant validation
  // we would validate `aValue` here, as per:
  // https://w3c.github.io/payment-request/#validate-merchant-s-details-algorithm
  //
  // Right now, MerchantValidationEvent is only implemented for standards
  // conformance, which is why at this point we throw a
  // NS_ERROR_DOM_NOT_SUPPORTED_ERR.

  ErrorResult result;
  result.ThrowNotSupportedError(
      "complete() is not supported by Firefox currently");
  mRequest->AbortUpdate(result);
  mRequest->SetUpdating(false);
}

void MerchantValidationEvent::RejectedCallback(JSContext* aCx,
                                               JS::Handle<JS::Value> aValue,
                                               ErrorResult& aRv) {
  MOZ_ASSERT(mRequest);
  if (!mWaitForUpdate) {
    return;
  }
  mWaitForUpdate = false;
  ErrorResult result;
  result.ThrowAbortError(
      "The promise for MerchantValidtaionEvent.complete() is rejected");
  mRequest->AbortUpdate(result);
  mRequest->SetUpdating(false);
}

void MerchantValidationEvent::Complete(Promise& aPromise, ErrorResult& aRv) {
  if (!IsTrusted()) {
    aRv.ThrowInvalidStateError("Called on an untrusted event");
    return;
  }

  MOZ_ASSERT(mRequest);

  if (mWaitForUpdate) {
    aRv.ThrowInvalidStateError(
        "The MerchantValidationEvent is waiting for update");
    return;
  }

  if (!mRequest->ReadyForUpdate()) {
    aRv.ThrowInvalidStateError(
        "The PaymentRequest state is not eInteractive or the PaymentRequest is "
        "updating");
    return;
  }

  aPromise.AppendNativeHandler(this);

  StopPropagation();
  StopImmediatePropagation();
  mWaitForUpdate = true;
  mRequest->SetUpdating(true);
}

void MerchantValidationEvent::SetRequest(PaymentRequest* aRequest) {
  MOZ_ASSERT(IsTrusted());
  MOZ_ASSERT(!mRequest);
  MOZ_ASSERT(aRequest);

  mRequest = aRequest;
}

void MerchantValidationEvent::GetValidationURL(nsAString& aValidationURL) {
  nsAutoCString utf8href;
  nsresult rv = mValidationURL->GetSpec(utf8href);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  Unused << rv;
  aValidationURL.Assign(NS_ConvertUTF8toUTF16(utf8href));
}

void MerchantValidationEvent::GetMethodName(nsAString& aMethodName) {
  aMethodName.Assign(mMethodName);
}

void MerchantValidationEvent::SetMethodName(const nsAString& aMethodName) {
  mMethodName.Assign(aMethodName);
}

MerchantValidationEvent::~MerchantValidationEvent() = default;

JSObject* MerchantValidationEvent::WrapObjectInternal(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return MerchantValidationEvent_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
