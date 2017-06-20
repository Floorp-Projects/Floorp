/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestManager_h
#define mozilla_dom_PaymentRequestManager_h

#include "nsISupports.h"
#include "PaymentRequest.h"
#include "mozilla/dom/PaymentRequestBinding.h"
#include "mozilla/dom/PaymentResponseBinding.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class PaymentRequestChild;
class IPCPaymentActionRequest;

/*
 *  PaymentRequestManager is a singleton used to manage the created PaymentRequests.
 *  It is also the communication agent to chrome proces.
 */
class PaymentRequestManager final
{
public:
  NS_INLINE_DECL_REFCOUNTING(PaymentRequestManager)

  static already_AddRefed<PaymentRequestManager> GetSingleton();

  already_AddRefed<PaymentRequest>
  GetPaymentRequestById(const nsAString& aRequestId);

  /*
   *  This method is used to create PaymentRequest object and send corresponding
   *  data to chrome process for internal payment creation, such that content
   *  process can ask specific task by sending requestId only.
   */
  nsresult
  CreatePayment(nsPIDOMWindowInner* aWindow,
                const Sequence<PaymentMethodData>& aMethodData,
                const PaymentDetailsInit& aDetails,
                const PaymentOptions& aOptions,
                PaymentRequest** aRequest);

  nsresult CanMakePayment(const nsAString& aRequestId);
  nsresult ShowPayment(const nsAString& aRequestId);
  nsresult AbortPayment(const nsAString& aRequestId);
  nsresult CompletePayment(const nsAString& aRequestId,
                           const PaymentComplete& aComplete);

  nsresult RespondPayment(const IPCPaymentActionResponse& aResponse);

  nsresult
  ReleasePaymentChild(PaymentRequestChild* aPaymentChild);

private:
  PaymentRequestManager() = default;
  ~PaymentRequestManager() = default;

  nsresult GetPaymentChild(PaymentRequest* aRequest,
                           PaymentRequestChild** aPaymentChild);
  nsresult ReleasePaymentChild(PaymentRequest* aRequest);

  nsresult SendRequestPayment(PaymentRequest* aRequest,
                              const IPCPaymentActionRequest& action,
                              bool aReleaseAfterSend = false);

  // The container for the created PaymentRequests
  nsTArray<RefPtr<PaymentRequest>> mRequestQueue;
  nsRefPtrHashtable<nsRefPtrHashKey<PaymentRequest>, PaymentRequestChild> mPaymentChildHash;
};

} // end of namespace dom
} // end of namespace mozilla

#endif
