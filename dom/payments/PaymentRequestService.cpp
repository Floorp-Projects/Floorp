/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "PaymentRequestData.h"
#include "PaymentRequestService.h"

namespace mozilla {
namespace dom {

StaticRefPtr<PaymentRequestService> gPaymentService;

namespace {

class PaymentRequestEnumerator final : public nsISimpleEnumerator
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

  PaymentRequestEnumerator()
    : mIndex(0)
  {}
private:
  ~PaymentRequestEnumerator() = default;
  uint32_t mIndex;
};

NS_IMPL_ISUPPORTS(PaymentRequestEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
PaymentRequestEnumerator::HasMoreElements(bool* aReturn)
{
  NS_ENSURE_ARG_POINTER(aReturn);
  *aReturn = false;
  if (NS_WARN_IF(!gPaymentService)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<PaymentRequestService> service = gPaymentService;
  *aReturn = mIndex < service->NumPayments();
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestEnumerator::GetNext(nsISupports** aItem)
{
  NS_ENSURE_ARG_POINTER(aItem);
  if (NS_WARN_IF(!gPaymentService)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIPaymentRequest> request =
    gPaymentService->GetPaymentRequestByIndex(mIndex);
  if (NS_WARN_IF(!request)) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsISupports> item = do_QueryInterface(request);
  if (NS_WARN_IF(!item)) {
    return NS_ERROR_FAILURE;
  }
  mIndex++;
  item.forget(aItem);
  return NS_OK;
}

} // end of anonymous namespace

/* PaymentRequestService */

NS_IMPL_ISUPPORTS(PaymentRequestService,
                  nsIPaymentRequestService)

already_AddRefed<PaymentRequestService>
PaymentRequestService::GetSingleton()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!gPaymentService) {
    gPaymentService = new PaymentRequestService();
    ClearOnShutdown(&gPaymentService);
  }
  RefPtr<PaymentRequestService> service = gPaymentService;
  return service.forget();
}

uint32_t
PaymentRequestService::NumPayments() const
{
  return mRequestQueue.Length();
}

already_AddRefed<nsIPaymentRequest>
PaymentRequestService::GetPaymentRequestByIndex(const uint32_t aIndex)
{
  if (aIndex >= mRequestQueue.Length()) {
    return nullptr;
  }
  nsCOMPtr<nsIPaymentRequest> request = mRequestQueue[aIndex];
  MOZ_ASSERT(request);
  return request.forget();
}

NS_IMETHODIMP
PaymentRequestService::GetPaymentRequestById(const nsAString& aRequestId,
                                             nsIPaymentRequest** aRequest)
{
  NS_ENSURE_ARG_POINTER(aRequest);
  *aRequest = nullptr;
  uint32_t numRequests = mRequestQueue.Length();
  for (uint32_t index = 0; index < numRequests; ++index) {
    nsCOMPtr<nsIPaymentRequest> request = mRequestQueue[index];
    MOZ_ASSERT(request);
    nsAutoString requestId;
    nsresult rv = request->GetRequestId(requestId);
    NS_ENSURE_SUCCESS(rv, rv);
    if (requestId == aRequestId) {
      request.forget(aRequest);
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::Enumerate(nsISimpleEnumerator** aEnumerator)
{
  NS_ENSURE_ARG_POINTER(aEnumerator);
  nsCOMPtr<nsISimpleEnumerator> enumerator = new PaymentRequestEnumerator();
  enumerator.forget(aEnumerator);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::Cleanup()
{
  mRequestQueue.Clear();
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::SetTestingUIService(nsIPaymentUIService* aUIService)
{
  // aUIService can be nullptr
  mTestingUIService = aUIService;
  return NS_OK;
}

nsresult
PaymentRequestService::CallTestingUIAction(const nsAString& aRequestId, uint32_t aActionType)
{
  nsCOMPtr<nsIPaymentActionResponse> response;
  nsresult rv;
  if (mTestingUIService) {
    switch (aActionType) {
      case nsIPaymentActionRequest::CANMAKE_ACTION: {
        rv = mTestingUIService->CanMakePayment(aRequestId, getter_AddRefs(response));
        break;
      }
      case nsIPaymentActionRequest::SHOW_ACTION: {
        rv = mTestingUIService->ShowPayment(aRequestId, getter_AddRefs(response));
        break;
      }
      case nsIPaymentActionRequest::ABORT_ACTION: {
        rv = mTestingUIService->AbortPayment(aRequestId, getter_AddRefs(response));
        break;
      }
      case nsIPaymentActionRequest::COMPLETE_ACTION: {
        rv = mTestingUIService->CompletePayment(aRequestId, getter_AddRefs(response));
        break;
      }
      case nsIPaymentActionRequest::UPDATE_ACTION: {
        rv = mTestingUIService->UpdatePayment(aRequestId, getter_AddRefs(response));
        break;
      }
      default : {
        return NS_ERROR_FAILURE;
      }
    }
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    // Since there is no UI implementation and no testing UI Service is registered,
    // set false response for canMakePayment(), ABORT_SUCCEEDED for abort() and
    // COMPLETE_SUCCEEDED for complete().
    switch (aActionType) {
      case nsIPaymentActionRequest::CANMAKE_ACTION: {
        nsCOMPtr<nsIPaymentCanMakeActionResponse> canMakeResponse =
          do_CreateInstance(NS_PAYMENT_CANMAKE_ACTION_RESPONSE_CONTRACT_ID);
        MOZ_ASSERT(canMakeResponse);
        rv = canMakeResponse->Init(aRequestId, false);
        NS_ENSURE_SUCCESS(rv, rv);
        response = do_QueryInterface(canMakeResponse);
        MOZ_ASSERT(response);
        break;
      }
      case nsIPaymentActionRequest::ABORT_ACTION: {
        nsCOMPtr<nsIPaymentAbortActionResponse> abortResponse =
          do_CreateInstance(NS_PAYMENT_ABORT_ACTION_RESPONSE_CONTRACT_ID);
        MOZ_ASSERT(abortResponse);
        rv = abortResponse->Init(aRequestId, nsIPaymentActionResponse::ABORT_SUCCEEDED);
        NS_ENSURE_SUCCESS(rv, rv);
        response = do_QueryInterface(abortResponse);
        MOZ_ASSERT(response);
        break;
      }
      case nsIPaymentActionRequest::COMPLETE_ACTION: {
        nsCOMPtr<nsIPaymentCompleteActionResponse> completeResponse =
          do_CreateInstance(NS_PAYMENT_COMPLETE_ACTION_RESPONSE_CONTRACT_ID);
        MOZ_ASSERT(completeResponse);
        rv = completeResponse->Init(aRequestId, nsIPaymentActionResponse::COMPLETE_SUCCEEDED);
        NS_ENSURE_SUCCESS(rv, rv);
        response = do_QueryInterface(completeResponse);
        MOZ_ASSERT(response);
        break;
      }
      default : {
        break;
      }
    }
  }
  if (response) {
    rv = RespondPayment(response);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::RemoveActionCallback(nsIPaymentActionCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);
  for (auto iter = mCallbackHashtable.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtr<nsIPaymentActionCallback> callback = iter.Data();
    MOZ_ASSERT(callback);
    if (callback == aCallback) {
      iter.Remove();
      return NS_OK;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::RequestPayment(nsIPaymentActionRequest* aRequest)
{
  NS_ENSURE_ARG_POINTER(aRequest);

  nsAutoString requestId;
  nsresult rv = aRequest->GetRequestId(requestId);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCOMPtr<nsIPaymentActionCallback> callback;
  rv = aRequest->GetCallback(getter_AddRefs(callback));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rv = SetActionCallback(requestId, callback);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  uint32_t type;
  rv = aRequest->GetType(&type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  switch (type) {
    case nsIPaymentActionRequest::CREATE_ACTION: {
      nsCOMPtr<nsIPaymentCreateActionRequest> request =
        do_QueryInterface(aRequest);
      MOZ_ASSERT(request);
      uint64_t tabId;
      rv = request->GetTabId(&tabId);
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIArray> methodData;
      rv = request->GetMethodData(getter_AddRefs(methodData));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIPaymentDetails> details;
      rv = request->GetDetails(getter_AddRefs(details));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIPaymentOptions> options;
      rv = request->GetOptions(getter_AddRefs(options));
      NS_ENSURE_SUCCESS(rv, rv);

      nsCOMPtr<nsIPaymentRequest> payment =
        new payments::PaymentRequest(tabId, requestId, methodData, details, options);

      if (!mRequestQueue.AppendElement(payment, mozilla::fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      break;
    }
    /*
     *  TODO: 1. Check basic card support once the Basic Card Payment spec is
     *           implemented.
     *           https://www.w3.org/TR/payment-method-basic-card/
     *        2. Check third party payment app support by traversing all
     *           registered third party payment apps.
     */
    case nsIPaymentActionRequest::CANMAKE_ACTION:
    /*
     *  TODO: Launch/inform payment UI here once the UI module is implemented.
     */
    case nsIPaymentActionRequest::SHOW_ACTION:
    case nsIPaymentActionRequest::ABORT_ACTION:
    case nsIPaymentActionRequest::COMPLETE_ACTION: {
      rv = CallTestingUIAction(requestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_ERROR_FAILURE;
      }
      break;
    }
    case nsIPaymentActionRequest::UPDATE_ACTION: {
      nsCOMPtr<nsIPaymentUpdateActionRequest> request = do_QueryInterface(aRequest);
      MOZ_ASSERT(request);

      nsCOMPtr<nsIPaymentDetails> details;
      rv = request->GetDetails(getter_AddRefs(details));
      NS_ENSURE_SUCCESS(rv, rv);

      rv = request->GetRequestId(requestId);
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIPaymentRequest> payment;
      rv = GetPaymentRequestById(requestId, getter_AddRefs(payment));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      rv = payment->UpdatePaymentDetails(details);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      rv = CallTestingUIAction(requestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return NS_ERROR_FAILURE;
      }
      break;
    }
    default: {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::RespondPayment(nsIPaymentActionResponse* aResponse)
{
  NS_ENSURE_ARG_POINTER(aResponse);
  nsAutoString requestId;
  nsresult rv = aResponse->GetRequestId(requestId);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIPaymentRequest> request;
  rv = GetPaymentRequestById(requestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIPaymentActionCallback> callback;
  if (!mCallbackHashtable.Get(requestId, getter_AddRefs(callback))) {
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(!callback)) {
    return NS_ERROR_FAILURE;
  }
  rv = callback->RespondPayment(aResponse);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Remove nsIPaymentRequest from mRequestQueue while receive succeeded abort
  // response or complete response
  uint32_t type;
  rv = aResponse->GetType(&type);
  NS_ENSURE_SUCCESS(rv, rv);
  switch (type) {
    case nsIPaymentActionResponse::ABORT_ACTION: {
      nsCOMPtr<nsIPaymentAbortActionResponse> response =
        do_QueryInterface(aResponse);
      MOZ_ASSERT(response);
      bool isSucceeded;
      rv = response->IsSucceeded(&isSucceeded);
      NS_ENSURE_SUCCESS(rv, rv);
      if (isSucceeded) {
        mRequestQueue.RemoveElement(request);
      }
      break;
    }
    case nsIPaymentActionResponse::COMPLETE_ACTION: {
      mRequestQueue.RemoveElement(request);
      break;
    }
    default: {
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::ChangeShippingAddress(const nsAString& aRequestId,
                                             nsIPaymentAddress* aAddress)
{
  nsCOMPtr<nsIPaymentActionCallback> callback;
  if (!mCallbackHashtable.Get(aRequestId, getter_AddRefs(callback))) {
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(!callback)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = callback->ChangeShippingAddress(aRequestId, aAddress);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::ChangeShippingOption(const nsAString& aRequestId,
                                            const nsAString& aOption)
{
  nsCOMPtr<nsIPaymentActionCallback> callback;
  if (!mCallbackHashtable.Get(aRequestId, getter_AddRefs(callback))) {
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(!callback)) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = callback->ChangeShippingOption(aRequestId, aOption);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
PaymentRequestService::SetActionCallback(const nsAString& aRequestId,
                                         nsIPaymentActionCallback* aCallback)
{
  NS_ENSURE_ARG_POINTER(aCallback);
  nsCOMPtr<nsIPaymentActionCallback> callback;
  if (mCallbackHashtable.Get(aRequestId, getter_AddRefs(callback))) {
    mCallbackHashtable.Remove(aRequestId);
  }
  mCallbackHashtable.Put(aRequestId, aCallback);
  return NS_OK;
}

nsresult
PaymentRequestService::RemoveActionCallback(const nsAString& aRequestId)
{
  nsCOMPtr<nsIPaymentActionCallback> callback;
  if (!mCallbackHashtable.Get(aRequestId, getter_AddRefs(callback))) {
    return NS_ERROR_FAILURE;
  }
  mCallbackHashtable.Remove(aRequestId);
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
