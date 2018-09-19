/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPrefs.h"
#include "mozilla/dom/PaymentResponse.h"
#include "mozilla/dom/BasicCardPaymentBinding.h"
#include "BasicCardPayment.h"
#include "PaymentAddress.h"
#include "PaymentRequestUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PaymentResponse, mOwner,
                                      mShippingAddress, mPromise)

NS_IMPL_CYCLE_COLLECTING_ADDREF(PaymentResponse)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PaymentResponse)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PaymentResponse)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
NS_INTERFACE_MAP_END

PaymentResponse::PaymentResponse(nsPIDOMWindowInner* aWindow,
                                 PaymentRequest* aRequest,
                                 const nsAString& aRequestId,
                                 const nsAString& aMethodName,
                                 const nsAString& aShippingOption,
                                 PaymentAddress* aShippingAddress,
                                 const nsAString& aDetails,
                                 const nsAString& aPayerName,
                                 const nsAString& aPayerEmail,
                                 const nsAString& aPayerPhone)
  : mOwner(aWindow)
  , mCompleteCalled(false)
  , mRequest(aRequest)
  , mRequestId(aRequestId)
  , mMethodName(aMethodName)
  , mDetails(aDetails)
  , mShippingOption(aShippingOption)
  , mPayerName(aPayerName)
  , mPayerEmail(aPayerEmail)
  , mPayerPhone(aPayerPhone)
  , mShippingAddress(aShippingAddress)
{

  // TODO: from https://github.com/w3c/browser-payment-api/issues/480
  // Add payerGivenName + payerFamilyName to PaymentAddress
  NS_NewTimerWithCallback(getter_AddRefs(mTimer),
                          this,
                          StaticPrefs::dom_payments_response_timeout(),
                          nsITimer::TYPE_ONE_SHOT,
                          aWindow->EventTargetFor(TaskCategory::Other));
}

PaymentResponse::~PaymentResponse()
{
}

JSObject*
PaymentResponse::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return PaymentResponse_Binding::Wrap(aCx, this, aGivenProto);
}

void
PaymentResponse::GetRequestId(nsString& aRetVal) const
{
  aRetVal = mRequestId;
}

void
PaymentResponse::GetMethodName(nsString& aRetVal) const
{
  aRetVal = mMethodName;
}

void
PaymentResponse::GetDetails(JSContext* aCx, JS::MutableHandle<JSObject*> aRetVal) const
{
  RefPtr<BasicCardService> service = BasicCardService::GetService();
  MOZ_ASSERT(service);
  if (!service->IsBasicCardPayment(mMethodName)) {
    DeserializeToJSObject(mDetails, aCx, aRetVal);
  } else {
    BasicCardResponse response;
    nsresult rv = service->DecodeBasicCardData(mDetails, mOwner, response);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }

    MOZ_ASSERT(aCx);
    JS::RootedValue value(aCx);
    if (NS_WARN_IF(!response.ToObjectInternal(aCx, &value))) {
      return;
    }
    aRetVal.set(&value.toObject());
  }
}

void
PaymentResponse::GetShippingOption(nsString& aRetVal) const
{
  aRetVal = mShippingOption;
}

void
PaymentResponse::GetPayerName(nsString& aRetVal) const
{
  aRetVal = mPayerName;
}

void PaymentResponse::GetPayerEmail(nsString& aRetVal) const
{
  aRetVal = mPayerEmail;
}

void PaymentResponse::GetPayerPhone(nsString& aRetVal) const
{
  aRetVal = mPayerPhone;
}

// TODO:
// Return a raw pointer here to avoid refcounting, but make sure it's safe
// (the object should be kept alive by the callee).
already_AddRefed<PaymentAddress>
PaymentResponse::GetShippingAddress() const
{
  RefPtr<PaymentAddress> address = mShippingAddress;
  return address.forget();
}

