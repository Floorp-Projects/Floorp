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

namespace mozilla::dom {

class PaymentRequestParent final : public PPaymentRequestParent {
  friend class PPaymentRequestParent;

  NS_INLINE_DECL_REFCOUNTING(PaymentRequestParent)
 public:
  PaymentRequestParent();

  nsresult RespondPayment(nsIPaymentActionResponse* aResponse);
  nsresult ChangeShippingAddress(const nsAString& aRequestId,
                                 nsIPaymentAddress* aAddress);
  nsresult ChangeShippingOption(const nsAString& aRequestId,
                                const nsAString& aOption);
  nsresult ChangePayerDetail(const nsAString& aRequestId,
                             const nsAString& aPayerName,
                             const nsAString& aPayerEmail,
                             const nsAString& aPayerPhone);
  nsresult ChangePaymentMethod(const nsAString& aRequestId,
                               const nsAString& aMethodName,
                               nsIMethodChangeDetails* aMethodDetails);

 protected:
  mozilla::ipc::IPCResult RecvRequestPayment(
      const IPCPaymentActionRequest& aRequest);

  mozilla::ipc::IPCResult Recv__delete__() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  ~PaymentRequestParent() = default;

  nsresult SerializeAddress(IPCPaymentAddress& ipcAddress,
                            nsIPaymentAddress* aAddress);
  nsresult SerializeResponseData(IPCPaymentResponseData& ipcData,
                                 nsIPaymentResponseData* aData);

  bool mActorAlive;
  nsString mRequestId;
};

}  // namespace mozilla::dom

#endif
