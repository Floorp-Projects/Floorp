/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/InputStreamUtils.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsIPaymentRequestService.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "PaymentRequestData.h"
#include "PaymentRequestParent.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(PaymentRequestParent, nsIPaymentActionCallback)

PaymentRequestParent::PaymentRequestParent(uint64_t aTabId)
  : mActorAlive(true)
  , mTabId(aTabId)
{
}

mozilla::ipc::IPCResult
PaymentRequestParent::RecvRequestPayment(const IPCPaymentActionRequest& aRequest)
{
  if (!mActorAlive) {
    return IPC_FAIL_NO_REASON(this);
  }
  nsCOMPtr<nsIPaymentActionRequest> action;
  nsresult rv;
  switch (aRequest.type()) {
    case IPCPaymentActionRequest::TIPCPaymentCreateActionRequest: {
      const IPCPaymentCreateActionRequest& request = aRequest;

      nsCOMPtr<nsIMutableArray> methodData = do_CreateInstance(NS_ARRAY_CONTRACTID);
      MOZ_ASSERT(methodData);
      for (IPCPaymentMethodData data : request.methodData()) {
        nsCOMPtr<nsIPaymentMethodData> method;
        rv = payments::PaymentMethodData::Create(data, getter_AddRefs(method));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return IPC_FAIL_NO_REASON(this);
        }
        rv = methodData->AppendElement(method);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return IPC_FAIL_NO_REASON(this);
        }
      }

      nsCOMPtr<nsIPaymentDetails> details;
      rv = payments::PaymentDetails::Create(request.details(), getter_AddRefs(details));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }

      nsCOMPtr<nsIPaymentOptions> options;
      rv = payments::PaymentOptions::Create(request.options(), getter_AddRefs(options));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }

      nsCOMPtr<nsIPaymentCreateActionRequest> createAction =
        do_CreateInstance(NS_PAYMENT_CREATE_ACTION_REQUEST_CONTRACT_ID);
      if (NS_WARN_IF(!createAction)) {
        return IPC_FAIL_NO_REASON(this);
      }
      rv = createAction->InitRequest(request.requestId(),
                                     this,
                                     mTabId,
                                     request.topLevelPrincipal(),
                                     methodData,
                                     details,
                                     options,
                                     request.shippingOption());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }

      action = do_QueryInterface(createAction);
      MOZ_ASSERT(action);
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCanMakeActionRequest: {
      const IPCPaymentCanMakeActionRequest& request = aRequest;
      rv = CreateActionRequest(request.requestId(),
                               nsIPaymentActionRequest::CANMAKE_ACTION,
                               getter_AddRefs(action));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentShowActionRequest: {
      const IPCPaymentShowActionRequest& request = aRequest;
      rv = CreateActionRequest(request.requestId(),
                               nsIPaymentActionRequest::SHOW_ACTION,
                               getter_AddRefs(action));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentAbortActionRequest: {
      const IPCPaymentAbortActionRequest& request = aRequest;
      rv = CreateActionRequest(request.requestId(),
                               nsIPaymentActionRequest::ABORT_ACTION,
                               getter_AddRefs(action));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentCompleteActionRequest: {
      const IPCPaymentCompleteActionRequest& request = aRequest;
      nsCOMPtr<nsIPaymentCompleteActionRequest> completeAction =
        do_CreateInstance(NS_PAYMENT_COMPLETE_ACTION_REQUEST_CONTRACT_ID);
      rv = completeAction->InitRequest(request.requestId(),
                                       this,
                                       request.completeStatus());
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }
      action = do_QueryInterface(completeAction);
      MOZ_ASSERT(action);
      break;
    }
    case IPCPaymentActionRequest::TIPCPaymentUpdateActionRequest: {
      const IPCPaymentUpdateActionRequest& request = aRequest;

      nsCOMPtr<nsIPaymentDetails> details;
      rv = payments::PaymentDetails::Create(request.details(), getter_AddRefs(details));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }

      nsCOMPtr<nsIPaymentUpdateActionRequest> updateAction =
        do_CreateInstance(NS_PAYMENT_UPDATE_ACTION_REQUEST_CONTRACT_ID);
      rv = updateAction->InitRequest(request.requestId(),
                                     this,
                                     details,
                                     request.shippingOption());
      action = do_QueryInterface(updateAction);
      MOZ_ASSERT(action);
      break;
    }
    default: {
      return IPC_FAIL(this, "Unexpected request type");
    }
  }
  nsCOMPtr<nsIPaymentRequestService> service =
    do_GetService(NS_PAYMENT_REQUEST_SERVICE_CONTRACT_ID);
  MOZ_ASSERT(service);
  rv = service->RequestPayment(action);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL(this, "nsIPaymentRequestService::RequestPayment failed");
  }
  return IPC_OK();
}

NS_IMETHODIMP
PaymentRequestParent::RespondPayment(nsIPaymentActionResponse* aResponse)
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIPaymentActionCallback> self = this;
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

NS_IMETHODIMP
PaymentRequestParent::ChangeShippingAddress(const nsAString& aRequestId,
                                            nsIPaymentAddress* aAddress)
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIPaymentActionCallback> self = this;
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

  nsAutoString languageCode;
  rv = aAddress->GetLanguageCode(languageCode);
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

  IPCPaymentAddress ipcAddress(country, addressLine, region, city,
                               dependentLocality, postalCode, sortingCode,
                               languageCode, organization, recipient, phone);

  nsAutoString requestId(aRequestId);
  if (!SendChangeShippingAddress(requestId, ipcAddress)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequestParent::ChangeShippingOption(const nsAString& aRequestId,
                                           const nsAString& aOption)
{
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIPaymentActionCallback> self = this;
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
  nsresult rv = service->RemoveActionCallback(this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    MOZ_ASSERT(false);
  }
}

nsresult
PaymentRequestParent::CreateActionRequest(const nsAString& aRequestId,
                                          uint32_t aActionType,
                                          nsIPaymentActionRequest** aAction)
{
  NS_ENSURE_ARG_POINTER(aAction);
  nsCOMPtr<nsIPaymentActionRequest> action =
    do_CreateInstance(NS_PAYMENT_ACTION_REQUEST_CONTRACT_ID);
  MOZ_ASSERT(action);
  nsresult rv = action->Init(aRequestId, aActionType, this);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  action.forget(aAction);
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
