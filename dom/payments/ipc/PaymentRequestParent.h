/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestParent_h
#define mozilla_dom_PaymentRequestParent_h

#include "mozilla/dom/PPaymentRequestParent.h"
#include "nsISupports.h"

namespace mozilla {
namespace dom {

class PaymentRequestParent final : public PPaymentRequestParent
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(PaymentRequestParent)

  explicit PaymentRequestParent(uint64_t aTabId);

protected:
  mozilla::ipc::IPCResult
  RecvRequestPayment(const IPCPaymentActionRequest& aRequest) override;

  mozilla::ipc::IPCResult Recv__delete__() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;
private:
  ~PaymentRequestParent() = default;

  bool mActorAlived;
  uint64_t mTabId;
};

} // end of namespace dom
} // end of namespace mozilla

#endif
