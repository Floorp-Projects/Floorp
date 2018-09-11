/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestParent_h
#define mozilla_dom_PaymentRequestParent_h

#include "mozilla/dom/PPaymentRequestParent.h"
#include "nsIPaymentAddress.h"
#include "nsIPaymentActionResponse.h"

namespace mozilla {
namespace dom {

class PaymentRequestParent final : public PPaymentRequestParent
{
  NS_INLINE_DECL_REFCOUNTING(PaymentRequestParent)
public:
  explicit PaymentRequestParent(uint64_t aTabId);

  uint64_t GetTabId();
  nsresult RespondPayment(nsIPaymentActionResponse* aResponse);
  nsresult ChangeShippingAddress(const nsAString& aRequestId,
                                 nsIPaymentAddress* aAddress);
  nsresult ChangeShippingOption(const nsAString& aRequestId,
                                const nsAString& aOption);

protected:
  mozilla::ipc::IPCResult
  RecvRequestPayment(const IPCPaymentActionRequest& aRequest) override;

  mozilla::ipc::IPCResult Recv__delete__() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
private:
  ~PaymentRequestParent() = default;

  bool mActorAlive;
  uint64_t mTabId;
  nsString mRequestId;
};

} // end of namespace dom
} // end of namespace mozilla

#endif
