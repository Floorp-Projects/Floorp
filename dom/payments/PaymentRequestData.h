/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestData_h
#define mozilla_dom_PaymentRequestData_h

#include "nsIPaymentRequest.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/PPaymentRequest.h"

namespace mozilla {
namespace dom {
namespace payments {

class PaymentMethodData final : public nsIPaymentMethodData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTMETHODDATA

  static nsresult Create(const IPCPaymentMethodData& aIPCMethodData,
                         nsIPaymentMethodData** aMethodData);

private:
  PaymentMethodData(nsIArray* aSupportedMethods,
                    const nsAString& aData);

  ~PaymentMethodData() = default;

  nsCOMPtr<nsIArray> mSupportedMethods;
  nsString mData;
};

class PaymentCurrencyAmount final : public nsIPaymentCurrencyAmount
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTCURRENCYAMOUNT

  static nsresult Create(const IPCPaymentCurrencyAmount& aIPCAmount,
                         nsIPaymentCurrencyAmount** aAmount);

private:
  PaymentCurrencyAmount(const nsAString& aCurrency,
                        const nsAString& aValue);

  ~PaymentCurrencyAmount() = default;

  nsString mCurrency;
  nsString mValue;
};

class PaymentItem final : public nsIPaymentItem
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTITEM

  static nsresult Create(const IPCPaymentItem& aIPCItem, nsIPaymentItem** aItem);

private:
  PaymentItem(const nsAString& aLabel,
              nsIPaymentCurrencyAmount* aAmount,
              const bool aPending);

  ~PaymentItem() = default;

  nsString mLabel;
  nsCOMPtr<nsIPaymentCurrencyAmount> mAmount;
  bool mPending;
};

class PaymentDetailsModifier final : public nsIPaymentDetailsModifier
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTDETAILSMODIFIER

  static nsresult Create(const IPCPaymentDetailsModifier& aIPCModifier,
                         nsIPaymentDetailsModifier** aModifier);

private:
  PaymentDetailsModifier(nsIArray* aSupportedMethods,
                         nsIPaymentItem* aTotal,
                         nsIArray* aAdditionalDisplayItems,
                         const nsAString& aData);

  ~PaymentDetailsModifier() = default;

  nsCOMPtr<nsIArray> mSupportedMethods;
  nsCOMPtr<nsIPaymentItem> mTotal;
  nsCOMPtr<nsIArray> mAdditionalDisplayItems;
  nsString mData;
};

class PaymentShippingOption final : public nsIPaymentShippingOption
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTSHIPPINGOPTION

  static nsresult Create(const IPCPaymentShippingOption& aIPCOption,
                         nsIPaymentShippingOption** aOption);

private:
  PaymentShippingOption(const nsAString& aId,
                        const nsAString& aLabel,
                        nsIPaymentCurrencyAmount* aAmount,
                        const bool aSelected=false);

  ~PaymentShippingOption() = default;

  nsString mId;
  nsString mLabel;
  nsCOMPtr<nsIPaymentCurrencyAmount> mAmount;
  bool mSelected;
};

class PaymentDetails final : public nsIPaymentDetails
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTDETAILS


  static nsresult Create(const IPCPaymentDetails& aIPCDetails,
                         nsIPaymentDetails** aDetails);
private:
  PaymentDetails(const nsAString& aId,
                 nsIPaymentItem* aTotalItem,
                 nsIArray* aDisplayItems,
                 nsIArray* aShippingOptions,
                 nsIArray* aModifiers,
                 const nsAString& aError);

  ~PaymentDetails() = default;

  nsString mId;
  nsCOMPtr<nsIPaymentItem> mTotalItem;
  nsCOMPtr<nsIArray> mDisplayItems;
  nsCOMPtr<nsIArray> mShippingOptions;
  nsCOMPtr<nsIArray> mModifiers;
  nsString mError;
};

class PaymentOptions final : public nsIPaymentOptions
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTOPTIONS

  static nsresult Create(const IPCPaymentOptions& aIPCOptions,
                         nsIPaymentOptions** aOptions);

private:
  PaymentOptions(const bool aRequestPayerName,
                 const bool aRequestPayerEmail,
                 const bool aRequestPayerPhone,
                 const bool aRequestShipping,
                 const nsAString& aShippintType);
  ~PaymentOptions() = default;

  bool mRequestPayerName;
  bool mRequestPayerEmail;
  bool mRequestPayerPhone;
  bool mRequestShipping;
  nsString mShippingType;
};

class PaymentRequest final : public nsIPaymentRequest
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTREQUEST

  PaymentRequest(const uint64_t aTabId,
                 const nsAString& aRequestId,
                 nsIArray* aPaymentMethods,
                 nsIPaymentDetails* aPaymentDetails,
                 nsIPaymentOptions* aPaymentOptions);

private:
  ~PaymentRequest() = default;

  uint64_t mTabId;
  nsString mRequestId;
  nsCOMPtr<nsIArray> mPaymentMethods;
  nsCOMPtr<nsIPaymentDetails> mPaymentDetails;
  nsCOMPtr<nsIPaymentOptions> mPaymentOptions;
};

} // end of namespace payment
} // end of namespace dom
} // end of namespace mozilla

#endif
