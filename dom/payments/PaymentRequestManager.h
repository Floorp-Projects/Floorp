/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestManager_h
#define mozilla_dom_PaymentRequestManager_h

#include "nsISupports.h"
#include "PaymentRequest.h"
#include "mozilla/dom/PaymentRequestBinding.h"
#include "mozilla/dom/PaymentRequestUpdateEventBinding.h"
#include "mozilla/dom/PaymentResponseBinding.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class PaymentRequestChild;
class IPCMethodChangeDetails;
class IPCPaymentAddress;
class IPCPaymentActionResponse;
class IPCPaymentActionRequest;

/*
 *  PaymentRequestManager is a singleton used to manage the created
 * PaymentRequests. It is also the communication agent to chrome process.
 */
class PaymentRequestManager final {
 public:
  NS_INLINE_DECL_REFCOUNTING(PaymentRequestManager)

  static already_AddRefed<PaymentRequestManager> GetSingleton();

  /*
   *  This method is used to create PaymentRequest object and send corresponding
   *  data to chrome process for internal payment creation, such that content
   *  process can ask specific task by sending requestId only.
   */
  void CreatePayment(JSContext* aCx, nsPIDOMWindowInner* aWindow,
                     nsIPrincipal* aTopLevelPrincipal,
                     const Sequence<PaymentMethodData>& aMethodData,
                     const PaymentDetailsInit& aDetails,
                     const PaymentOptions& aOptions, PaymentRequest** aRequest,
                     ErrorResult& aRv);

  void CanMakePayment(PaymentRequest* aRequest, ErrorResult& aRv);
  void ShowPayment(PaymentRequest* aRequest, ErrorResult& aRv);
  void AbortPayment(PaymentRequest* aRequest, ErrorResult& aRv);
  void CompletePayment(PaymentRequest* aRequest,
                       const PaymentComplete& aComplete, ErrorResult& aRv,
                       bool aTimedOut = false);
  void UpdatePayment(JSContext* aCx, PaymentRequest* aRequest,
                     const PaymentDetailsUpdate& aDetails,
                     bool aRequestShipping, ErrorResult& aRv);
  nsresult ClosePayment(PaymentRequest* aRequest);
  void RetryPayment(JSContext* aCx, PaymentRequest* aRequest,
                    const PaymentValidationErrors& aErrors, ErrorResult& aRv);

  nsresult RespondPayment(PaymentRequest* aRequest,
                          const IPCPaymentActionResponse& aResponse);
  nsresult ChangeShippingAddress(PaymentRequest* aRequest,
                                 const IPCPaymentAddress& aAddress);
  nsresult ChangeShippingOption(PaymentRequest* aRequest,
                                const nsAString& aOption);
  nsresult ChangePayerDetail(PaymentRequest* aRequest,
                             const nsAString& aPayerName,
                             const nsAString& aPayerEmail,
                             const nsAString& aPayerPhone);
  nsresult ChangePaymentMethod(PaymentRequest* aRequest,
                               const nsAString& aMethodName,
                               const IPCMethodChangeDetails& aMethodDetails);

  bool IsRegionSupported(const nsAString& region) const;

  // Called to ensure that we don't "leak" aRequest if we shut down while it had
  // an active request to the parent.
  void RequestIPCOver(PaymentRequest* aRequest);

 private:
  PaymentRequestManager();
  ~PaymentRequestManager();

  PaymentRequestChild* GetPaymentChild(PaymentRequest* aRequest);

  nsresult SendRequestPayment(PaymentRequest* aRequest,
                              const IPCPaymentActionRequest& action,
                              bool aResponseExpected = true);

  void NotifyRequestDone(PaymentRequest* aRequest);

  // Strong pointer to requests with ongoing IPC messages to the parent.
  nsTHashMap<nsRefPtrHashKey<PaymentRequest>, uint32_t> mActivePayments;

  nsTArray<nsString> mSupportedRegions;
};

}  // end of namespace dom
}  // end of namespace mozilla

#endif
