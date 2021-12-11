/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PaymentRequestData_h
#define mozilla_dom_PaymentRequestData_h

#include "nsIPaymentAddress.h"
#include "nsIPaymentRequest.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/PaymentRequestParent.h"

namespace mozilla {
namespace dom {
namespace payments {

class PaymentMethodData final : public nsIPaymentMethodData {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTMETHODDATA

  static nsresult Create(const IPCPaymentMethodData& aIPCMethodData,
                         nsIPaymentMethodData** aMethodData);

 private:
  PaymentMethodData(const nsAString& aSupportedMethods, const nsAString& aData);

  ~PaymentMethodData() = default;

  nsString mSupportedMethods;
  nsString mData;
};

class PaymentCurrencyAmount final : public nsIPaymentCurrencyAmount {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTCURRENCYAMOUNT

  static nsresult Create(const IPCPaymentCurrencyAmount& aIPCAmount,
                         nsIPaymentCurrencyAmount** aAmount);

 private:
  PaymentCurrencyAmount(const nsAString& aCurrency, const nsAString& aValue);

  ~PaymentCurrencyAmount() = default;

  nsString mCurrency;
  nsString mValue;
};

class PaymentItem final : public nsIPaymentItem {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTITEM

  static nsresult Create(const IPCPaymentItem& aIPCItem,
                         nsIPaymentItem** aItem);

 private:
  PaymentItem(const nsAString& aLabel, nsIPaymentCurrencyAmount* aAmount,
              const bool aPending);

  ~PaymentItem() = default;

  nsString mLabel;
  nsCOMPtr<nsIPaymentCurrencyAmount> mAmount;
  bool mPending;
};

class PaymentDetailsModifier final : public nsIPaymentDetailsModifier {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTDETAILSMODIFIER

  static nsresult Create(const IPCPaymentDetailsModifier& aIPCModifier,
                         nsIPaymentDetailsModifier** aModifier);

 private:
  PaymentDetailsModifier(const nsAString& aSupportedMethods,
                         nsIPaymentItem* aTotal,
                         nsIArray* aAdditionalDisplayItems,
                         const nsAString& aData);

  ~PaymentDetailsModifier() = default;

  nsString mSupportedMethods;
  nsCOMPtr<nsIPaymentItem> mTotal;
  nsCOMPtr<nsIArray> mAdditionalDisplayItems;
  nsString mData;
};

class PaymentShippingOption final : public nsIPaymentShippingOption {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTSHIPPINGOPTION

  static nsresult Create(const IPCPaymentShippingOption& aIPCOption,
                         nsIPaymentShippingOption** aOption);

 private:
  PaymentShippingOption(const nsAString& aId, const nsAString& aLabel,
                        nsIPaymentCurrencyAmount* aAmount,
                        const bool aSelected = false);

  ~PaymentShippingOption() = default;

  nsString mId;
  nsString mLabel;
  nsCOMPtr<nsIPaymentCurrencyAmount> mAmount;
  bool mSelected;
};

class PaymentDetails final : public nsIPaymentDetails {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTDETAILS

  static nsresult Create(const IPCPaymentDetails& aIPCDetails,
                         nsIPaymentDetails** aDetails);
  nsresult Update(nsIPaymentDetails* aDetails, const bool aRequestShipping);
  const nsString& GetShippingAddressErrors() const;
  const nsString& GetPayerErrors() const;
  const nsString& GetPaymentMethodErrors() const;
  nsresult UpdateErrors(const nsAString& aError, const nsAString& aPayerErrors,
                        const nsAString& aPaymentMethodErrors,
                        const nsAString& aShippingAddressErrors);

 private:
  PaymentDetails(const nsAString& aId, nsIPaymentItem* aTotalItem,
                 nsIArray* aDisplayItems, nsIArray* aShippingOptions,
                 nsIArray* aModifiers, const nsAString& aError,
                 const nsAString& aShippingAddressError,
                 const nsAString& aPayerError,
                 const nsAString& aPaymentMethodError);

  ~PaymentDetails() = default;

