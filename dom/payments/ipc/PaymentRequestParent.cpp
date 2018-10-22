/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/InputStreamUtils.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsIPaymentRequestService.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "PaymentRequestData.h"
#include "PaymentRequestParent.h"
#include "PaymentRequestService.h"

namespace mozilla {
namespace dom {

PaymentRequestParent::PaymentRequestParent(uint64_t aTabId)
  : mActorAlive(true)
  , mTabId(aTabId)
  , mRequestId(EmptyString())
{
}

mozilla::ipc::IPCResult
PaymentRequestParent::RecvRequestPayment(const IPCPaymentActionRequest& aRequest)
{
  if (!mActorAlive) {
    return IPC_FAIL_NO_REASON(this);
  }
  switch (aRequest.type()) {
    case IPCPaymentActionRequest::TIPCPaymentCreateActionRequest:{
      const IPCPaymentCreateActionRequest& request = aRequest;
      mRequestId = request.requestId();
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCanMakeActionRequest:{
      const IPCPaymentCanMakeActionRequest& request = aRequest;
      mRequestId = request.requestId();
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentShowActionRequest:{
      const IPCPaymentShowActionRequest& request = aRequest;
      mRequestId = request.requestId();
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentAbortActionRequest: {
      const IPCPaymentAbortActionRequest& request = aRequest;
      mRequestId = request.requestId();
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCompleteActionRequest: {
      const IPCPaymentCompleteActionRequest& request = aRequest;
      mRequestId = request.requestId();
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentUpdateActionRequest: {
      const IPCPaymentUpdateActionRequest& request = aRequest;
      mRequestId = request.requestId();
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCloseActionRequest: {
      const IPCPaymentCloseActionRequest& request = aRequest;
      mRequestId = request.requestId();
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentRetryActionRequest: {
      const IPCPaymentRetryActionRequest& request = aRequest;
      mRequestId = request.requestId();
      break;
    }
    default : {
      return IPC_FAIL(this, "Unknown PaymentRequest action type");
    }
  }
  nsCOMPtr<nsIPaymentRequestService> service =
    do_GetService(NS_PAYMENT_REQUEST_SERVICE_CONTRACT_ID);
  MOZ_ASSERT(service);
  PaymentRequestService* rowService =
    static_cast<PaymentRequestService*>(service.get());
  MOZ_ASSERT(rowService);
  nsresult rv = rowService->RequestPayment(mRequestId, aRequest, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL(this, "nsIPaymentRequestService::RequestPayment failed");
  }
  return IPC_OK();
}

uint64_t
PaymentRequestParent::GetTabId()
{
  return mTabId;
}

nsresult
PaymentRequestParent::RespondPayment(nsIPaymentActionResponse* aResponse)
{
  if (!NS_IsMainThread()) {
    RefPtr<PaymentRequestParent> self = this;
    nsCOMPtr<nsIPaymentActionResponse> response = aResponse;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("PaymentRequestParent::RespondPayment",
                                                     [self, response] ()
    {
      self->RespondPayment(response);
    });
    return NS_DispatchToMainThread(r);
  }

  if (!mActorAlive) {
    return NS_ERROR_FAILURE;
  }
  uint32_t type;
  nsresult rv = aResponse->GetType(&type);
  NS_ENSURE_SUCCESS(rv, rv);
  nsAutoString requestId;
  rv = aResponse->GetRequestId(requestId);
  NS_ENSURE_SUCCESS(rv, rv);
  switch (type) {
    case nsIPaymentActionResponse::CANMAKE_ACTION: {
      nsCOMPtr<nsIPaymentCanMakeActionResponse> response = do_QueryInterface(aResponse);
      MOZ_ASSERT(response);
      bool result;
      rv = response->GetResult(&result);
      NS_ENSURE_SUCCESS(rv, rv);
      IPCPaymentCanMakeActionResponse actionResponse(requestId, result);
      if (!SendRespondPayment(actionResponse)) {
        return NS_ERROR_FAILURE;
      }
      break;
    }
    case nsIPaymentActionResponse::SHOW_ACTION: {
      nsCOMPtr<nsIPaymentShowActionResponse> response =
        do_QueryInterface(aResponse);
      MOZ_ASSERT(response);
      uint32_t acceptStatus;
      NS_ENSURE_SUCCESS(response->GetAcceptStatus(&acceptStatus), NS_ERROR_FAILURE);
      nsAutoString methodName;
      NS_ENSURE_SUCCESS(response->GetMethodName(methodName), NS_ERROR_FAILURE);
      nsAutoString data;
      NS_ENSURE_SUCCESS(response->GetData(data), NS_ERROR_FAILURE);
      nsAutoString payerName;
      NS_ENSURE_SUCCESS(response->GetPayerName(payerName), NS_ERROR_FAILURE);
      nsAutoString payerEmail;
      NS_ENSURE_SUCCESS(response->GetPayerEmail(payerEmail), NS_ERROR_FAILURE);
      nsAutoString payerPhone;
      NS_ENSURE_SUCCESS(response->GetPayerPhone(payerPhone), NS_ERROR_FAILURE);
      IPCPaymentShowActionResponse actionResponse(requestId,
                                                  acceptStatus,
                                                  methodName,
                                                  data,
                                                  payerName,
                                                  payerEmail,
                                                  payerPhone);
      if (!SendRespondPayment(actionResponse)) {
        return NS_ERROR_FAILURE;
      }
      break;
    }
    case nsIPaymentActionResponse::ABORT_ACTION: {
      nsCOMPtr<nsIPaymentAbortActionResponse> response =
        do_QueryInterface(aResponse);
      MOZ_ASSERT(response);
      bool isSucceeded;
      rv = response->IsSucceeded(&isSucceeded);
      NS_ENSURE_SUCCESS(rv, rv);
      IPCPaymentAbortActionResponse actionResponse(requestId, isSucceeded);
      if (!SendRespondPayment(actionResponse)) {
        return NS_ERROR_FAILURE;
      }
      break;
    }
    case nsIPaymentActionResponse::COMPLETE_ACTION: {
      nsCOMPtr<nsIPaymentCompleteActionResponse> response =
        do_QueryInterface(aResponse);
      MOZ_ASSERT(response);
      bool isCompleted;
      rv = response->IsCompleted(&isCompleted);
      NS_ENSURE_SUCCESS(rv, rv);
      IPCPaymentCompleteActionResponse actionResponse(requestId, isCompleted);
      if (!SendRespondPayment(actionResponse)) {
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

nsresult
PaymentRequestParent::ChangeShippingAddress(const nsAString& aRequestId,
                                            nsIPaymentAddress* aAddress)
{
  if (!NS_IsMainThread()) {
    RefPtr<PaymentRequestParent> self = this;
    nsCOMPtr<nsIPaymentAddress> address = aAddress;
    nsAutoString requestId(aRequestId);
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("dom::PaymentRequestParent::ChangeShippingAddress",
                                                     [self, requestId, address] ()
    {
      self->ChangeShippingAddress(requestId, address);
    });
    return NS_DispatchToMainThread(r);
  }
  if (!mActorAlive) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString country;
  nsresult rv = aAddress->GetCountry(country);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIArray> iaddressLine;
  rv = aAddress->GetAddressLine(getter_AddRefs(iaddressLine));
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString region;
  rv = aAddress->GetRegion(region);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString regionCode;
  rv = aAddress->GetRegionCode(regionCode);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString city;
  rv = aAddress->GetCity(city);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString dependentLocality;
  rv = aAddress->GetDependentLocality(dependentLocality);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString postalCode;
  rv = aAddress->GetPostalCode(postalCode);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString sortingCode;
  rv = aAddress->GetSortingCode(sortingCode);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString organization;
  rv = aAddress->GetOrganization(organization);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString recipient;
  rv = aAddress->GetRecipient(recipient);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString phone;
  rv = aAddress->GetPhone(phone);
  NS_ENSURE_SUCCESS(rv, rv);

  nsTArray<nsString> addressLine;
  uint32_t length;
  rv = iaddressLine->GetLength(&length);
  NS_ENSURE_SUCCESS(rv, rv);
  for (uint32_t index = 0; index < length; ++index) {
    nsCOMPtr<nsISupportsString> iaddress = do_QueryElementAt(iaddressLine, index);
    MOZ_ASSERT(iaddress);
    nsAutoString address;
    rv = iaddress->GetData(address);
    NS_ENSURE_SUCCESS(rv, rv);
    addressLine.AppendElement(address);
  }

  IPCPaymentAddress ipcAddress(country, addressLine, region, regionCode, city,
                               dependentLocality, postalCode, sortingCode,
                               organization, recipient, phone);

  nsAutoString requestId(aRequestId);
  if (!SendChangeShippingAddress(requestId, ipcAddress)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
PaymentRequestParent::ChangeShippingOption(const nsAString& aRequestId,
                                           const nsAString& aOption)
{
  if (!NS_IsMainThread()) {
    RefPtr<PaymentRequestParent> self = this;
    nsAutoString requestId(aRequestId);
    nsAutoString option(aOption);
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("dom::PaymentRequestParent::ChangeShippingOption",
                                                     [self, requestId, option] ()
    {
      self->ChangeShippingOption(requestId, option);
    });
    return NS_DispatchToMainThread(r);
  }
  if (!mActorAlive) {
    return NS_ERROR_FAILURE;
  }
  nsAutoString requestId(aRequestId);
  nsAutoString option(aOption);
  if (!SendChangeShippingOption(requestId, option)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
PaymentRequestParent::ChangePayerDetail(const nsAString& aRequestId,
                                        const nsAString& aPayerName,
                                        const nsAString& aPayerEmail,
                                        const nsAString& aPayerPhone)
{
  nsAutoString requestId(aRequestId);
  nsAutoString payerName(aPayerName);
  nsAutoString payerEmail(aPayerEmail);
  nsAutoString payerPhone(aPayerPhone);
  if (!NS_IsMainThread()) {
    RefPtr<PaymentRequestParent> self = this;
    nsCOMPtr<nsIRunnable> r = NS_NewRunnableFunction("dom::PaymentRequestParent::ChangePayerDetail",
                                                     [self, requestId, payerName, payerEmail, payerPhone] ()
    {
      self->ChangePayerDetail(requestId, payerName, payerEmail, payerPhone);
    });
    return NS_DispatchToMainThread(r);
  }
  if (!mActorAlive) {
    return NS_ERROR_FAILURE;
  }
  if (!SendChangePayerDetail(requestId, payerName, payerEmail, payerPhone)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

mozilla::ipc::IPCResult
PaymentRequestParent::Recv__delete__()
{
  mActorAlive = false;
  return IPC_OK();
}

void
PaymentRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorAlive = false;
  nsCOMPtr<nsIPaymentRequestService> service =
    do_GetService(NS_PAYMENT_REQUEST_SERVICE_CONTRACT_ID);
  MOZ_ASSERT(service);
  if (!mRequestId.Equals(EmptyString())) {
    nsCOMPtr<nsIPaymentRequest> request;
    nsresult rv = service->GetPaymentRequestById(mRequestId, getter_AddRefs(request));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
    if (!request) {
      return;
    }
    payments::PaymentRequest* rowRequest =
      static_cast<payments::PaymentRequest*>(request.get());
    MOZ_ASSERT(rowRequest);
    rowRequest->SetIPC(nullptr);
  }
}
} // end of namespace dom
} // end of namespace mozilla
