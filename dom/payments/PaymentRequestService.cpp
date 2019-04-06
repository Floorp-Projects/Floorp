/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCardPayment.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/BasicCardPaymentBinding.h"
#include "mozilla/dom/PaymentRequestParent.h"
#include "nsSimpleEnumerator.h"
#include "PaymentRequestService.h"

namespace mozilla {
namespace dom {

StaticRefPtr<PaymentRequestService> gPaymentService;

namespace {

class PaymentRequestEnumerator final : public nsSimpleEnumerator {
 public:
  NS_DECL_NSISIMPLEENUMERATOR

  PaymentRequestEnumerator() : mIndex(0) {}

  const nsID& DefaultInterface() override {
    return NS_GET_IID(nsIPaymentRequest);
  }

 private:
  ~PaymentRequestEnumerator() override = default;
  uint32_t mIndex;
};

NS_IMETHODIMP
PaymentRequestEnumerator::HasMoreElements(bool* aReturn) {
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
PaymentRequestEnumerator::GetNext(nsISupports** aItem) {
  NS_ENSURE_ARG_POINTER(aItem);
  if (NS_WARN_IF(!gPaymentService)) {
    return NS_ERROR_FAILURE;
  }
  RefPtr<payments::PaymentRequest> rowRequest =
      gPaymentService->GetPaymentRequestByIndex(mIndex);
  if (!rowRequest) {
    return NS_ERROR_FAILURE;
  }
  mIndex++;
  rowRequest.forget(aItem);
  return NS_OK;
}

}  // end of anonymous namespace

/* PaymentRequestService */

NS_IMPL_ISUPPORTS(PaymentRequestService, nsIPaymentRequestService)

already_AddRefed<PaymentRequestService> PaymentRequestService::GetSingleton() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!gPaymentService) {
    gPaymentService = new PaymentRequestService();
    ClearOnShutdown(&gPaymentService);
  }
  RefPtr<PaymentRequestService> service = gPaymentService;
  return service.forget();
}

uint32_t PaymentRequestService::NumPayments() const {
  return mRequestQueue.Length();
}

already_AddRefed<payments::PaymentRequest>
PaymentRequestService::GetPaymentRequestByIndex(const uint32_t aIndex) {
  if (aIndex >= mRequestQueue.Length()) {
    return nullptr;
  }
  RefPtr<payments::PaymentRequest> request = mRequestQueue[aIndex];
  MOZ_ASSERT(request);
  return request.forget();
}

NS_IMETHODIMP
PaymentRequestService::GetPaymentRequestById(const nsAString& aRequestId,
                                             nsIPaymentRequest** aRequest) {
  NS_ENSURE_ARG_POINTER(aRequest);
  *aRequest = nullptr;
  RefPtr<payments::PaymentRequest> rowRequest;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(rowRequest));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  rowRequest.forget(aRequest);
  return NS_OK;
}

