/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestService_h
#define mozilla_dom_PaymentRequestService_h

#include "nsInterfaceHashtable.h"
#include "nsIPaymentRequestService.h"
#include "nsISimpleEnumerator.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"
#include "PaymentRequestData.h"

namespace mozilla {
namespace dom {

// The implmentation of nsIPaymentRequestService

class PaymentRequestService final : public nsIPaymentRequestService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTREQUESTSERVICE

  PaymentRequestService() = default;

  static already_AddRefed<PaymentRequestService> GetSingleton();

  already_AddRefed<payments::PaymentRequest>
  GetPaymentRequestByIndex(const uint32_t index);

  uint32_t NumPayments() const;

  nsresult RequestPayment(const nsAString& aRequestId,
                          const IPCPaymentActionRequest& aAction,
                          PaymentRequestParent* aCallback);
private:
  ~PaymentRequestService() = default;

  nsresult GetPaymentRequestById(const nsAString& aRequestId,
                                 payments::PaymentRequest** aRequest);

  nsresult
  LaunchUIAction(const nsAString& aRequestId, uint32_t aActionType);

  bool
  CanMakePayment(const nsAString& aRequestId);

  bool
  IsBasicCardPayment(const nsAString& aRequestId);

  FallibleTArray<RefPtr<payments::PaymentRequest>> mRequestQueue;

  nsCOMPtr<nsIPaymentUIService> mTestingUIService;

  RefPtr<payments::PaymentRequest> mShowingRequest;
};

} // end of namespace dom
} // end of namespace mozilla

#endif
