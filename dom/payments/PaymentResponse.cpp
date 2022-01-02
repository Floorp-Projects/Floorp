/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/dom/PaymentResponse.h"
#include "mozilla/dom/BasicCardPaymentBinding.h"
#include "mozilla/dom/PaymentRequestUpdateEvent.h"
#include "BasicCardPayment.h"
#include "PaymentAddress.h"
#include "PaymentRequest.h"
#include "PaymentRequestManager.h"
#include "PaymentRequestUtils.h"
#include "mozilla/EventStateManager.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(PaymentResponse)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(PaymentResponse,
                                               DOMEventTargetHelper)
  // Don't need NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER because
  // DOMEventTargetHelper does it for us.
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PaymentResponse,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mShippingAddress)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mTimer)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PaymentResponse,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mShippingAddress)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mTimer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaymentResponse)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(PaymentResponse, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(PaymentResponse, DOMEventTargetHelper)

PaymentResponse::PaymentResponse(
    nsPIDOMWindowInner* aWindow, PaymentRequest* aRequest,
    const nsAString& aRequestId, const nsAString& aMethodName,
    const nsAString& aShippingOption, PaymentAddress* aShippingAddress,
    const ResponseData& aDetails, const nsAString& aPayerName,
    const nsAString& aPayerEmail, const nsAString& aPayerPhone)
    : DOMEventTargetHelper(aWindow),
      mCompleteCalled(false),
      mRequest(aRequest),
      mRequestId(aRequestId),
      mMethodName(aMethodName),
      mDetails(aDetails),
      mShippingOption(aShippingOption),
      mPayerName(aPayerName),
      mPayerEmail(aPayerEmail),
      mPayerPhone(aPayerPhone),
      mShippingAddress(aShippingAddress) {
  // TODO: from https://github.com/w3c/browser-payment-api/issues/480
  // Add payerGivenName + payerFamilyName to PaymentAddress
  NS_NewTimerWithCallback(getter_AddRefs(mTimer), this,
                          StaticPrefs::dom_payments_response_timeout(),
                          nsITimer::TYPE_ONE_SHOT,
                          aWindow->EventTargetFor(TaskCategory::Other));
}

PaymentResponse::~PaymentResponse() = default;

JSObject* PaymentResponse::WrapObject(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return PaymentResponse_Binding::Wrap(aCx, this, aGivenProto);
}

void PaymentResponse::GetRequestId(nsString& aRetVal) const {
  aRetVal = mRequestId;
}

void PaymentResponse::GetMethodName(nsString& aRetVal) const {
  aRetVal = mMethodName;
}

void PaymentResponse::GetDetails(JSContext* aCx,
                                 JS::MutableHandle<JSObject*> aRetVal) const {
  switch (mDetails.type()) {
    case ResponseData::GeneralResponse: {
      const GeneralData& rawData = mDetails.generalData();
      DeserializeToJSObject(rawData.data, aCx, aRetVal);
      break;
    }
    case ResponseData::BasicCardResponse: {
      const BasicCardData& rawData = mDetails.basicCardData();
      BasicCardResponse basicCardResponse;
      if (!rawData.cardholderName.IsEmpty()) {
        basicCardResponse.mCardholderName = rawData.cardholderName;
      }
      basicCardResponse.mCardNumber = rawData.cardNumber;
      if (!rawData.expiryMonth.IsEmpty()) {
        basicCardResponse.mExpiryMonth = rawData.expiryMonth;
      }
      if (!rawData.expiryYear.IsEmpty()) {
        basicCardResponse.mExpiryYear = rawData.expiryYear;
      }
      if (!rawData.cardSecurityCode.IsEmpty()) {
        basicCardResponse.mCardSecurityCode = rawData.cardSecurityCode;
      }
      if (!rawData.billingAddress.country.IsEmpty() ||
          !rawData.billingAddress.addressLine.IsEmpty() ||
          !rawData.billingAddress.region.IsEmpty() ||
          !rawData.billingAddress.regionCode.IsEmpty() ||
          !rawData.billingAddress.city.IsEmpty() ||
          !rawData.billingAddress.dependentLocality.IsEmpty() ||
          !rawData.billingAddress.postalCode.IsEmpty() ||
          !rawData.billingAddress.sortingCode.IsEmpty() ||
          !rawData.billingAddress.organization.IsEmpty() ||
          !rawData.billingAddress.recipient.IsEmpty() ||
          !rawData.billingAddress.phone.IsEmpty()) {
        basicCardResponse.mBillingAddress = new PaymentAddress(
            GetOwner(), rawData.billingAddress.country,
            rawData.billingAddress.addressLine, rawData.billingAddress.region,
            rawData.billingAddress.regionCode, rawData.billingAddress.city,
            rawData.billingAddress.dependentLocality,
            rawData.billingAddress.postalCode,
            rawData.billingAddress.sortingCode,
            rawData.billingAddress.organization,
            rawData.billingAddress.recipient, rawData.billingAddress.phone);
      }
      MOZ_ASSERT(aCx);
      JS::RootedValue value(aCx);
      if (NS_WARN_IF(!basicCardResponse.ToObjectInternal(aCx, &value))) {
        return;
      }
      aRetVal.set(&value.toObject());
      break;
    }
    default: {
      MOZ_ASSERT(false);
      break;
    }
  }
}

void PaymentResponse::GetShippingOption(nsString& aRetVal) const {
  aRetVal = mShippingOption;
}

void PaymentResponse::GetPayerName(nsString& aRetVal) const {
  aRetVal = mPayerName;
}

void PaymentResponse::GetPayerEmail(nsString& aRetVal) const {
  aRetVal = mPayerEmail;
}

void PaymentResponse::GetPayerPhone(nsString& aRetVal) const {
  aRetVal = mPayerPhone;
}

// TODO:
// Return a raw pointer here to avoid refcounting, but make sure it's safe
// (the object should be kept alive by the callee).
already_AddRefed<PaymentAddress> PaymentResponse::GetShippingAddress() const {
  RefPtr<PaymentAddress> address = mShippingAddress;
  return address.forget();
}

already_AddRefed<Promise> PaymentResponse::Complete(PaymentComplete result,
                                                    ErrorResult& aRv) {
  MOZ_ASSERT(mRequest);
  if (!mRequest->InFullyActiveDocument()) {
    aRv.ThrowAbortError("The owner document is not fully active");
    return nullptr;
  }

  if (mCompleteCalled) {
    aRv.ThrowInvalidStateError(
        "PaymentResponse.complete() has already been called");
    return nullptr;
  }

  mCompleteCalled = true;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  manager->CompletePayment(mRequest, result, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (NS_WARN_IF(!GetOwner())) {
    aRv.ThrowAbortError("Global object should exist");
    return nullptr;
  }

  nsIGlobalObject* global = GetOwner()->AsGlobal();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  mPromise = promise;
  return promise.forget();
}

void PaymentResponse::RespondComplete() {
  // mPromise may be null when timing out
  if (mPromise) {
    mPromise->MaybeResolve(JS::UndefinedHandleValue);
    mPromise = nullptr;
  }
}

already_AddRefed<Promise> PaymentResponse::Retry(
    JSContext* aCx, const PaymentValidationErrors& aErrors, ErrorResult& aRv) {
  MOZ_ASSERT(mRequest);
  if (!mRequest->InFullyActiveDocument()) {
    aRv.ThrowAbortError("The owner document is not fully active");
    return nullptr;
  }

  nsIGlobalObject* global = GetOwner()->AsGlobal();
  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  if (mCompleteCalled || mRetryPromise) {
    aRv.ThrowInvalidStateError(
        "PaymentResponse.complete() has already been called");
    return nullptr;
  }

  if (mRetryPromise) {
    aRv.ThrowInvalidStateError("Is retrying the PaymentRequest");
    return nullptr;
  }

  ValidatePaymentValidationErrors(aErrors, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  // Depending on the PMI, try to do IDL type conversion
  // (e.g., basic-card expects at BasicCardErrors dictionary)
  ConvertPaymentMethodErrors(aCx, aErrors, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  MOZ_ASSERT(mRequest);
  mRequest->RetryPayment(aCx, aErrors, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  mRetryPromise = promise;
  return promise.forget();
}

void PaymentResponse::RespondRetry(const nsAString& aMethodName,
                                   const nsAString& aShippingOption,
                                   PaymentAddress* aShippingAddress,
                                   const ResponseData& aDetails,
                                   const nsAString& aPayerName,
                                   const nsAString& aPayerEmail,
                                   const nsAString& aPayerPhone) {
  // mRetryPromise could be nulled when document activity is changed.
  if (!mRetryPromise) {
    return;
  }
  mMethodName = aMethodName;
  mShippingOption = aShippingOption;
  mShippingAddress = aShippingAddress;
  mDetails = aDetails;
  mPayerName = aPayerName;
  mPayerEmail = aPayerEmail;
  mPayerPhone = aPayerPhone;

  if (NS_WARN_IF(!GetOwner())) {
    return;
  }

  NS_NewTimerWithCallback(getter_AddRefs(mTimer), this,
                          StaticPrefs::dom_payments_response_timeout(),
                          nsITimer::TYPE_ONE_SHOT,
                          GetOwner()->EventTargetFor(TaskCategory::Other));
  MOZ_ASSERT(mRetryPromise);
  mRetryPromise->MaybeResolve(JS::UndefinedHandleValue);
  mRetryPromise = nullptr;
}

void PaymentResponse::RejectRetry(ErrorResult&& aRejectReason) {
  MOZ_ASSERT(mRetryPromise);
  mRetryPromise->MaybeReject(std::move(aRejectReason));
  mRetryPromise = nullptr;
}

void PaymentResponse::ConvertPaymentMethodErrors(
    JSContext* aCx, const PaymentValidationErrors& aErrors,
    ErrorResult& aRv) const {
  MOZ_ASSERT(aCx);
  if (!aErrors.mPaymentMethod.WasPassed()) {
    return;
  }
  RefPtr<BasicCardService> service = BasicCardService::GetService();
  MOZ_ASSERT(service);
  if (service->IsBasicCardPayment(mMethodName)) {
    MOZ_ASSERT(aErrors.mPaymentMethod.Value(),
               "The IDL says this is not nullable!");
    service->CheckForValidBasicCardErrors(aCx, aErrors.mPaymentMethod.Value(),
                                          aRv);
  }
}

void PaymentResponse::ValidatePaymentValidationErrors(
    const PaymentValidationErrors& aErrors, ErrorResult& aRv) {
  // Should not be empty errors
  // check PaymentValidationErrors.error
  if (aErrors.mError.WasPassed() && !aErrors.mError.Value().IsEmpty()) {
    return;
  }
  // check PaymentValidationErrors.payer
  if (aErrors.mPayer.WasPassed()) {
    PayerErrors payerErrors(aErrors.mPayer.Value());
    if (payerErrors.mName.WasPassed() && !payerErrors.mName.Value().IsEmpty()) {
      return;
    }
    if (payerErrors.mEmail.WasPassed() &&
        !payerErrors.mEmail.Value().IsEmpty()) {
      return;
    }
    if (payerErrors.mPhone.WasPassed() &&
        !payerErrors.mPhone.Value().IsEmpty()) {
      return;
    }
  }
  // check PaymentValidationErrors.paymentMethod
  if (aErrors.mPaymentMethod.WasPassed()) {
    return;
  }
  // check PaymentValidationErrors.shippingAddress
  if (aErrors.mShippingAddress.WasPassed()) {
    AddressErrors addErrors(aErrors.mShippingAddress.Value());
    if (addErrors.mAddressLine.WasPassed() &&
        !addErrors.mAddressLine.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mCity.WasPassed() && !addErrors.mCity.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mCountry.WasPassed() &&
        !addErrors.mCountry.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mDependentLocality.WasPassed() &&
        !addErrors.mDependentLocality.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mOrganization.WasPassed() &&
        !addErrors.mOrganization.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mPhone.WasPassed() && !addErrors.mPhone.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mPostalCode.WasPassed() &&
        !addErrors.mPostalCode.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mRecipient.WasPassed() &&
        !addErrors.mRecipient.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mRegion.WasPassed() && !addErrors.mRegion.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mRegionCode.WasPassed() &&
        !addErrors.mRegionCode.Value().IsEmpty()) {
      return;
    }
    if (addErrors.mSortingCode.WasPassed() &&
        !addErrors.mSortingCode.Value().IsEmpty()) {
      return;
    }
  }
  aRv.ThrowAbortError("PaymentValidationErrors can not be an empty error");
}

NS_IMETHODIMP
PaymentResponse::Notify(nsITimer* timer) {
  mTimer = nullptr;

  if (!mRequest->InFullyActiveDocument()) {
    return NS_OK;
  }

  if (mCompleteCalled) {
    return NS_OK;
  }

  mCompleteCalled = true;

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  if (NS_WARN_IF(!manager)) {
    return NS_ERROR_FAILURE;
  }
  manager->CompletePayment(mRequest, PaymentComplete::Unknown, IgnoreErrors(),
                           true);
  return NS_OK;
}

nsresult PaymentResponse::UpdatePayerDetail(const nsAString& aPayerName,
                                            const nsAString& aPayerEmail,
                                            const nsAString& aPayerPhone) {
  MOZ_ASSERT(mRequest->ReadyForUpdate());
  PaymentOptions options;
  mRequest->GetOptions(options);
  if (options.mRequestPayerName) {
    mPayerName = aPayerName;
  }
  if (options.mRequestPayerEmail) {
    mPayerEmail = aPayerEmail;
  }
  if (options.mRequestPayerPhone) {
    mPayerPhone = aPayerPhone;
  }
  return DispatchUpdateEvent(u"payerdetailchange"_ns);
}

nsresult PaymentResponse::DispatchUpdateEvent(const nsAString& aType) {
  PaymentRequestUpdateEventInit init;
  RefPtr<PaymentRequestUpdateEvent> event =
      PaymentRequestUpdateEvent::Constructor(this, aType, init);
  event->SetTrusted(true);
  event->SetRequest(mRequest);

  ErrorResult rv;
  DispatchEvent(*event, rv);
  return rv.StealNSResult();
}

}  // namespace mozilla::dom
