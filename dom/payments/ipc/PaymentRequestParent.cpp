/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/InputStreamUtils.h"
#include "nsArrayUtils.h"
#include "nsCOMPtr.h"
#include "nsIMutableArray.h"
#include "nsIPaymentActionRequest.h"
#include "nsIPaymentRequestService.h"
#include "nsISupportsPrimitives.h"
#include "nsServiceManagerUtils.h"
#include "PaymentRequestData.h"
#include "PaymentRequestParent.h"

namespace mozilla {
namespace dom {

PaymentRequestParent::PaymentRequestParent(uint64_t aTabId)
  : mActorAlived(true)
  , mTabId(aTabId)
{
}

mozilla::ipc::IPCResult
PaymentRequestParent::RecvRequestPayment(const IPCPaymentActionRequest& aRequest)
{
  MOZ_ASSERT(mActorAlived);
  nsCOMPtr<nsIPaymentActionRequest> actionRequest;
  nsresult rv;
  switch (aRequest.type()) {
    case IPCPaymentActionRequest::TIPCPaymentCreateActionRequest: {
      IPCPaymentCreateActionRequest request = aRequest;

      nsCOMPtr<nsIMutableArray> methodData = do_CreateInstance(NS_ARRAY_CONTRACTID);
      MOZ_ASSERT(methodData);
      for (IPCPaymentMethodData data : request.methodData()) {
        nsCOMPtr<nsIPaymentMethodData> method;
        rv = payments::PaymentMethodData::Create(data, getter_AddRefs(method));
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return IPC_FAIL_NO_REASON(this);
        }
        rv = methodData->AppendElement(method, false);
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

      nsCOMPtr<nsIPaymentCreateActionRequest> createRequest =
        do_CreateInstance(NS_PAYMENT_CREATE_ACTION_REQUEST_CONTRACT_ID);
      if (NS_WARN_IF(!createRequest)) {
        return IPC_FAIL_NO_REASON(this);
      }
      rv = createRequest->InitRequest(request.requestId(),
                                      mTabId,
                                      methodData,
                                      details,
                                      options);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return IPC_FAIL_NO_REASON(this);
      }

      actionRequest = do_QueryInterface(createRequest);
      MOZ_ASSERT(actionRequest);
      break;
    }
    default: {
      return IPC_FAIL(this, "Unexpected request type");
    }
  }
  nsCOMPtr<nsIPaymentRequestService> service =
    do_GetService(NS_PAYMENT_REQUEST_SERVICE_CONTRACT_ID);
  MOZ_ASSERT(service);
  rv = service->RequestPayment(actionRequest);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PaymentRequestParent::Recv__delete__()
{
  mActorAlived = false;
  return IPC_OK();
}

void
PaymentRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorAlived = false;
}

} // end of namespace dom
} // end of namespace mozilla
