/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PaymentRequestBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsArrayUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIMutableArray.h"
#include "nsUnicharUtils.h"
#include "PaymentRequestData.h"
#include "PaymentRequestUtils.h"

namespace mozilla::dom::payments {

/* PaymentMethodData */

NS_IMPL_ISUPPORTS(PaymentMethodData, nsIPaymentMethodData)

PaymentMethodData::PaymentMethodData(const nsAString& aSupportedMethods,
                                     const nsAString& aData)
    : mSupportedMethods(aSupportedMethods), mData(aData) {}

nsresult PaymentMethodData::Create(const IPCPaymentMethodData& aIPCMethodData,
                                   nsIPaymentMethodData** aMethodData) {
  NS_ENSURE_ARG_POINTER(aMethodData);
  nsCOMPtr<nsIPaymentMethodData> methodData = new PaymentMethodData(
      aIPCMethodData.supportedMethods(), aIPCMethodData.data());
  methodData.forget(aMethodData);
  return NS_OK;
}

NS_IMETHODIMP
PaymentMethodData::GetSupportedMethods(nsAString& aSupportedMethods) {
  aSupportedMethods = mSupportedMethods;
  return NS_OK;
}

NS_IMETHODIMP
PaymentMethodData::GetData(JSContext* aCx, JS::MutableHandleValue aData) {
  if (mData.IsEmpty()) {
    aData.set(JS::NullValue());
    return NS_OK;
  }
  nsresult rv = DeserializeToJSValue(mData, aCx, aData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

/* PaymentCurrencyAmount */

NS_IMPL_ISUPPORTS(PaymentCurrencyAmount, nsIPaymentCurrencyAmount)

PaymentCurrencyAmount::PaymentCurrencyAmount(const nsAString& aCurrency,
                                             const nsAString& aValue)
    : mValue(aValue) {
  /*
   *  According to the spec
   *  https://w3c.github.io/payment-request/#validity-checkers
   *  Set amount.currency to the result of ASCII uppercasing amount.currency.
   */
  ToUpperCase(aCurrency, mCurrency);
}

nsresult PaymentCurrencyAmount::Create(
    const IPCPaymentCurrencyAmount& aIPCAmount,
    nsIPaymentCurrencyAmount** aAmount) {
  NS_ENSURE_ARG_POINTER(aAmount);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount =
      new PaymentCurrencyAmount(aIPCAmount.currency(), aIPCAmount.value());
  amount.forget(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
PaymentCurrencyAmount::GetCurrency(nsAString& aCurrency) {
  aCurrency = mCurrency;
  return NS_OK;
}

NS_IMETHODIMP
PaymentCurrencyAmount::GetValue(nsAString& aValue) {
  aValue = mValue;
  return NS_OK;
}

/* PaymentItem */

NS_IMPL_ISUPPORTS(PaymentItem, nsIPaymentItem)

PaymentItem::PaymentItem(const nsAString& aLabel,
                         nsIPaymentCurrencyAmount* aAmount, const bool aPending)
    : mLabel(aLabel), mAmount(aAmount), mPending(aPending) {}

nsresult PaymentItem::Create(const IPCPaymentItem& aIPCItem,
                             nsIPaymentItem** aItem) {
  NS_ENSURE_ARG_POINTER(aItem);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount;
  nsresult rv =
      PaymentCurrencyAmount::Create(aIPCItem.amount(), getter_AddRefs(amount));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCOMPtr<nsIPaymentItem> item =
      new PaymentItem(aIPCItem.label(), amount, aIPCItem.pending());
  item.forget(aItem);
  return NS_OK;
}

NS_IMETHODIMP
PaymentItem::GetLabel(nsAString& aLabel) {
  aLabel = mLabel;
  return NS_OK;
}

NS_IMETHODIMP
PaymentItem::GetAmount(nsIPaymentCurrencyAmount** aAmount) {
  NS_ENSURE_ARG_POINTER(aAmount);
  MOZ_ASSERT(mAmount);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount = mAmount;
  amount.forget(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
PaymentItem::GetPending(bool* aPending) {
  NS_ENSURE_ARG_POINTER(aPending);
  *aPending = mPending;
  return NS_OK;
}

/* PaymentDetailsModifier */

NS_IMPL_ISUPPORTS(PaymentDetailsModifier, nsIPaymentDetailsModifier)

PaymentDetailsModifier::PaymentDetailsModifier(
    const nsAString& aSupportedMethods, nsIPaymentItem* aTotal,
    nsIArray* aAdditionalDisplayItems, const nsAString& aData)
    : mSupportedMethods(aSupportedMethods),
      mTotal(aTotal),
      mAdditionalDisplayItems(aAdditionalDisplayItems),
      mData(aData) {}

nsresult PaymentDetailsModifier::Create(
    const IPCPaymentDetailsModifier& aIPCModifier,
    nsIPaymentDetailsModifier** aModifier) {
  NS_ENSURE_ARG_POINTER(aModifier);
  nsCOMPtr<nsIPaymentItem> total;
  nsresult rv =
      PaymentItem::Create(aIPCModifier.total(), getter_AddRefs(total));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIArray> displayItems;
  if (aIPCModifier.additionalDisplayItemsPassed()) {
    nsCOMPtr<nsIMutableArray> items = do_CreateInstance(NS_ARRAY_CONTRACTID);
    MOZ_ASSERT(items);
    for (const IPCPaymentItem& item : aIPCModifier.additionalDisplayItems()) {
      nsCOMPtr<nsIPaymentItem> additionalItem;
      rv = PaymentItem::Create(item, getter_AddRefs(additionalItem));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      rv = items->AppendElement(additionalItem);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    displayItems = std::move(items);
  }
  nsCOMPtr<nsIPaymentDetailsModifier> modifier =
      new PaymentDetailsModifier(aIPCModifier.supportedMethods(), total,
                                 displayItems, aIPCModifier.data());
  modifier.forget(aModifier);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetailsModifier::GetSupportedMethods(nsAString& aSupportedMethods) {
  aSupportedMethods = mSupportedMethods;
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetailsModifier::GetTotal(nsIPaymentItem** aTotal) {
  NS_ENSURE_ARG_POINTER(aTotal);
  MOZ_ASSERT(mTotal);
  nsCOMPtr<nsIPaymentItem> total = mTotal;
  total.forget(aTotal);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetailsModifier::GetAdditionalDisplayItems(
    nsIArray** aAdditionalDisplayItems) {
  NS_ENSURE_ARG_POINTER(aAdditionalDisplayItems);
  nsCOMPtr<nsIArray> additionalItems = mAdditionalDisplayItems;
  additionalItems.forget(aAdditionalDisplayItems);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetailsModifier::GetData(JSContext* aCx, JS::MutableHandleValue aData) {
  if (mData.IsEmpty()) {
    aData.set(JS::NullValue());
    return NS_OK;
  }
  nsresult rv = DeserializeToJSValue(mData, aCx, aData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

/* PaymentShippingOption */

NS_IMPL_ISUPPORTS(PaymentShippingOption, nsIPaymentShippingOption)

PaymentShippingOption::PaymentShippingOption(const nsAString& aId,
                                             const nsAString& aLabel,
                                             nsIPaymentCurrencyAmount* aAmount,
                                             const bool aSelected)
    : mId(aId), mLabel(aLabel), mAmount(aAmount), mSelected(aSelected) {}

nsresult PaymentShippingOption::Create(
    const IPCPaymentShippingOption& aIPCOption,
    nsIPaymentShippingOption** aOption) {
  NS_ENSURE_ARG_POINTER(aOption);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount;
  nsresult rv = PaymentCurrencyAmount::Create(aIPCOption.amount(),
                                              getter_AddRefs(amount));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCOMPtr<nsIPaymentShippingOption> option = new PaymentShippingOption(
      aIPCOption.id(), aIPCOption.label(), amount, aIPCOption.selected());
  option.forget(aOption);
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::GetId(nsAString& aId) {
  aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::GetLabel(nsAString& aLabel) {
  aLabel = mLabel;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::GetAmount(nsIPaymentCurrencyAmount** aAmount) {
  NS_ENSURE_ARG_POINTER(aAmount);
  MOZ_ASSERT(mAmount);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount = mAmount;
  amount.forget(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::GetSelected(bool* aSelected) {
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = mSelected;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::SetSelected(bool aSelected) {
  mSelected = aSelected;
  return NS_OK;
}

/* PaymentDetails */

NS_IMPL_ISUPPORTS(PaymentDetails, nsIPaymentDetails)

PaymentDetails::PaymentDetails(const nsAString& aId, nsIPaymentItem* aTotalItem,
                               nsIArray* aDisplayItems,
                               nsIArray* aShippingOptions, nsIArray* aModifiers,
                               const nsAString& aError,
                               const nsAString& aShippingAddressErrors,
                               const nsAString& aPayerErrors,
                               const nsAString& aPaymentMethodErrors)
    : mId(aId),
      mTotalItem(aTotalItem),
      mDisplayItems(aDisplayItems),
      mShippingOptions(aShippingOptions),
      mModifiers(aModifiers),
      mError(aError),
      mShippingAddressErrors(aShippingAddressErrors),
      mPayerErrors(aPayerErrors),
      mPaymentMethodErrors(aPaymentMethodErrors) {}

nsresult PaymentDetails::Create(const IPCPaymentDetails& aIPCDetails,
                                nsIPaymentDetails** aDetails) {
  NS_ENSURE_ARG_POINTER(aDetails);

  nsCOMPtr<nsIPaymentItem> total;
  nsresult rv = PaymentItem::Create(aIPCDetails.total(), getter_AddRefs(total));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIArray> displayItems;
  nsCOMPtr<nsIMutableArray> items = do_CreateInstance(NS_ARRAY_CONTRACTID);
  MOZ_ASSERT(items);
  for (const IPCPaymentItem& displayItem : aIPCDetails.displayItems()) {
    nsCOMPtr<nsIPaymentItem> item;
    rv = PaymentItem::Create(displayItem, getter_AddRefs(item));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = items->AppendElement(item);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  displayItems = std::move(items);

  nsCOMPtr<nsIArray> shippingOptions;
  nsCOMPtr<nsIMutableArray> options = do_CreateInstance(NS_ARRAY_CONTRACTID);
  MOZ_ASSERT(options);
  for (const IPCPaymentShippingOption& shippingOption :
       aIPCDetails.shippingOptions()) {
    nsCOMPtr<nsIPaymentShippingOption> option;
    rv = PaymentShippingOption::Create(shippingOption, getter_AddRefs(option));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = options->AppendElement(option);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  shippingOptions = std::move(options);

  nsCOMPtr<nsIArray> modifiers;
  nsCOMPtr<nsIMutableArray> detailsModifiers =
      do_CreateInstance(NS_ARRAY_CONTRACTID);
  MOZ_ASSERT(detailsModifiers);
  for (const IPCPaymentDetailsModifier& modifier : aIPCDetails.modifiers()) {
    nsCOMPtr<nsIPaymentDetailsModifier> detailsModifier;
    rv = PaymentDetailsModifier::Create(modifier,
                                        getter_AddRefs(detailsModifier));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    rv = detailsModifiers->AppendElement(detailsModifier);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  modifiers = std::move(detailsModifiers);

  nsCOMPtr<nsIPaymentDetails> details = new PaymentDetails(
      aIPCDetails.id(), total, displayItems, shippingOptions, modifiers,
      aIPCDetails.error(), aIPCDetails.shippingAddressErrors(),
      aIPCDetails.payerErrors(), aIPCDetails.paymentMethodErrors());

  details.forget(aDetails);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetId(nsAString& aId) {
  aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetTotalItem(nsIPaymentItem** aTotalItem) {
  NS_ENSURE_ARG_POINTER(aTotalItem);
  MOZ_ASSERT(mTotalItem);
  nsCOMPtr<nsIPaymentItem> total = mTotalItem;
  total.forget(aTotalItem);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetDisplayItems(nsIArray** aDisplayItems) {
  NS_ENSURE_ARG_POINTER(aDisplayItems);
  nsCOMPtr<nsIArray> displayItems = mDisplayItems;
  displayItems.forget(aDisplayItems);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetShippingOptions(nsIArray** aShippingOptions) {
  NS_ENSURE_ARG_POINTER(aShippingOptions);
  nsCOMPtr<nsIArray> options = mShippingOptions;
  options.forget(aShippingOptions);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetModifiers(nsIArray** aModifiers) {
  NS_ENSURE_ARG_POINTER(aModifiers);
  nsCOMPtr<nsIArray> modifiers = mModifiers;
  modifiers.forget(aModifiers);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetError(nsAString& aError) {
  aError = mError;
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetShippingAddressErrors(JSContext* aCx,
                                         JS::MutableHandleValue aErrors) {
  AddressErrors errors;
  errors.Init(mShippingAddressErrors);
  if (!ToJSValue(aCx, errors, aErrors)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetPayerErrors(JSContext* aCx, JS::MutableHandleValue aErrors) {
  PayerErrors errors;
  errors.Init(mPayerErrors);
  if (!ToJSValue(aCx, errors, aErrors)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetPaymentMethodErrors(JSContext* aCx,
                                       JS::MutableHandleValue aErrors) {
  if (mPaymentMethodErrors.IsEmpty()) {
    aErrors.set(JS::NullValue());
    return NS_OK;
  }
  nsresult rv = DeserializeToJSValue(mPaymentMethodErrors, aCx, aErrors);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

nsresult PaymentDetails::Update(nsIPaymentDetails* aDetails,
                                const bool aRequestShipping) {
  MOZ_ASSERT(aDetails);
  /*
   * According to the spec [1], update the attributes if they present in new
   * details (i.e., PaymentDetailsUpdate); otherwise, keep original value.
   * Note |id| comes only from initial details (i.e., PaymentDetailsInit) and
   * |error| only from new details.
   *
   *   [1] https://www.w3.org/TR/payment-request/#updatewith-method
   */

  nsresult rv = aDetails->GetTotalItem(getter_AddRefs(mTotalItem));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIArray> displayItems;
  rv = aDetails->GetDisplayItems(getter_AddRefs(displayItems));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (displayItems) {
    mDisplayItems = displayItems;
  }

  if (aRequestShipping) {
    nsCOMPtr<nsIArray> shippingOptions;
    rv = aDetails->GetShippingOptions(getter_AddRefs(shippingOptions));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    mShippingOptions = shippingOptions;
  }

  nsCOMPtr<nsIArray> modifiers;
  rv = aDetails->GetModifiers(getter_AddRefs(modifiers));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  if (modifiers) {
    mModifiers = modifiers;
  }

  rv = aDetails->GetError(mError);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  PaymentDetails* rowDetails = static_cast<PaymentDetails*>(aDetails);
  MOZ_ASSERT(rowDetails);
  mShippingAddressErrors = rowDetails->GetShippingAddressErrors();
  mPayerErrors = rowDetails->GetPayerErrors();
  mPaymentMethodErrors = rowDetails->GetPaymentMethodErrors();

  return NS_OK;
}

const nsString& PaymentDetails::GetShippingAddressErrors() const {
  return mShippingAddressErrors;
}

const nsString& PaymentDetails::GetPayerErrors() const { return mPayerErrors; }

const nsString& PaymentDetails::GetPaymentMethodErrors() const {
  return mPaymentMethodErrors;
}

nsresult PaymentDetails::UpdateErrors(const nsAString& aError,
                                      const nsAString& aPayerErrors,
                                      const nsAString& aPaymentMethodErrors,
                                      const nsAString& aShippingAddressErrors) {
  mError = aError;
  mPayerErrors = aPayerErrors;
  mPaymentMethodErrors = aPaymentMethodErrors;
  mShippingAddressErrors = aShippingAddressErrors;
  return NS_OK;
}

/* PaymentOptions */

NS_IMPL_ISUPPORTS(PaymentOptions, nsIPaymentOptions)

PaymentOptions::PaymentOptions(const bool aRequestPayerName,
                               const bool aRequestPayerEmail,
                               const bool aRequestPayerPhone,
                               const bool aRequestShipping,
                               const bool aRequestBillingAddress,
                               const nsAString& aShippingType)
    : mRequestPayerName(aRequestPayerName),
      mRequestPayerEmail(aRequestPayerEmail),
      mRequestPayerPhone(aRequestPayerPhone),
      mRequestShipping(aRequestShipping),
      mRequestBillingAddress(aRequestBillingAddress),
      mShippingType(aShippingType) {}

nsresult PaymentOptions::Create(const IPCPaymentOptions& aIPCOptions,
                                nsIPaymentOptions** aOptions) {
  NS_ENSURE_ARG_POINTER(aOptions);

  nsCOMPtr<nsIPaymentOptions> options = new PaymentOptions(
      aIPCOptions.requestPayerName(), aIPCOptions.requestPayerEmail(),
      aIPCOptions.requestPayerPhone(), aIPCOptions.requestShipping(),
      aIPCOptions.requestBillingAddress(), aIPCOptions.shippingType());
  options.forget(aOptions);
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestPayerName(bool* aRequestPayerName) {
  NS_ENSURE_ARG_POINTER(aRequestPayerName);
  *aRequestPayerName = mRequestPayerName;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestPayerEmail(bool* aRequestPayerEmail) {
  NS_ENSURE_ARG_POINTER(aRequestPayerEmail);
  *aRequestPayerEmail = mRequestPayerEmail;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestPayerPhone(bool* aRequestPayerPhone) {
  NS_ENSURE_ARG_POINTER(aRequestPayerPhone);
  *aRequestPayerPhone = mRequestPayerPhone;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestShipping(bool* aRequestShipping) {
  NS_ENSURE_ARG_POINTER(aRequestShipping);
  *aRequestShipping = mRequestShipping;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestBillingAddress(bool* aRequestBillingAddress) {
  NS_ENSURE_ARG_POINTER(aRequestBillingAddress);
  *aRequestBillingAddress = mRequestBillingAddress;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetShippingType(nsAString& aShippingType) {
  aShippingType = mShippingType;
  return NS_OK;
}

/* PaymentReqeust */

NS_IMPL_ISUPPORTS(PaymentRequest, nsIPaymentRequest)

PaymentRequest::PaymentRequest(const uint64_t aTopOuterWindowId,
                               const nsAString& aRequestId,
                               nsIPrincipal* aTopLevelPrincipal,
                               nsIArray* aPaymentMethods,
                               nsIPaymentDetails* aPaymentDetails,
                               nsIPaymentOptions* aPaymentOptions,
                               const nsAString& aShippingOption)
    : mTopOuterWindowId(aTopOuterWindowId),
      mRequestId(aRequestId),
      mTopLevelPrincipal(aTopLevelPrincipal),
      mPaymentMethods(aPaymentMethods),
      mPaymentDetails(aPaymentDetails),
      mPaymentOptions(aPaymentOptions),
      mShippingOption(aShippingOption),
      mState(eCreated) {}

NS_IMETHODIMP
PaymentRequest::GetTopOuterWindowId(uint64_t* aTopOuterWindowId) {
  NS_ENSURE_ARG_POINTER(aTopOuterWindowId);
  *aTopOuterWindowId = mTopOuterWindowId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetTopLevelPrincipal(nsIPrincipal** aTopLevelPrincipal) {
  NS_ENSURE_ARG_POINTER(aTopLevelPrincipal);
  MOZ_ASSERT(mTopLevelPrincipal);
  nsCOMPtr<nsIPrincipal> principal = mTopLevelPrincipal;
  principal.forget(aTopLevelPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetRequestId(nsAString& aRequestId) {
  aRequestId = mRequestId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetPaymentMethods(nsIArray** aPaymentMethods) {
  NS_ENSURE_ARG_POINTER(aPaymentMethods);
  MOZ_ASSERT(mPaymentMethods);
  nsCOMPtr<nsIArray> methods = mPaymentMethods;
  methods.forget(aPaymentMethods);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetPaymentDetails(nsIPaymentDetails** aPaymentDetails) {
  NS_ENSURE_ARG_POINTER(aPaymentDetails);
  MOZ_ASSERT(mPaymentDetails);
  nsCOMPtr<nsIPaymentDetails> details = mPaymentDetails;
  details.forget(aPaymentDetails);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetPaymentOptions(nsIPaymentOptions** aPaymentOptions) {
  NS_ENSURE_ARG_POINTER(aPaymentOptions);
  MOZ_ASSERT(mPaymentOptions);
  nsCOMPtr<nsIPaymentOptions> options = mPaymentOptions;
  options.forget(aPaymentOptions);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetShippingOption(nsAString& aShippingOption) {
  aShippingOption = mShippingOption;
  return NS_OK;
}

nsresult PaymentRequest::UpdatePaymentDetails(
    nsIPaymentDetails* aPaymentDetails, const nsAString& aShippingOption) {
  MOZ_ASSERT(aPaymentDetails);
  bool requestShipping;
  nsresult rv = mPaymentOptions->GetRequestShipping(&requestShipping);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mShippingOption = aShippingOption;

  PaymentDetails* rowDetails =
      static_cast<PaymentDetails*>(mPaymentDetails.get());
  MOZ_ASSERT(rowDetails);
  return rowDetails->Update(aPaymentDetails, requestShipping);
}

void PaymentRequest::SetCompleteStatus(const nsAString& aCompleteStatus) {
  mCompleteStatus = aCompleteStatus;
}

nsresult PaymentRequest::UpdateErrors(const nsAString& aError,
                                      const nsAString& aPayerErrors,
                                      const nsAString& aPaymentMethodErrors,
                                      const nsAString& aShippingAddressErrors) {
  PaymentDetails* rowDetails =
      static_cast<PaymentDetails*>(mPaymentDetails.get());
  MOZ_ASSERT(rowDetails);
  return rowDetails->UpdateErrors(aError, aPayerErrors, aPaymentMethodErrors,
                                  aShippingAddressErrors);
}

NS_IMETHODIMP
PaymentRequest::GetCompleteStatus(nsAString& aCompleteStatus) {
  aCompleteStatus = mCompleteStatus;
  return NS_OK;
}

/* PaymentAddress */

NS_IMPL_ISUPPORTS(PaymentAddress, nsIPaymentAddress)

NS_IMETHODIMP
PaymentAddress::Init(const nsAString& aCountry, nsIArray* aAddressLine,
                     const nsAString& aRegion, const nsAString& aRegionCode,
                     const nsAString& aCity,
                     const nsAString& aDependentLocality,
                     const nsAString& aPostalCode,
                     const nsAString& aSortingCode,
                     const nsAString& aOrganization,
                     const nsAString& aRecipient, const nsAString& aPhone) {
  mCountry = aCountry;
  mAddressLine = aAddressLine;
  mRegion = aRegion;
  mRegionCode = aRegionCode;
  mCity = aCity;
  mDependentLocality = aDependentLocality;
  mPostalCode = aPostalCode;
  mSortingCode = aSortingCode;
  mOrganization = aOrganization;
  mRecipient = aRecipient;
  mPhone = aPhone;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetCountry(nsAString& aCountry) {
  aCountry = mCountry;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetAddressLine(nsIArray** aAddressLine) {
  NS_ENSURE_ARG_POINTER(aAddressLine);
  nsCOMPtr<nsIArray> addressLine = mAddressLine;
  addressLine.forget(aAddressLine);
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetRegion(nsAString& aRegion) {
  aRegion = mRegion;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetRegionCode(nsAString& aRegionCode) {
  aRegionCode = mRegionCode;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetCity(nsAString& aCity) {
  aCity = mCity;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetDependentLocality(nsAString& aDependentLocality) {
  aDependentLocality = mDependentLocality;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetPostalCode(nsAString& aPostalCode) {
  aPostalCode = mPostalCode;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetSortingCode(nsAString& aSortingCode) {
  aSortingCode = mSortingCode;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetOrganization(nsAString& aOrganization) {
  aOrganization = mOrganization;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetRecipient(nsAString& aRecipient) {
  aRecipient = mRecipient;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetPhone(nsAString& aPhone) {
  aPhone = mPhone;
  return NS_OK;
}

}  // namespace mozilla::dom::payments
