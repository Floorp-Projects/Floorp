/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/PaymentRequestParent.h"
#include "PaymentRequestData.h"
#include "PaymentRequestService.h"
#include "BasicCardPayment.h"
#include "nsSimpleEnumerator.h"

namespace mozilla {
namespace dom {

StaticRefPtr<PaymentRequestService> gPaymentService;

namespace {

class PaymentRequestEnumerator final : public nsSimpleEnumerator
{
public:
  NS_DECL_NSISIMPLEENUMERATOR

  PaymentRequestEnumerator()
    : mIndex(0)
  {}

  const nsID& DefaultInterface() override
  {
    return NS_GET_IID(nsIPaymentRequest);
  }

private:
  ~PaymentRequestEnumerator() override = default;
  uint32_t mIndex;
};

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
  mIndex++;
  request.forget(aItem);
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
PaymentRequestService::LaunchUIAction(const nsAString& aRequestId, uint32_t aActionType)
{
  nsCOMPtr<nsIPaymentUIService> uiService;
  nsresult rv;
  if (mTestingUIService) {
    uiService = mTestingUIService;
  } else {
    uiService = do_GetService(NS_PAYMENT_UI_SERVICE_CONTRACT_ID, &rv);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  switch (aActionType) {
    case IPCPaymentActionRequest::TIPCPaymentShowActionRequest:{
      rv = uiService->ShowPayment(aRequestId);
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentAbortActionRequest: {
      rv = uiService->AbortPayment(aRequestId);
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCompleteActionRequest: {
      rv = uiService->CompletePayment(aRequestId);
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentUpdateActionRequest: {
      rv = uiService->UpdatePayment(aRequestId);
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCloseActionRequest: {
      rv = uiService->ClosePayment(aRequestId);
      break;
    }
    default : {
      return NS_ERROR_FAILURE;
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult
PaymentRequestService::RequestPayment(const nsAString& aRequestId,
                                      const IPCPaymentActionRequest& aAction,
                                      PaymentRequestParent* aIPC)
{
  NS_ENSURE_ARG_POINTER(aIPC);

  nsresult rv = NS_OK;
  uint32_t type = aAction.type();

  if (type != IPCPaymentActionRequest::TIPCPaymentCreateActionRequest) {
    nsCOMPtr<nsIPaymentRequest> request;
    rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (!request && type != IPCPaymentActionRequest::TIPCPaymentCloseActionRequest) {
      return NS_ERROR_FAILURE;
    }
    if (request) {
      payments::PaymentRequest* rowRequest =
        static_cast<payments::PaymentRequest*>(request.get());
      if (!rowRequest) {
        return NS_ERROR_FAILURE;
      }
      rowRequest->SetIPC(aIPC);
    }
  }

  switch (type) {
    case IPCPaymentActionRequest::TIPCPaymentCreateActionRequest: {
      const IPCPaymentCreateActionRequest& action = aAction;
      uint64_t tabId = aIPC->GetTabId();
      nsCOMPtr<nsIMutableArray> methodData = do_CreateInstance(NS_ARRAY_CONTRACTID);
      MOZ_ASSERT(methodData);
      for (IPCPaymentMethodData data : action.methodData()) {
        nsCOMPtr<nsIPaymentMethodData> method;
        rv = payments::PaymentMethodData::Create(data, getter_AddRefs(method));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = methodData->AppendElement(method);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      nsCOMPtr<nsIPaymentDetails> details;
      rv = payments::PaymentDetails::Create(action.details(), getter_AddRefs(details));
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIPaymentOptions> options;
      rv = payments::PaymentOptions::Create(action.options(), getter_AddRefs(options));
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIPaymentRequest> payment =
        new payments::PaymentRequest(tabId,
                                     aRequestId,
                                     action.topLevelPrincipal(),
                                     methodData,
                                     details,
                                     options,
                                     action.shippingOption());

      if (!mRequestQueue.AppendElement(payment, mozilla::fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCanMakeActionRequest: {
      nsCOMPtr<nsIPaymentCanMakeActionResponse> canMakeResponse =
        do_CreateInstance(NS_PAYMENT_CANMAKE_ACTION_RESPONSE_CONTRACT_ID);
      MOZ_ASSERT(canMakeResponse);
      rv = canMakeResponse->Init(aRequestId,
                                 CanMakePayment(aRequestId));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      rv = RespondPayment(canMakeResponse.get());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentShowActionRequest: {
      if (mShowingRequest || !CanMakePayment(aRequestId)) {
        uint32_t responseStatus;
        if (mShowingRequest) {
          responseStatus = nsIPaymentActionResponse::PAYMENT_REJECTED;
        } else {
          responseStatus = nsIPaymentActionResponse::PAYMENT_NOTSUPPORTED;
        }
        nsCOMPtr<nsIPaymentShowActionResponse> showResponse =
          do_CreateInstance(NS_PAYMENT_SHOW_ACTION_RESPONSE_CONTRACT_ID);
        MOZ_ASSERT(showResponse);
        rv = showResponse->Init(aRequestId,
                                responseStatus,
                                EmptyString(),
                                nullptr,
                                EmptyString(),
                                EmptyString(),
                                EmptyString());
        rv = RespondPayment(showResponse.get());
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      } else {
        rv = GetPaymentRequestById(aRequestId,
                                   getter_AddRefs(mShowingRequest));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
        rv = LaunchUIAction(aRequestId, type);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentAbortActionRequest: {
      rv = LaunchUIAction(aRequestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCompleteActionRequest: {
      const IPCPaymentCompleteActionRequest& action = aAction;
      nsCOMPtr<nsIPaymentRequest> payment;
      rv = GetPaymentRequestById(aRequestId, getter_AddRefs(payment));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      MOZ_ASSERT(payment);
      payments::PaymentRequest* rowPayment =
        static_cast<payments::PaymentRequest*>(payment.get());
      MOZ_ASSERT(rowPayment);
      rowPayment->SetCompleteStatus(action.completeStatus());
      rv = LaunchUIAction(aRequestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentUpdateActionRequest: {
      const IPCPaymentUpdateActionRequest& action = aAction;
      nsCOMPtr<nsIPaymentDetails> details;
      rv = payments::PaymentDetails::Create(action.details(), getter_AddRefs(details));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      nsCOMPtr<nsIPaymentRequest> payment;
      rv = GetPaymentRequestById(aRequestId, getter_AddRefs(payment));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      MOZ_ASSERT(payment);
      payments::PaymentRequest* rowPayment =
        static_cast<payments::PaymentRequest*>(payment.get());
      MOZ_ASSERT(rowPayment);
      rv = rowPayment->UpdatePaymentDetails(details, action.shippingOption());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (mShowingRequest) {
        MOZ_ASSERT(mShowingRequest == payment);
        rv = LaunchUIAction(aRequestId, type);
      } else {
        mShowingRequest = payment;
        rv = LaunchUIAction(aRequestId,
                            IPCPaymentActionRequest::TIPCPaymentShowActionRequest);
      }
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCloseActionRequest: {
      nsCOMPtr<nsIPaymentRequest> payment;
      rv = GetPaymentRequestById(aRequestId, getter_AddRefs(payment));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      rv = LaunchUIAction(aRequestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (mShowingRequest == payment) {
        mShowingRequest = nullptr;
      }
      mRequestQueue.RemoveElement(payment);
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentRetryActionRequest: {
      const IPCPaymentRetryActionRequest& action = aAction;
      nsCOMPtr<nsIPaymentRequest> payment;
      rv = GetPaymentRequestById(aRequestId, getter_AddRefs(payment));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      MOZ_ASSERT(payment);
      payments::PaymentRequest* rowPayment =
        static_cast<payments::PaymentRequest*>(payment.get());
      MOZ_ASSERT(rowPayment);
      rowPayment->UpdateErrors(action.error(),
                               action.payerErrors(),
                               action.paymentMethodErrors(),
                               action.shippingAddressErrors());
      MOZ_ASSERT(mShowingRequest == payment);
      rv = LaunchUIAction(aRequestId,
                          IPCPaymentActionRequest::TIPCPaymentUpdateActionRequest);
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
  if (!request) {
    return NS_ERROR_FAILURE;
  }
  payments::PaymentRequest* rowRequest =
    static_cast<payments::PaymentRequest*>(request.get());
  if (!rowRequest) {
    return NS_ERROR_FAILURE;
  }
  if (!rowRequest->GetIPC()) {
    return NS_ERROR_FAILURE;
  }
  rv = rowRequest->GetIPC()->RespondPayment(aResponse);
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
      mShowingRequest = nullptr;
      if (isSucceeded) {
        mRequestQueue.RemoveElement(request);
      }
      break;
    }
    case nsIPaymentActionResponse::SHOW_ACTION: {
      nsCOMPtr<nsIPaymentShowActionResponse> response =
        do_QueryInterface(aResponse);
      MOZ_ASSERT(response);
      uint32_t acceptStatus;
      rv = response->GetAcceptStatus(&acceptStatus);
      NS_ENSURE_SUCCESS(rv, rv);
      if (acceptStatus != nsIPaymentActionResponse::PAYMENT_ACCEPTED) {
        mShowingRequest = nullptr;
        mRequestQueue.RemoveElement(request);
      }
      break;
    }
    case nsIPaymentActionResponse::COMPLETE_ACTION: {
      mShowingRequest = nullptr;
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
  nsCOMPtr<nsIPaymentRequest> request;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!request) {
    return NS_ERROR_FAILURE;
  }
  payments::PaymentRequest* rowRequest =
    static_cast<payments::PaymentRequest*>(request.get());
  if (!rowRequest) {
    return NS_ERROR_FAILURE;
  }
  if (!rowRequest->GetIPC()) {
    return NS_ERROR_FAILURE;
  }
  rv = rowRequest->GetIPC()->ChangeShippingAddress(aRequestId, aAddress);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::ChangeShippingOption(const nsAString& aRequestId,
                                            const nsAString& aOption)
{
  nsCOMPtr<nsIPaymentRequest> request;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!request) {
    return NS_ERROR_FAILURE;
  }
  payments::PaymentRequest* rowRequest =
    static_cast<payments::PaymentRequest*>(request.get());
  if (!rowRequest) {
    return NS_ERROR_FAILURE;
  }
  if (!rowRequest->GetIPC()) {
    return NS_ERROR_FAILURE;
  }
  rv = rowRequest->GetIPC()->ChangeShippingOption(aRequestId, aOption);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

bool
PaymentRequestService::CanMakePayment(const nsAString& aRequestId)
{
  /*
   *  TODO: Check third party payment app support by traversing all
   *        registered third party payment apps.
   */
  return IsBasicCardPayment(aRequestId);
}

bool
PaymentRequestService::IsBasicCardPayment(const nsAString& aRequestId)
{
  nsCOMPtr<nsIPaymentRequest> payment;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(payment));
  NS_ENSURE_SUCCESS(rv, false);
  nsCOMPtr<nsIArray> methods;
  rv = payment->GetPaymentMethods(getter_AddRefs(methods));
  NS_ENSURE_SUCCESS(rv, false);
  uint32_t length;
  rv = methods->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, false);
  RefPtr<BasicCardService> service = BasicCardService::GetService();
  MOZ_ASSERT(service);
  for (uint32_t index = 0; index < length; ++index) {
    nsCOMPtr<nsIPaymentMethodData> method = do_QueryElementAt(methods, index);
    MOZ_ASSERT(method);
    nsAutoString supportedMethods;
    rv = method->GetSupportedMethods(supportedMethods);
    NS_ENSURE_SUCCESS(rv, false);
    if (service->IsBasicCardPayment(supportedMethods)) {
      return true;
    }
  }
  return false;
}

} // end of namespace dom
} // end of namespace mozilla