already_AddRefed<Promise>
PaymentResponse::Complete(PaymentComplete result, ErrorResult& aRv)
{
  if (mCompleteCalled) {
    aRv.Throw(NS_ERROR_DOM_INVALID_STATE_ERR);
    return nullptr;
  }

  mCompleteCalled = true;

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  if (NS_WARN_IF(!manager)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  nsresult rv = manager->CompletePayment(mRequest, result);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsIGlobalObject* global = mOwner->AsGlobal();
  ErrorResult errResult;
  RefPtr<Promise> promise = Promise::Create(global, errResult);
  if (errResult.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  mPromise = promise;
  return promise.forget();
}

void
PaymentResponse::RespondComplete()
{
  // mPromise may be null when timing out
  if (mPromise) {
    mPromise->MaybeResolve(JS::UndefinedHandleValue);
    mPromise = nullptr;
  }
}

already_AddRefed<Promise>
PaymentResponse::Retry(JSContext* aCx,
                       const PaymentValidationErrors& aErrors,
                       ErrorResult& aRv)
{
  nsIGlobalObject* global = mOwner->AsGlobal();
  ErrorResult errResult;
  RefPtr<Promise> promise = Promise::Create(global, errResult);
  if (errResult.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }

  nsIDocument* doc = mOwner->GetExtantDoc();
  if (!doc || !doc->IsCurrentActiveDocument()) {
    promise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
    return promise.forget();
  }

  if (mCompleteCalled || mRetryPromise) {
    promise->MaybeReject(NS_ERROR_DOM_INVALID_STATE_ERR);
    return promise.forget();
  }

  nsresult rv = ValidatePaymentValidationErrors(aErrors);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  MOZ_ASSERT(mRequest);
  rv = mRequest->RetryPayment(aCx, aErrors);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    promise->MaybeReject(rv);
    return promise.forget();
  }

  mRetryPromise = promise;
  return promise.forget();
}

void
PaymentResponse::RespondRetry(const nsAString& aMethodName,
                              const nsAString& aShippingOption,
                              PaymentAddress* aShippingAddress,
                              const nsAString& aDetails,
                              const nsAString& aPayerName,
                              const nsAString& aPayerEmail,
                              const nsAString& aPayerPhone)
{
  mMethodName = aMethodName;
  mShippingOption = aShippingOption;
  mShippingAddress = aShippingAddress;
  mDetails = aDetails;
  mPayerName = aPayerName;
  mPayerEmail = aPayerEmail;
  mPayerPhone = aPayerPhone;

  NS_NewTimerWithCallback(getter_AddRefs(mTimer),
                          this,
                          StaticPrefs::dom_payments_response_timeout(),
                          nsITimer::TYPE_ONE_SHOT,
                          mOwner->EventTargetFor(TaskCategory::Other));
  MOZ_ASSERT(mRetryPromise);
  mRetryPromise->MaybeResolve(JS::UndefinedHandleValue);
  mRetryPromise = nullptr;
}

void
PaymentResponse::RejectRetry(nsresult aRejectReason)
{
  MOZ_ASSERT(mRetryPromise);
  mRetryPromise->MaybeReject(aRejectReason);
  mRetryPromise = nullptr;
}

nsresult
PaymentResponse::ValidatePaymentValidationErrors(const PaymentValidationErrors& aErrors)
{
  // Should not be empty errors
  // check PaymentValidationErrors.error
  if (aErrors.mError.WasPassed() && !aErrors.mError.Value().IsEmpty()) {
    return NS_OK;
  }
  // check PaymentValidationErrors.payer
  PayerErrorFields payerErrors(aErrors.mPayer);
  if (payerErrors.mName.WasPassed() && !payerErrors.mName.Value().IsEmpty()) {
    return NS_OK;
  }
  if (payerErrors.mEmail.WasPassed() && !payerErrors.mEmail.Value().IsEmpty()) {
    return NS_OK;
  }
  if (payerErrors.mPhone.WasPassed() && !payerErrors.mPhone.Value().IsEmpty()) {
    return NS_OK;
  }
  // check PaymentValidationErrors.paymentMethod
  if (aErrors.mPaymentMethod.WasPassed()) {
    return NS_OK;
  }
  // check PaymentValidationErrors.shippingAddress
  AddressErrors addErrors(aErrors.mShippingAddress);
  if (addErrors.mAddressLine.WasPassed() &&
      !addErrors.mAddressLine.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mCity.WasPassed() && !addErrors.mCity.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mCountry.WasPassed() && !addErrors.mCountry.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mDependentLocality.WasPassed() &&
      !addErrors.mDependentLocality.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mOrganization.WasPassed() &&
      !addErrors.mOrganization.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mPhone.WasPassed() && !addErrors.mPhone.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mPostalCode.WasPassed() &&
      !addErrors.mPostalCode.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mRecipient.WasPassed() &&
      !addErrors.mRecipient.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mRegion.WasPassed() &&
      !addErrors.mRegion.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mRegionCode.WasPassed() &&
      !addErrors.mRegionCode.Value().IsEmpty()) {
    return NS_OK;
  }
  if (addErrors.mSortingCode.WasPassed() &&
      !addErrors.mSortingCode.Value().IsEmpty()) {
    return NS_OK;
  }
  return NS_ERROR_DOM_ABORT_ERR;
}

NS_IMETHODIMP
PaymentResponse::Notify(nsITimer *timer)
{
  mTimer = nullptr;
  if (mCompleteCalled) {
    return NS_OK;
  }

  mCompleteCalled = true;

  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  if (NS_WARN_IF(!manager)) {
    return NS_ERROR_FAILURE;
  }

  return manager->CompletePayment(mRequest, PaymentComplete::Unknown, true);
}

} // namespace dom
} // namespace mozilla
