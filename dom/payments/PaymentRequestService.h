/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestService_h
#define mozilla_dom_PaymentRequestService_h

#include "nsIPaymentRequest.h"
#include "nsIPaymentRequestService.h"
#include "nsISimpleEnumerator.h"
#include "nsCOMPtr.h"
#include "nsTArray.h"

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

  already_AddRefed<nsIPaymentRequest>
  GetPaymentRequestByIndex(const uint32_t index);

  uint32_t NumPayments() const;

private:
  ~PaymentRequestService() = default;

  nsresult
  SetActionCallback(const nsAString& aRequestId,
                    nsIPaymentActionCallback* aCallback);
  nsresult
  RemoveActionCallback(const nsAString& aRequestId);

  // this method is only used for testing
  nsresult
  CallTestingUIAction(const nsAString& aRequestId, uint32_t aActionType);

  FallibleTArray<nsCOMPtr<nsIPaymentRequest>> mRequestQueue;

  nsInterfaceHashtable<nsStringHashKey, nsIPaymentActionCallback> mCallbackHashtable;

  nsCOMPtr<nsIPaymentUIService> mTestingUIService;

  nsCOMPtr<nsIPaymentRequest> mShowingRequest;
};

} // end of namespace dom
} // end of namespace mozilla

#endif
