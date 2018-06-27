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
                                 RefPtr<PaymentAddress> aShippingAddress,
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
