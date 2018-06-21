/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestChild_h
#define mozilla_dom_PaymentRequestChild_h

#include "mozilla/dom/PPaymentRequestChild.h"

namespace mozilla {
namespace dom {

class PaymentRequest;

class PaymentRequestChild final : public PPaymentRequestChild
{
public:
  explicit PaymentRequestChild(PaymentRequest* aRequest);

  void MaybeDelete(bool aCanBeInManager);

  nsresult RequestPayment(const IPCPaymentActionRequest& aAction);

protected:
  mozilla::ipc::IPCResult
  RecvRespondPayment(const IPCPaymentActionResponse& aResponse) override;

  mozilla::ipc::IPCResult
  RecvChangeShippingAddress(const nsString& aRequestId,
                            const IPCPaymentAddress& aAddress) override;

  mozilla::ipc::IPCResult
  RecvChangeShippingOption(const nsString& aRequestId,
                           const nsString& aOption) override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

private:
  ~PaymentRequestChild() = default;

  void DetachFromRequest(bool aCanBeInManager);

  PaymentRequest* MOZ_NON_OWNING_REF mRequest;
};

} // end of namespace dom
} // end of namespace mozilla

#endif