nsresult PaymentRequestService::GetPaymentRequestById(
    const nsAString& aRequestId, payments::PaymentRequest** aRequest) {
  NS_ENSURE_ARG_POINTER(aRequest);
  *aRequest = nullptr;
  uint32_t numRequests = mRequestQueue.Length();
  for (uint32_t index = 0; index < numRequests; ++index) {
    RefPtr<payments::PaymentRequest> request = mRequestQueue[index];
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
PaymentRequestService::Enumerate(nsISimpleEnumerator** aEnumerator) {
  NS_ENSURE_ARG_POINTER(aEnumerator);
  nsCOMPtr<nsISimpleEnumerator> enumerator = new PaymentRequestEnumerator();
  enumerator.forget(aEnumerator);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::Cleanup() {
  mRequestQueue.Clear();
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::SetTestingUIService(nsIPaymentUIService* aUIService) {
  // aUIService can be nullptr
  mTestingUIService = aUIService;
  return NS_OK;
}

nsresult PaymentRequestService::LaunchUIAction(const nsAString& aRequestId,
                                               uint32_t aActionType) {
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
    case IPCPaymentActionRequest::TIPCPaymentShowActionRequest: {
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
    default: {
      return NS_ERROR_FAILURE;
    }
  }
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult PaymentRequestService::RequestPayment(
    const nsAString& aRequestId, const IPCPaymentActionRequest& aAction,
    PaymentRequestParent* aIPC) {
  NS_ENSURE_ARG_POINTER(aIPC);

  nsresult rv = NS_OK;
  uint32_t type = aAction.type();

  RefPtr<payments::PaymentRequest> request;
  if (type != IPCPaymentActionRequest::TIPCPaymentCreateActionRequest) {
    rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    if (!request &&
        type != IPCPaymentActionRequest::TIPCPaymentCloseActionRequest) {
      return NS_ERROR_FAILURE;
    }
    if (request) {
      request->SetIPC(aIPC);
    }
  }

  switch (type) {
    case IPCPaymentActionRequest::TIPCPaymentCreateActionRequest: {
      MOZ_ASSERT(!request);
      const IPCPaymentCreateActionRequest& action = aAction;
      nsCOMPtr<nsIMutableArray> methodData =
          do_CreateInstance(NS_ARRAY_CONTRACTID);
      MOZ_ASSERT(methodData);
      for (IPCPaymentMethodData data : action.methodData()) {
        nsCOMPtr<nsIPaymentMethodData> method;
        rv = payments::PaymentMethodData::Create(data, getter_AddRefs(method));
        NS_ENSURE_SUCCESS(rv, rv);
        rv = methodData->AppendElement(method);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      nsCOMPtr<nsIPaymentDetails> details;
      rv = payments::PaymentDetails::Create(action.details(),
                                            getter_AddRefs(details));
      NS_ENSURE_SUCCESS(rv, rv);
      nsCOMPtr<nsIPaymentOptions> options;
      rv = payments::PaymentOptions::Create(action.options(),
                                            getter_AddRefs(options));
      NS_ENSURE_SUCCESS(rv, rv);
      RefPtr<payments::PaymentRequest> request = new payments::PaymentRequest(
          action.topOuterWindowId(), aRequestId, action.topLevelPrincipal(),
          methodData, details, options, action.shippingOption());

      if (!mRequestQueue.AppendElement(request, mozilla::fallible)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCanMakeActionRequest: {
      nsCOMPtr<nsIPaymentCanMakeActionResponse> canMakeResponse =
          do_CreateInstance(NS_PAYMENT_CANMAKE_ACTION_RESPONSE_CONTRACT_ID);
      MOZ_ASSERT(canMakeResponse);
      rv = canMakeResponse->Init(aRequestId, CanMakePayment(aRequestId));
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
      const IPCPaymentShowActionRequest& action = aAction;
      rv = ShowPayment(aRequestId, action.isUpdating());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentAbortActionRequest: {
      MOZ_ASSERT(request);
      request->SetState(payments::PaymentRequest::eInteractive);
      rv = LaunchUIAction(aRequestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCompleteActionRequest: {
      MOZ_ASSERT(request);
      const IPCPaymentCompleteActionRequest& action = aAction;
      request->SetCompleteStatus(action.completeStatus());
      rv = LaunchUIAction(aRequestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentUpdateActionRequest: {
      const IPCPaymentUpdateActionRequest& action = aAction;
      nsCOMPtr<nsIPaymentDetails> details;
      rv = payments::PaymentDetails::Create(action.details(),
                                            getter_AddRefs(details));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      MOZ_ASSERT(request);
      rv = request->UpdatePaymentDetails(details, action.shippingOption());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      nsAutoString completeStatus;
      rv = request->GetCompleteStatus(completeStatus);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (completeStatus.Equals(NS_LITERAL_STRING("initial"))) {
        request->SetCompleteStatus(EmptyString());
      }
      MOZ_ASSERT(mShowingRequest && mShowingRequest == request);
      rv = LaunchUIAction(aRequestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCloseActionRequest: {
      rv = LaunchUIAction(aRequestId, type);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      if (mShowingRequest == request) {
        mShowingRequest = nullptr;
      }
      mRequestQueue.RemoveElement(request);
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentRetryActionRequest: {
      const IPCPaymentRetryActionRequest& action = aAction;
      MOZ_ASSERT(request);
      request->UpdateErrors(action.error(), action.payerErrors(),
                            action.paymentMethodErrors(),
                            action.shippingAddressErrors());
      request->SetState(payments::PaymentRequest::eInteractive);
      MOZ_ASSERT(mShowingRequest == request);
      rv = LaunchUIAction(
          aRequestId, IPCPaymentActionRequest::TIPCPaymentUpdateActionRequest);
      break;
    }
    default: {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::RespondPayment(nsIPaymentActionResponse* aResponse) {
  NS_ENSURE_ARG_POINTER(aResponse);
  nsAutoString requestId;
  nsresult rv = aResponse->GetRequestId(requestId);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<payments::PaymentRequest> request;
  rv = GetPaymentRequestById(requestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!request) {
    return NS_ERROR_FAILURE;
  }
  uint32_t type;
  rv = aResponse->GetType(&type);
  NS_ENSURE_SUCCESS(rv, rv);

  // PaymentRequest can only be responded when
  // 1. the state is eInteractive
  // 2. the state is eClosed and response type is COMPLETE_ACTION
  // 3. the state is eCreated and response type is CANMAKE_ACTION
  payments::PaymentRequest::eState state = request->GetState();
  bool canBeResponded = (state == payments::PaymentRequest::eInteractive) ||
                        (state == payments::PaymentRequest::eClosed &&
                         type == nsIPaymentActionResponse::COMPLETE_ACTION) ||
                        (state == payments::PaymentRequest::eCreated &&
                         type == nsIPaymentActionResponse::CANMAKE_ACTION);
  if (!canBeResponded) {
    return NS_ERROR_FAILURE;
  }

  if (!request->GetIPC()) {
    return NS_ERROR_FAILURE;
  }
  rv = request->GetIPC()->RespondPayment(aResponse);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Remove PaymentRequest from mRequestQueue while receive succeeded abort
  // response or complete response
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
        request->SetState(payments::PaymentRequest::eClosed);
      }
      break;
    }
    case nsIPaymentActionResponse::SHOW_ACTION: {
      request->SetState(payments::PaymentRequest::eClosed);
      nsCOMPtr<nsIPaymentShowActionResponse> response =
          do_QueryInterface(aResponse);
      MOZ_ASSERT(response);
      uint32_t acceptStatus;
      rv = response->GetAcceptStatus(&acceptStatus);
      NS_ENSURE_SUCCESS(rv, rv);
      if (acceptStatus != nsIPaymentActionResponse::PAYMENT_ACCEPTED) {
        // Check if rejecting the showing PaymentRequest.
        // If yes, set mShowingRequest as nullptr.
        if (mShowingRequest == request) {
          mShowingRequest = nullptr;
        }
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
                                             nsIPaymentAddress* aAddress) {
  RefPtr<payments::PaymentRequest> request;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!request) {
    return NS_ERROR_FAILURE;
  }
  if (request->GetState() != payments::PaymentRequest::eInteractive) {
    return NS_ERROR_FAILURE;
  }
  if (!request->GetIPC()) {
    return NS_ERROR_FAILURE;
  }
  rv = request->GetIPC()->ChangeShippingAddress(aRequestId, aAddress);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::ChangeShippingOption(const nsAString& aRequestId,
                                            const nsAString& aOption) {
  RefPtr<payments::PaymentRequest> request;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!request) {
    return NS_ERROR_FAILURE;
  }
  if (request->GetState() != payments::PaymentRequest::eInteractive) {
    return NS_ERROR_FAILURE;
  }
  if (!request->GetIPC()) {
    return NS_ERROR_FAILURE;
  }
  rv = request->GetIPC()->ChangeShippingOption(aRequestId, aOption);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::ChangePayerDetail(const nsAString& aRequestId,
                                         const nsAString& aPayerName,
                                         const nsAString& aPayerEmail,
                                         const nsAString& aPayerPhone) {
  RefPtr<payments::PaymentRequest> request;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(request);
  if (!request->GetIPC()) {
    return NS_ERROR_FAILURE;
  }
  rv = request->GetIPC()->ChangePayerDetail(aRequestId, aPayerName, aPayerEmail,
                                            aPayerPhone);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestService::ChangePaymentMethod(
    const nsAString& aRequestId, const nsAString& aMethodName,
    nsIMethodChangeDetails* aMethodDetails) {
  RefPtr<payments::PaymentRequest> request;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (!request) {
    return NS_ERROR_FAILURE;
  }
  if (request->GetState() != payments::PaymentRequest::eInteractive) {
    return NS_ERROR_FAILURE;
  }
  if (!request->GetIPC()) {
    return NS_ERROR_FAILURE;
  }
  rv = request->GetIPC()->ChangePaymentMethod(aRequestId, aMethodName,
                                              aMethodDetails);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

bool PaymentRequestService::CanMakePayment(const nsAString& aRequestId) {
  /*
   *  TODO: Check third party payment app support by traversing all
   *        registered third party payment apps.
   */
  return IsBasicCardPayment(aRequestId);
}

nsresult PaymentRequestService::ShowPayment(const nsAString& aRequestId,
                                            bool aIsUpdating) {
  nsresult rv;
  RefPtr<payments::PaymentRequest> request;
  rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(request);
  request->SetState(payments::PaymentRequest::eInteractive);
  if (aIsUpdating) {
    request->SetCompleteStatus(NS_LITERAL_STRING("initial"));
  }

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
    rv = showResponse->Init(aRequestId, responseStatus, EmptyString(), nullptr,
                            EmptyString(), EmptyString(), EmptyString());
    rv = RespondPayment(showResponse.get());
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  } else {
    mShowingRequest = request;
    rv = LaunchUIAction(aRequestId,
                        IPCPaymentActionRequest::TIPCPaymentShowActionRequest);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

bool PaymentRequestService::IsBasicCardPayment(const nsAString& aRequestId) {
  RefPtr<payments::PaymentRequest> request;
  nsresult rv = GetPaymentRequestById(aRequestId, getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, false);
  nsCOMPtr<nsIArray> methods;
  rv = request->GetPaymentMethods(getter_AddRefs(methods));
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

}  // end of namespace dom
}  // end of namespace mozilla
