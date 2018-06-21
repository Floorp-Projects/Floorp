/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaymentRequestChild.h"
#include "mozilla/dom/PaymentRequest.h"
#include "mozilla/dom/PaymentRequestManager.h"

namespace mozilla {
namespace dom {

PaymentRequestChild::PaymentRequestChild(PaymentRequest* aRequest)
  : mRequest(aRequest)
{
  mRequest->SetIPC(this);
}

nsresult
PaymentRequestChild::RequestPayment(const IPCPaymentActionRequest& aAction)
{
  if (!mRequest) {
    return NS_ERROR_FAILURE;
  }
  if (!SendRequestPayment(aAction)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

mozilla::ipc::IPCResult
PaymentRequestChild::RecvRespondPayment(const IPCPaymentActionResponse& aResponse)
{
  if (!mRequest) {
    return IPC_FAIL_NO_REASON(this);
  }
  const IPCPaymentActionResponse& response = aResponse;
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);

  // Hold a strong reference to our request for the entire response.
  RefPtr<PaymentRequest> request(mRequest);
  nsresult rv = manager->RespondPayment(request, response);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PaymentRequestChild::RecvChangeShippingAddress(const nsString& aRequestId,
                                               const IPCPaymentAddress& aAddress)
{
  if (!mRequest) {
    return IPC_FAIL_NO_REASON(this);
  }
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  RefPtr<PaymentRequest> request(mRequest);
  nsresult rv = manager->ChangeShippingAddress(request, aAddress);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PaymentRequestChild::RecvChangeShippingOption(const nsString& aRequestId,
                                              const nsString& aOption)
{
  if (!mRequest) {
    return IPC_FAIL_NO_REASON(this);
  }
  RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
  MOZ_ASSERT(manager);
  RefPtr<PaymentRequest> request(mRequest);
  nsresult rv = manager->ChangeShippingOption(request, aOption);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

void
PaymentRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mRequest) {
    DetachFromRequest(true);
  }
}

void
PaymentRequestChild::MaybeDelete(bool aCanBeInManager)
{
  if (mRequest) {
    DetachFromRequest(aCanBeInManager);
    Send__delete__(this);
  }
}

void
PaymentRequestChild::DetachFromRequest(bool aCanBeInManager)
{
  MOZ_ASSERT(mRequest);

  if (aCanBeInManager) {
    RefPtr<PaymentRequestManager> manager = PaymentRequestManager::GetSingleton();
    MOZ_ASSERT(manager);

    RefPtr<PaymentRequest> request(mRequest);
    manager->RequestIPCOver(request);
  }

  mRequest->SetIPC(nullptr);
  mRequest = nullptr;
}

} // end of namespace dom
} // end of namespace mozilla