  nsString mId;
  nsCOMPtr<nsIPaymentItem> mTotalItem;
  nsCOMPtr<nsIArray> mDisplayItems;
  nsCOMPtr<nsIArray> mShippingOptions;
  nsCOMPtr<nsIArray> mModifiers;
  nsString mError;
  nsString mShippingAddressErrors;
  nsString mPayerErrors;
  nsString mPaymentMethodErrors;
};

class PaymentOptions final : public nsIPaymentOptions {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTOPTIONS

  static nsresult Create(const IPCPaymentOptions& aIPCOptions,
                         nsIPaymentOptions** aOptions);

 private:
  PaymentOptions(const bool aRequestPayerName, const bool aRequestPayerEmail,
                 const bool aRequestPayerPhone, const bool aRequestShipping,
                 const bool aRequestBillingAddress,
                 const nsAString& aShippintType);
  ~PaymentOptions() = default;

  bool mRequestPayerName;
  bool mRequestPayerEmail;
  bool mRequestPayerPhone;
  bool mRequestShipping;
  bool mRequestBillingAddress;
  nsString mShippingType;
};

class PaymentRequest final : public nsIPaymentRequest {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTREQUEST

  PaymentRequest(const uint64_t aTopOuterWindowId, const nsAString& aRequestId,
                 nsIPrincipal* aPrincipal, nsIArray* aPaymentMethods,
                 nsIPaymentDetails* aPaymentDetails,
                 nsIPaymentOptions* aPaymentOptions,
                 const nsAString& aShippingOption);

  void SetIPC(PaymentRequestParent* aIPC) { mIPC = aIPC; }

  PaymentRequestParent* GetIPC() const { return mIPC; }

  nsresult UpdatePaymentDetails(nsIPaymentDetails* aPaymentDetails,
                                const nsAString& aShippingOption);

  void SetCompleteStatus(const nsAString& aCompleteStatus);

  nsresult UpdateErrors(const nsAString& aError, const nsAString& aPayerErrors,
                        const nsAString& aPaymentMethodErrors,
                        const nsAString& aShippingAddressErrors);

  // The state represents the PaymentRequest's state in the spec. The state is
  // not synchronized between content and parent processes.
  // eCreated     - the state means a PaymentRequest is created when new
  //                PaymentRequest() is called. This is the initial state.
  // eInteractive - When PaymentRequest is requested to show to users, the state
  //                becomes eInteractive. Under eInteractive state, Payment UI
  //                pop up and gather the user's information until the user
  //                accepts or rejects the PaymentRequest.
  // eClosed      - When the user accepts or rejects the PaymentRequest, the
  //                state becomes eClosed. Under eClosed state, response from
  //                Payment UI would not be accepted by PaymentRequestService
  //                anymore, except the Complete response.
  enum eState { eCreated, eInteractive, eClosed };

  void SetState(const eState aState) { mState = aState; }

  const eState& GetState() const { return mState; }

 private:
  ~PaymentRequest() = default;

  uint64_t mTopOuterWindowId;
  nsString mRequestId;
  nsString mCompleteStatus;
  nsCOMPtr<nsIPrincipal> mTopLevelPrincipal;
  nsCOMPtr<nsIArray> mPaymentMethods;
  nsCOMPtr<nsIPaymentDetails> mPaymentDetails;
  nsCOMPtr<nsIPaymentOptions> mPaymentOptions;
  nsString mShippingOption;

  // IPC's life cycle should be controlled by IPC mechanism.
  // PaymentRequest should not own the reference of it.
  PaymentRequestParent* mIPC;
  eState mState;
};

class PaymentAddress final : public nsIPaymentAddress {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAYMENTADDRESS

  PaymentAddress() = default;

 private:
  ~PaymentAddress() = default;

  nsString mCountry;
  nsCOMPtr<nsIArray> mAddressLine;
  nsString mRegion;
  nsString mRegionCode;
  nsString mCity;
  nsString mDependentLocality;
  nsString mPostalCode;
  nsString mSortingCode;
  nsString mOrganization;
  nsString mRecipient;
  nsString mPhone;
};

}  // namespace payments
}  // end of namespace dom
}  // end of namespace mozilla

#endif
