/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentActionResponse_h
#define mozilla_dom_PaymentActionResponse_h

#include "nsCOMPtr.h"
#include "nsIPaymentActionResponse.h"
#include "nsString.h"

namespace mozilla::dom {

class PaymentRequestParent;

class PaymentResponseData : public nsIPaymentResponseData {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTRESPONSEDATA

  PaymentResponseData() = default;

 protected:
  virtual ~PaymentResponseData() = default;

  uint32_t mType;
};

class GeneralResponseData final : public PaymentResponseData,
                                  public nsIGeneralResponseData {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPAYMENTRESPONSEDATA(PaymentResponseData::)
  NS_DECL_NSIGENERALRESPONSEDATA

  GeneralResponseData();

 private:
  ~GeneralResponseData() = default;

  nsString mData;
};

class BasicCardResponseData final : public nsIBasicCardResponseData,
                                    public PaymentResponseData {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPAYMENTRESPONSEDATA(PaymentResponseData::)
  NS_DECL_NSIBASICCARDRESPONSEDATA

  BasicCardResponseData();

 private:
  ~BasicCardResponseData() = default;

  nsString mCardholderName;
  nsString mCardNumber;
  nsString mExpiryMonth;
  nsString mExpiryYear;
  nsString mCardSecurityCode;
  nsCOMPtr<nsIPaymentAddress> mBillingAddress;
};

class PaymentActionResponse : public nsIPaymentActionResponse {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTACTIONRESPONSE

  PaymentActionResponse();

 protected:
  virtual ~PaymentActionResponse() = default;

  nsString mRequestId;
  uint32_t mType;
};

class PaymentCanMakeActionResponse final
    : public nsIPaymentCanMakeActionResponse,
      public PaymentActionResponse {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPAYMENTACTIONRESPONSE(PaymentActionResponse::)
  NS_DECL_NSIPAYMENTCANMAKEACTIONRESPONSE

  PaymentCanMakeActionResponse();

 private:
  ~PaymentCanMakeActionResponse() = default;

  bool mResult;
};

class PaymentShowActionResponse final : public nsIPaymentShowActionResponse,
                                        public PaymentActionResponse {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPAYMENTACTIONRESPONSE(PaymentActionResponse::)
  NS_DECL_NSIPAYMENTSHOWACTIONRESPONSE

  PaymentShowActionResponse();

 private:
  ~PaymentShowActionResponse() = default;

  uint32_t mAcceptStatus;
  nsString mMethodName;
  nsCOMPtr<nsIPaymentResponseData> mData;
  nsString mPayerName;
  nsString mPayerEmail;
  nsString mPayerPhone;
};

class PaymentAbortActionResponse final : public nsIPaymentAbortActionResponse,
                                         public PaymentActionResponse {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPAYMENTACTIONRESPONSE(PaymentActionResponse::)
  NS_DECL_NSIPAYMENTABORTACTIONRESPONSE

  PaymentAbortActionResponse();

 private:
  ~PaymentAbortActionResponse() = default;

  uint32_t mAbortStatus;
};

class PaymentCompleteActionResponse final
    : public nsIPaymentCompleteActionResponse,
      public PaymentActionResponse {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIPAYMENTACTIONRESPONSE(PaymentActionResponse::)
  NS_DECL_NSIPAYMENTCOMPLETEACTIONRESPONSE

  PaymentCompleteActionResponse();

 private:
  ~PaymentCompleteActionResponse() = default;

  uint32_t mCompleteStatus;
};

class MethodChangeDetails : public nsIMethodChangeDetails {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMETHODCHANGEDETAILS

  MethodChangeDetails() = default;

 protected:
  virtual ~MethodChangeDetails() = default;

  uint32_t mType;
};

class GeneralMethodChangeDetails final : public MethodChangeDetails,
                                         public nsIGeneralChangeDetails {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIMETHODCHANGEDETAILS(MethodChangeDetails::)
  NS_DECL_NSIGENERALCHANGEDETAILS

  GeneralMethodChangeDetails();

 private:
  ~GeneralMethodChangeDetails() = default;

  nsString mDetails;
};

class BasicCardMethodChangeDetails final : public MethodChangeDetails,
                                           public nsIBasicCardChangeDetails {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSIMETHODCHANGEDETAILS(MethodChangeDetails::)
  NS_DECL_NSIBASICCARDCHANGEDETAILS

  BasicCardMethodChangeDetails();

 private:
  ~BasicCardMethodChangeDetails() = default;

  nsCOMPtr<nsIPaymentAddress> mBillingAddress;
};

}  // namespace mozilla::dom

#endif
