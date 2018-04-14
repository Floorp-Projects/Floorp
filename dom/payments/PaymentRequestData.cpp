/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsArrayUtils.h"
#include "nsIMutableArray.h"
#include "nsISupportsPrimitives.h"
#include "nsUnicharUtils.h"
#include "PaymentRequestData.h"
#include "PaymentRequestUtils.h"

namespace mozilla {
namespace dom {
namespace payments {

/* PaymentMethodData */

NS_IMPL_ISUPPORTS(PaymentMethodData,
                  nsIPaymentMethodData)

PaymentMethodData::PaymentMethodData(const nsAString& aSupportedMethods,
                                     const nsAString& aData)
  : mSupportedMethods(aSupportedMethods)
  , mData(aData)
{
}

nsresult
PaymentMethodData::Create(const IPCPaymentMethodData& aIPCMethodData,
                          nsIPaymentMethodData** aMethodData)
{
  NS_ENSURE_ARG_POINTER(aMethodData);
  nsCOMPtr<nsIPaymentMethodData> methodData =
    new PaymentMethodData(aIPCMethodData.supportedMethods(), aIPCMethodData.data());
  methodData.forget(aMethodData);
  return NS_OK;
}

NS_IMETHODIMP
PaymentMethodData::GetSupportedMethods(nsAString& aSupportedMethods)
{
  aSupportedMethods = mSupportedMethods;
  return NS_OK;
}

NS_IMETHODIMP
PaymentMethodData::GetData(JSContext* aCx, JS::MutableHandleValue aData)
{
  if (mData.IsEmpty()) {
    aData.set(JS::NullValue());
    return NS_OK;
  }
  nsresult rv = DeserializeToJSValue(mData, aCx ,aData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

/* PaymentCurrencyAmount */

NS_IMPL_ISUPPORTS(PaymentCurrencyAmount,
                  nsIPaymentCurrencyAmount)

PaymentCurrencyAmount::PaymentCurrencyAmount(const nsAString& aCurrency,
                                             const nsAString& aValue)
  : mValue(aValue)
{
  /*
   *  According to the spec
   *  https://w3c.github.io/payment-request/#validity-checkers
   *  Set amount.currency to the result of ASCII uppercasing amount.currency.
   */
  ToUpperCase(aCurrency, mCurrency);
}

nsresult
PaymentCurrencyAmount::Create(const IPCPaymentCurrencyAmount& aIPCAmount,
                              nsIPaymentCurrencyAmount** aAmount)
{
  NS_ENSURE_ARG_POINTER(aAmount);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount =
    new PaymentCurrencyAmount(aIPCAmount.currency(), aIPCAmount.value());
  amount.forget(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
PaymentCurrencyAmount::GetCurrency(nsAString& aCurrency)
{
  aCurrency = mCurrency;
  return NS_OK;
}

NS_IMETHODIMP
PaymentCurrencyAmount::GetValue(nsAString& aValue)
{
  aValue = mValue;
  return NS_OK;
}

/* PaymentItem */

NS_IMPL_ISUPPORTS(PaymentItem,
                  nsIPaymentItem)

PaymentItem::PaymentItem(const nsAString& aLabel,
                         nsIPaymentCurrencyAmount* aAmount,
                         const bool aPending,
                         const nsAString& aType)
  : mLabel(aLabel)
  , mAmount(aAmount)
  , mPending(aPending)
  , mType(aType)
{
}

nsresult
PaymentItem::Create(const IPCPaymentItem& aIPCItem, nsIPaymentItem** aItem)
{
  NS_ENSURE_ARG_POINTER(aItem);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount;
  nsresult rv = PaymentCurrencyAmount::Create(aIPCItem.amount(),
                                              getter_AddRefs(amount));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCOMPtr<nsIPaymentItem> item =
    new PaymentItem(aIPCItem.label(), amount, aIPCItem.pending(), aIPCItem.type());
  item.forget(aItem);
  return NS_OK;
}

NS_IMETHODIMP
PaymentItem::GetLabel(nsAString& aLabel)
{
  aLabel = mLabel;
  return NS_OK;
}

NS_IMETHODIMP
PaymentItem::GetAmount(nsIPaymentCurrencyAmount** aAmount)
{
  NS_ENSURE_ARG_POINTER(aAmount);
  MOZ_ASSERT(mAmount);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount = mAmount;
  amount.forget(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
PaymentItem::GetPending(bool* aPending)
{
  NS_ENSURE_ARG_POINTER(aPending);
  *aPending = mPending;
  return NS_OK;
}

NS_IMETHODIMP
PaymentItem::GetType(nsAString& aType)
{
  aType = mType;
  return NS_OK;
}

/* PaymentDetailsModifier */

NS_IMPL_ISUPPORTS(PaymentDetailsModifier,
                  nsIPaymentDetailsModifier)

PaymentDetailsModifier::PaymentDetailsModifier(const nsAString& aSupportedMethods,
                                               nsIPaymentItem* aTotal,
                                               nsIArray* aAdditionalDisplayItems,
                                               const nsAString& aData)
  : mSupportedMethods(aSupportedMethods)
  , mTotal(aTotal)
  , mAdditionalDisplayItems(aAdditionalDisplayItems)
  , mData(aData)
{
}

nsresult
PaymentDetailsModifier::Create(const IPCPaymentDetailsModifier& aIPCModifier,
                               nsIPaymentDetailsModifier** aModifier)
{
  NS_ENSURE_ARG_POINTER(aModifier);
  nsCOMPtr<nsIPaymentItem> total;
  nsresult rv = PaymentItem::Create(aIPCModifier.total(), getter_AddRefs(total));
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
    displayItems = items.forget();
  }
  nsCOMPtr<nsIPaymentDetailsModifier> modifier =
    new PaymentDetailsModifier(aIPCModifier.supportedMethods(),
                               total,
                               displayItems,
                               aIPCModifier.data());
  modifier.forget(aModifier);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetailsModifier::GetSupportedMethods(nsAString& aSupportedMethods)
{
  aSupportedMethods = mSupportedMethods;
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetailsModifier::GetTotal(nsIPaymentItem** aTotal)
{
  NS_ENSURE_ARG_POINTER(aTotal);
  MOZ_ASSERT(mTotal);
  nsCOMPtr<nsIPaymentItem> total = mTotal;
  total.forget(aTotal);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetailsModifier::GetAdditionalDisplayItems(nsIArray** aAdditionalDisplayItems)
{
  NS_ENSURE_ARG_POINTER(aAdditionalDisplayItems);
  nsCOMPtr<nsIArray> additionalItems = mAdditionalDisplayItems;
  additionalItems.forget(aAdditionalDisplayItems);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetailsModifier::GetData(JSContext* aCx, JS::MutableHandleValue aData)
{
  if (mData.IsEmpty()) {
    aData.set(JS::NullValue());
    return NS_OK;
  }
  nsresult rv = DeserializeToJSValue(mData, aCx ,aData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  return NS_OK;
}

/* PaymentShippingOption */

NS_IMPL_ISUPPORTS(PaymentShippingOption,
                  nsIPaymentShippingOption)

PaymentShippingOption::PaymentShippingOption(const nsAString& aId,
                                             const nsAString& aLabel,
                                             nsIPaymentCurrencyAmount* aAmount,
                                             const bool aSelected)
  : mId(aId)
  , mLabel(aLabel)
  , mAmount(aAmount)
  , mSelected(aSelected)
{
}

nsresult
PaymentShippingOption::Create(const IPCPaymentShippingOption& aIPCOption,
                              nsIPaymentShippingOption** aOption)
{
  NS_ENSURE_ARG_POINTER(aOption);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount;
  nsresult rv = PaymentCurrencyAmount::Create(aIPCOption.amount(), getter_AddRefs(amount));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  nsCOMPtr<nsIPaymentShippingOption> option =
    new PaymentShippingOption(aIPCOption.id(), aIPCOption.label(), amount, aIPCOption.selected());
  option.forget(aOption);
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::GetId(nsAString& aId)
{
  aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::GetLabel(nsAString& aLabel)
{
  aLabel = mLabel;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::GetAmount(nsIPaymentCurrencyAmount** aAmount)
{
  NS_ENSURE_ARG_POINTER(aAmount);
  MOZ_ASSERT(mAmount);
  nsCOMPtr<nsIPaymentCurrencyAmount> amount = mAmount;
  amount.forget(aAmount);
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::GetSelected(bool* aSelected)
{
  NS_ENSURE_ARG_POINTER(aSelected);
  *aSelected = mSelected;
  return NS_OK;
}

NS_IMETHODIMP
PaymentShippingOption::SetSelected(bool aSelected)
{
  mSelected = aSelected;
  return NS_OK;
}

/* PaymentDetails */

NS_IMPL_ISUPPORTS(PaymentDetails,
                  nsIPaymentDetails)

PaymentDetails::PaymentDetails(const nsAString& aId,
                               nsIPaymentItem* aTotalItem,
                               nsIArray* aDisplayItems,
                               nsIArray* aShippingOptions,
                               nsIArray* aModifiers,
                               const nsAString& aError)
  : mId(aId)
  , mTotalItem(aTotalItem)
  , mDisplayItems(aDisplayItems)
  , mShippingOptions(aShippingOptions)
  , mModifiers(aModifiers)
  , mError(aError)
{
}

nsresult
PaymentDetails::Create(const IPCPaymentDetails& aIPCDetails,
                       nsIPaymentDetails** aDetails)
{
  NS_ENSURE_ARG_POINTER(aDetails);

  nsCOMPtr<nsIPaymentItem> total;
  nsresult rv = PaymentItem::Create(aIPCDetails.total(), getter_AddRefs(total));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIArray> displayItems;
  if (aIPCDetails.displayItemsPassed()) {
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
    displayItems = items.forget();
  }

  nsCOMPtr<nsIArray> shippingOptions;
  if (aIPCDetails.shippingOptionsPassed()) {
    nsCOMPtr<nsIMutableArray> options = do_CreateInstance(NS_ARRAY_CONTRACTID);
    MOZ_ASSERT(options);
    for (const IPCPaymentShippingOption& shippingOption : aIPCDetails.shippingOptions()) {
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
    shippingOptions = options.forget();
  }

  nsCOMPtr<nsIArray> modifiers;
  if (aIPCDetails.modifiersPassed()) {
    nsCOMPtr<nsIMutableArray> detailsModifiers = do_CreateInstance(NS_ARRAY_CONTRACTID);
    MOZ_ASSERT(detailsModifiers);
    for (const IPCPaymentDetailsModifier& modifier : aIPCDetails.modifiers()) {
      nsCOMPtr<nsIPaymentDetailsModifier> detailsModifier;
      rv = PaymentDetailsModifier::Create(modifier, getter_AddRefs(detailsModifier));
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      rv = detailsModifiers->AppendElement(detailsModifier);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    modifiers = detailsModifiers.forget();
  }

  nsCOMPtr<nsIPaymentDetails> details =
    new PaymentDetails(aIPCDetails.id(), total, displayItems, shippingOptions,
                       modifiers, aIPCDetails.error());

  details.forget(aDetails);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetId(nsAString& aId)
{
  aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetTotalItem(nsIPaymentItem** aTotalItem)
{
  NS_ENSURE_ARG_POINTER(aTotalItem);
  MOZ_ASSERT(mTotalItem);
  nsCOMPtr<nsIPaymentItem> total = mTotalItem;
  total.forget(aTotalItem);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetDisplayItems(nsIArray** aDisplayItems)
{
  NS_ENSURE_ARG_POINTER(aDisplayItems);
  nsCOMPtr<nsIArray> displayItems = mDisplayItems;
  displayItems.forget(aDisplayItems);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetShippingOptions(nsIArray** aShippingOptions)
{
  NS_ENSURE_ARG_POINTER(aShippingOptions);
  nsCOMPtr<nsIArray> options = mShippingOptions;
  options.forget(aShippingOptions);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetModifiers(nsIArray** aModifiers)
{
  NS_ENSURE_ARG_POINTER(aModifiers);
  nsCOMPtr<nsIArray> modifiers = mModifiers;
  modifiers.forget(aModifiers);
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::GetError(nsAString& aError)
{
  aError = mError;
  return NS_OK;
}

NS_IMETHODIMP
PaymentDetails::Update(nsIPaymentDetails* aDetails, const bool aRequestShipping)
{
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
  return NS_OK;

}
/* PaymentOptions */

NS_IMPL_ISUPPORTS(PaymentOptions,
                  nsIPaymentOptions)

PaymentOptions::PaymentOptions(const bool aRequestPayerName,
                               const bool aRequestPayerEmail,
                               const bool aRequestPayerPhone,
                               const bool aRequestShipping,
                               const nsAString& aShippingType)
  : mRequestPayerName(aRequestPayerName)
  , mRequestPayerEmail(aRequestPayerEmail)
  , mRequestPayerPhone(aRequestPayerPhone)
  , mRequestShipping(aRequestShipping)
  , mShippingType(aShippingType)
{
}

nsresult
PaymentOptions::Create(const IPCPaymentOptions& aIPCOptions,
                       nsIPaymentOptions** aOptions)
{
  NS_ENSURE_ARG_POINTER(aOptions);

  nsCOMPtr<nsIPaymentOptions> options =
    new PaymentOptions(aIPCOptions.requestPayerName(),
                       aIPCOptions.requestPayerEmail(),
                       aIPCOptions.requestPayerPhone(),
                       aIPCOptions.requestShipping(),
                       aIPCOptions.shippingType());
  options.forget(aOptions);
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestPayerName(bool* aRequestPayerName)
{
  NS_ENSURE_ARG_POINTER(aRequestPayerName);
  *aRequestPayerName = mRequestPayerName;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestPayerEmail(bool* aRequestPayerEmail)
{
  NS_ENSURE_ARG_POINTER(aRequestPayerEmail);
  *aRequestPayerEmail = mRequestPayerEmail;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestPayerPhone(bool* aRequestPayerPhone)
{
  NS_ENSURE_ARG_POINTER(aRequestPayerPhone);
  *aRequestPayerPhone = mRequestPayerPhone;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetRequestShipping(bool* aRequestShipping)
{
  NS_ENSURE_ARG_POINTER(aRequestShipping);
  *aRequestShipping = mRequestShipping;
  return NS_OK;
}

NS_IMETHODIMP
PaymentOptions::GetShippingType(nsAString& aShippingType)
{
  aShippingType = mShippingType;
  return NS_OK;
}

/* PaymentReqeust */

NS_IMPL_ISUPPORTS(PaymentRequest,
                  nsIPaymentRequest)

PaymentRequest::PaymentRequest(const uint64_t aTabId,
                               const nsAString& aRequestId,
                               nsIPrincipal* aTopLevelPrincipal,
                               nsIArray* aPaymentMethods,
                               nsIPaymentDetails* aPaymentDetails,
                               nsIPaymentOptions* aPaymentOptions,
			       const nsAString& aShippingOption)
  : mTabId(aTabId)
  , mRequestId(aRequestId)
  , mTopLevelPrincipal(aTopLevelPrincipal)
  , mPaymentMethods(aPaymentMethods)
  , mPaymentDetails(aPaymentDetails)
  , mPaymentOptions(aPaymentOptions)
  , mShippingOption(aShippingOption)
{
}

NS_IMETHODIMP
PaymentRequest::GetTabId(uint64_t* aTabId)
{
  NS_ENSURE_ARG_POINTER(aTabId);
  *aTabId = mTabId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetTopLevelPrincipal(nsIPrincipal** aTopLevelPrincipal)
{
  NS_ENSURE_ARG_POINTER(aTopLevelPrincipal);
  MOZ_ASSERT(mTopLevelPrincipal);
  nsCOMPtr<nsIPrincipal> principal = mTopLevelPrincipal;
  principal.forget(aTopLevelPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetRequestId(nsAString& aRequestId)
{
  aRequestId = mRequestId;
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetPaymentMethods(nsIArray** aPaymentMethods)
{
  NS_ENSURE_ARG_POINTER(aPaymentMethods);
  MOZ_ASSERT(mPaymentMethods);
  nsCOMPtr<nsIArray> methods = mPaymentMethods;
  methods.forget(aPaymentMethods);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetPaymentDetails(nsIPaymentDetails** aPaymentDetails)
{
  NS_ENSURE_ARG_POINTER(aPaymentDetails);
  MOZ_ASSERT(mPaymentDetails);
  nsCOMPtr<nsIPaymentDetails> details = mPaymentDetails;
  details.forget(aPaymentDetails);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetPaymentOptions(nsIPaymentOptions** aPaymentOptions)
{
  NS_ENSURE_ARG_POINTER(aPaymentOptions);
  MOZ_ASSERT(mPaymentOptions);
  nsCOMPtr<nsIPaymentOptions> options = mPaymentOptions;
  options.forget(aPaymentOptions);
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetShippingOption(nsAString& aShippingOption)
{
  aShippingOption = mShippingOption;
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::UpdatePaymentDetails(nsIPaymentDetails* aPaymentDetails,
                                     const nsAString& aShippingOption)
{
  MOZ_ASSERT(aPaymentDetails);
  bool requestShipping;
  nsresult rv = mPaymentOptions->GetRequestShipping(&requestShipping);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mShippingOption = aShippingOption;
  return mPaymentDetails->Update(aPaymentDetails, requestShipping);
}

NS_IMETHODIMP
PaymentRequest::SetCompleteStatus(const nsAString& aCompleteStatus)
{
  mCompleteStatus = aCompleteStatus;
  return NS_OK;
}

NS_IMETHODIMP
PaymentRequest::GetCompleteStatus(nsAString& aCompleteStatus)
{
  aCompleteStatus = mCompleteStatus;
  return NS_OK;
}

/* PaymentAddress */

NS_IMPL_ISUPPORTS(PaymentAddress, nsIPaymentAddress)

NS_IMETHODIMP
PaymentAddress::Init(const nsAString& aCountry,
                     nsIArray* aAddressLine,
                     const nsAString& aRegion,
                     const nsAString& aCity,
                     const nsAString& aDependentLocality,
                     const nsAString& aPostalCode,
                     const nsAString& aSortingCode,
                     const nsAString& aLanguageCode,
                     const nsAString& aOrganization,
                     const nsAString& aRecipient,
                     const nsAString& aPhone)
{
  mCountry = aCountry;
  mAddressLine = aAddressLine;
  mRegion = aRegion;
  mCity = aCity;
  mDependentLocality = aDependentLocality;
  mPostalCode = aPostalCode;
  mSortingCode = aSortingCode;
  mLanguageCode = aLanguageCode;
  mOrganization = aOrganization;
  mRecipient = aRecipient;
  mPhone = aPhone;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetCountry(nsAString& aCountry)
{
  aCountry = mCountry;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetAddressLine(nsIArray** aAddressLine)
{
  NS_ENSURE_ARG_POINTER(aAddressLine);
  nsCOMPtr<nsIArray> addressLine = mAddressLine;
  addressLine.forget(aAddressLine);
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetRegion(nsAString& aRegion)
{
  aRegion = mRegion;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetCity(nsAString& aCity)
{
  aCity = mCity;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetDependentLocality(nsAString& aDependentLocality)
{
  aDependentLocality = mDependentLocality;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetPostalCode(nsAString& aPostalCode)
{
  aPostalCode = mPostalCode;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetSortingCode(nsAString& aSortingCode)
{
  aSortingCode = mSortingCode;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetLanguageCode(nsAString& aLanguageCode)
{
  aLanguageCode = mLanguageCode;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetOrganization(nsAString& aOrganization)
{
  aOrganization = mOrganization;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetRecipient(nsAString& aRecipient)
{
  aRecipient = mRecipient;
  return NS_OK;
}

NS_IMETHODIMP
PaymentAddress::GetPhone(nsAString& aPhone)
{
  aPhone = mPhone;
  return NS_OK;
}

} // end of namespace payment
} // end of namespace dom
} // end of namespace mozilla
