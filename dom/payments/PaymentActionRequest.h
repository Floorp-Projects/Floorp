/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentActionRequest_h
#define mozilla_dom_PaymentActionRequest_h

#include "nsIPaymentActionRequest.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIArray.h"
#include "nsString.h"

namespace mozilla {
namespace dom {

class PaymentActionRequest : public nsIPaymentActionRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTACTIONREQUEST

  PaymentActionRequest() = default;

protected:
  virtual ~PaymentActionRequest() = default;

  nsString mRequestId;
  uint32_t mType;
};

class PaymentCreateActionRequest final : public nsIPaymentCreateActionRequest
                                       , public PaymentActionRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPAYMENTACTIONREQUEST(PaymentActionRequest::)
  NS_DECL_NSIPAYMENTCREATEACTIONREQUEST

  PaymentCreateActionRequest() = default;

private:
  ~PaymentCreateActionRequest() = default;

  uint64_t mTabId;
  nsCOMPtr<nsIArray> mMethodData;
  nsCOMPtr<nsIPaymentDetails> mDetails;
  nsCOMPtr<nsIPaymentOptions> mOptions;
};

} // end of namespace dom
} // end of namespace mozilla

#endif
