/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaymentRequestManager.h"
#include "PaymentRequestUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "nsContentUtils.h"
#include "nsString.h"

namespace mozilla {
namespace dom {
namespace {

/*
 *  Following Convert* functions are used for convert PaymentRequest structs
 *  to transferable structs for IPC.
 */
nsresult
ConvertMethodData(const PaymentMethodData& aMethodData,
                  IPCPaymentMethodData& aIPCMethodData)
{
  // Convert JSObject to a serialized string
  nsAutoString serializedData;
  if (aMethodData.mData.WasPassed()) {
    JSContext* cx = nsContentUtils::GetCurrentJSContext();
    MOZ_ASSERT(cx);
    JS::RootedObject object(cx, aMethodData.mData.Value());
    nsresult rv = SerializeFromJSObject(cx, object, serializedData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  aIPCMethodData = IPCPaymentMethodData(aMethodData.mSupportedMethods, serializedData);
  return NS_OK;
}

void
ConvertCurrencyAmount(const PaymentCurrencyAmount& aAmount,
                      IPCPaymentCurrencyAmount& aIPCCurrencyAmount)
{
  aIPCCurrencyAmount = IPCPaymentCurrencyAmount(aAmount.mCurrency, aAmount.mValue);
}

void
ConvertItem(const PaymentItem& aItem, IPCPaymentItem& aIPCItem)
{
  IPCPaymentCurrencyAmount amount;
  ConvertCurrencyAmount(aItem.mAmount, amount);
  aIPCItem = IPCPaymentItem(aItem.mLabel, amount, aItem.mPending);
}

nsresult
ConvertModifier(const PaymentDetailsModifier& aModifier,
                IPCPaymentDetailsModifier& aIPCModifier)
{
  // Convert JSObject to a serialized string
  nsAutoString serializedData;
  if (aModifier.mData.WasPassed()) {
    JSContext* cx = nsContentUtils::GetCurrentJSContext();
    MOZ_ASSERT(cx);
    JS::RootedObject object(cx, aModifier.mData.Value());
    nsresult rv = SerializeFromJSObject(cx, object, serializedData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  IPCPaymentItem total;
  ConvertItem(aModifier.mTotal, total);

  nsTArray<IPCPaymentItem> additionalDisplayItems;
  if (aModifier.mAdditionalDisplayItems.WasPassed()) {
    for (const PaymentItem& item : aModifier.mAdditionalDisplayItems.Value()) {
      IPCPaymentItem displayItem;
      ConvertItem(item, displayItem);
      additionalDisplayItems.AppendElement(displayItem);
    }
  }
  aIPCModifier = IPCPaymentDetailsModifier(aModifier.mSupportedMethods,
                                           total,
                                           additionalDisplayItems,
                                           serializedData,
                                           aModifier.mAdditionalDisplayItems.WasPassed());
  return NS_OK;
}

void
ConvertShippingOption(const PaymentShippingOption& aOption,
                      IPCPaymentShippingOption& aIPCOption)
{
  IPCPaymentCurrencyAmount amount;
  ConvertCurrencyAmount(aOption.mAmount, amount);
  aIPCOption = IPCPaymentShippingOption(aOption.mId, aOption.mLabel, amount, aOption.mSelected);
}

nsresult
ConvertDetailsBase(const PaymentDetailsBase& aDetails,
                   nsTArray<IPCPaymentItem>& aDisplayItems,
                   nsTArray<IPCPaymentShippingOption>& aShippingOptions,
                   nsTArray<IPCPaymentDetailsModifier>& aModifiers,
                   bool aResetShippingOptions)
{
  if (aDetails.mDisplayItems.WasPassed()) {
    for (const PaymentItem& item : aDetails.mDisplayItems.Value()) {
      IPCPaymentItem displayItem;
      ConvertItem(item, displayItem);
      aDisplayItems.AppendElement(displayItem);
    }
  }
  if (aDetails.mShippingOptions.WasPassed() && !aResetShippingOptions) {
    for (const PaymentShippingOption& option : aDetails.mShippingOptions.Value()) {
      IPCPaymentShippingOption shippingOption;
      ConvertShippingOption(option, shippingOption);
      aShippingOptions.AppendElement(shippingOption);
    }
  }
  if (aDetails.mModifiers.WasPassed()) {
    for (const PaymentDetailsModifier& modifier : aDetails.mModifiers.Value()) {
      IPCPaymentDetailsModifier detailsModifier;
      nsresult rv = ConvertModifier(modifier, detailsModifier);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      aModifiers.AppendElement(detailsModifier);
    }
  }
  return NS_OK;
}

nsresult
ConvertDetailsInit(const PaymentDetailsInit& aDetails,
                   IPCPaymentDetails& aIPCDetails,
                   bool aResetShippingOptions)
{
  // Convert PaymentDetailsBase members
  nsTArray<IPCPaymentItem> displayItems;
  nsTArray<IPCPaymentShippingOption> shippingOptions;
  nsTArray<IPCPaymentDetailsModifier> modifiers;
  nsresult rv = ConvertDetailsBase(aDetails, displayItems, shippingOptions, modifiers, aResetShippingOptions);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Convert |id|
  nsString id(EmptyString());
  if (aDetails.mId.WasPassed()) {
    id = aDetails.mId.Value();
  }

  // Convert required |total|
  IPCPaymentItem total;
  ConvertItem(aDetails.mTotal, total);

  aIPCDetails = IPCPaymentDetails(id,
                                  total,
                                  displayItems,
                                  shippingOptions,
                                  modifiers,
                                  EmptyString(), // error message
                                  aDetails.mDisplayItems.WasPassed(),
                                  aDetails.mShippingOptions.WasPassed(),
                                  aDetails.mModifiers.WasPassed());
  return NS_OK;
}

nsresult
ConvertDetailsUpdate(const PaymentDetailsUpdate& aDetails,
                     IPCPaymentDetails& aIPCDetails)
{
  // Convert PaymentDetailsBase members
  nsTArray<IPCPaymentItem> displayItems;
  nsTArray<IPCPaymentShippingOption> shippingOptions;
  nsTArray<IPCPaymentDetailsModifier> modifiers;
  // [TODO] Populate a boolean flag as aResetShippingOptions based on the
  // result of processing details.shippingOptions in UpdatePayment method.
  nsresult rv = ConvertDetailsBase(
    aDetails, displayItems, shippingOptions, modifiers, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // Convert required |total|
  IPCPaymentItem total;
  ConvertItem(aDetails.mTotal, total);

  // Convert |error|
  nsString error(EmptyString());
  if (aDetails.mError.WasPassed()) {
    error = aDetails.mError.Value();
  }

  aIPCDetails = IPCPaymentDetails(EmptyString(), // id
                                  total,
                                  displayItems,
                                  shippingOptions,
                                  modifiers,
                                  error,
                                  aDetails.mDisplayItems.WasPassed(),
                                  aDetails.mShippingOptions.WasPassed(),
                                  aDetails.mModifiers.WasPassed());
  return NS_OK;
}

void
ConvertOptions(const PaymentOptions& aOptions,
               IPCPaymentOptions& aIPCOption)
{
  uint8_t shippingTypeIndex = static_cast<uint8_t>(aOptions.mShippingType);
  nsString shippingType(NS_LITERAL_STRING("shipping"));
  if (shippingTypeIndex < ArrayLength(PaymentShippingTypeValues::strings)) {
    shippingType.AssignASCII(
      PaymentShippingTypeValues::strings[shippingTypeIndex].value);
  }
  aIPCOption = IPCPaymentOptions(aOptions.mRequestPayerName,
                                 aOptions.mRequestPayerEmail,
                                 aOptions.mRequestPayerPhone,
                                 aOptions.mRequestShipping,
                                 shippingType);
}
} // end of namespace

/* PaymentRequestManager */

StaticRefPtr<PaymentRequestManager> gPaymentManager;

nsresult
PaymentRequestManager::GetPaymentChild(PaymentRequest* aRequest,
                                       PaymentRequestChild** aChild)
{
  NS_ENSURE_ARG_POINTER(aRequest);
  NS_ENSURE_ARG_POINTER(aChild);
  *aChild = nullptr;

  RefPtr<PaymentRequestChild> paymentChild;
  if (mPaymentChildHash.Get(aRequest, getter_AddRefs(paymentChild))) {
    paymentChild.forget(aChild);
    return NS_OK;
  }

  nsPIDOMWindowInner* win = aRequest->GetOwner();
  NS_ENSURE_TRUE(win, NS_ERROR_FAILURE);
  TabChild* tabChild = TabChild::GetFrom(win->GetDocShell());
  NS_ENSURE_TRUE(tabChild, NS_ERROR_FAILURE);
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);

  // Only one payment request can interact with user at the same time.
  // Before we create a new PaymentRequestChild, make sure there is no other
  // payment request are interacting on the same tab.
  for (auto iter = mPaymentChildHash.ConstIter(); !iter.Done(); iter.Next()) {
    RefPtr<PaymentRequest> request = iter.Key();
    if (request->Equals(requestId)) {
      continue;
    }
    nsPIDOMWindowInner* requestOwner = request->GetOwner();
    NS_ENSURE_TRUE(requestOwner, NS_ERROR_FAILURE);
    TabChild* tmpChild = TabChild::GetFrom(requestOwner->GetDocShell());
    NS_ENSURE_TRUE(tmpChild, NS_ERROR_FAILURE);
    if (tmpChild->GetTabId() == tabChild->GetTabId()) {
      return NS_ERROR_FAILURE;
    }
  }

  paymentChild = new PaymentRequestChild();
  tabChild->SendPPaymentRequestConstructor(paymentChild);
  if (!mPaymentChildHash.Put(aRequest, paymentChild, mozilla::fallible) ) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  paymentChild.forget(aChild);
  return NS_OK;
}

nsresult
PaymentRequestManager::ReleasePaymentChild(PaymentRequestChild* aPaymentChild)
{
  NS_ENSURE_ARG_POINTER(aPaymentChild);
  for (auto iter = mPaymentChildHash.Iter(); !iter.Done(); iter.Next()) {
    RefPtr<PaymentRequestChild> child = iter.Data();
    if (NS_WARN_IF(!child)) {
      return NS_ERROR_FAILURE;
    }
    if (child == aPaymentChild) {
      iter.Remove();
      return NS_OK;
    }
  }
  return NS_OK;
}

nsresult
PaymentRequestManager::ReleasePaymentChild(PaymentRequest* aRequest)
{
  NS_ENSURE_ARG_POINTER(aRequest);

  RefPtr<PaymentRequestChild> paymentChild;
  if(!mPaymentChildHash.Remove(aRequest, getter_AddRefs(paymentChild))) {
    return NS_ERROR_FAILURE;
  }
  if (NS_WARN_IF(!paymentChild)) {
    return NS_ERROR_FAILURE;
  }
  paymentChild->MaybeDelete();
  return NS_OK;
}

nsresult
PaymentRequestManager::SendRequestPayment(PaymentRequest* aRequest,
                                          const IPCPaymentActionRequest& aAction,
                                          bool aReleaseAfterSend)
{
  RefPtr<PaymentRequestChild> requestChild;
  nsresult rv = GetPaymentChild(aRequest, getter_AddRefs(requestChild));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = requestChild->RequestPayment(aAction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aReleaseAfterSend) {
    rv = ReleasePaymentChild(aRequest);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  return NS_OK;
}

already_AddRefed<PaymentRequestManager>
PaymentRequestManager::GetSingleton()
{
  if (!gPaymentManager) {
    gPaymentManager = new PaymentRequestManager();
    ClearOnShutdown(&gPaymentManager);
  }
  RefPtr<PaymentRequestManager> manager = gPaymentManager;
  return manager.forget();
}

already_AddRefed<PaymentRequest>
PaymentRequestManager::GetPaymentRequestById(const nsAString& aRequestId)
{
  for (const RefPtr<PaymentRequest>& request : mRequestQueue) {
    if (request->Equals(aRequestId)) {
      RefPtr<PaymentRequest> paymentRequest = request;
      return paymentRequest.forget();
    }
  }
  return nullptr;
}

void
GetSelectedShippingOption(const PaymentDetailsInit& aDetails,
                          nsAString& aOption,
                          bool* aResetOptions)
{
  SetDOMStringToNull(aOption);
  if (!aDetails.mShippingOptions.WasPassed()) {
    return;
  }

  nsTArray<nsString> seenIDs;
  const Sequence<PaymentShippingOption>& shippingOptions =
    aDetails.mShippingOptions.Value();
  for (const PaymentShippingOption& shippingOption : shippingOptions) {
    // If there are duplicate IDs present in the shippingOptions, reset aOption
    // to null and set resetOptions flag to reset details.shippingOptions later
    // when converting to IPC structure.
    if (seenIDs.Contains(shippingOption.mId)) {
      SetDOMStringToNull(aOption);
      *aResetOptions = true;
      return;
    }
    seenIDs.AppendElement(shippingOption.mId);

    // set aOption to last selected option's ID
    if (shippingOption.mSelected) {
      aOption = shippingOption.mId;
    }
  }
}

nsresult
PaymentRequestManager::CreatePayment(nsPIDOMWindowInner* aWindow,
                                     const Sequence<PaymentMethodData>& aMethodData,
                                     const PaymentDetailsInit& aDetails,
                                     const PaymentOptions& aOptions,
                                     PaymentRequest** aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aRequest);
  *aRequest = nullptr;
  nsresult rv;

  RefPtr<PaymentRequest> request = PaymentRequest::CreatePaymentRequest(aWindow, rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  /*
   *  Set request's |mId| to details.id if details.id exists.
   *  Otherwise, set |mId| to internal id.
   */
  nsAutoString requestId;
  if (aDetails.mId.WasPassed() && !aDetails.mId.Value().IsEmpty()) {
    requestId = aDetails.mId.Value();
  } else {
    request->GetInternalId(requestId);
  }
  request->SetId(requestId);

  /*
   *  Set request's mShippingOption to last selected option's ID if
   *  details.shippingOptions exists and IDs of all options are unique.
   *  Otherwise, set mShippingOption to null and set the resetShippingOptions
   *  flag to reset details.shippingOptions to an empty array later when
   *  converting details to IPC structure.
   */
  nsAutoString shippingOption;
  bool resetShippingOptions = false;
  GetSelectedShippingOption(aDetails, shippingOption, &resetShippingOptions);
  request->SetShippingOption(shippingOption);

  /*
   * Set request's |mShippingType| if shipping is required.
   */
  if (aOptions.mRequestShipping) {
    request->SetShippingType(
        Nullable<PaymentShippingType>(aOptions.mShippingType));
  }

  nsAutoString internalId;
  request->GetInternalId(internalId);

  nsTArray<IPCPaymentMethodData> methodData;
  for (const PaymentMethodData& data : aMethodData) {
    IPCPaymentMethodData ipcMethodData;
    rv = ConvertMethodData(data, ipcMethodData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    methodData.AppendElement(ipcMethodData);
  }

  IPCPaymentDetails details;
  rv = ConvertDetailsInit(aDetails, details, resetShippingOptions);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  IPCPaymentOptions options;
  ConvertOptions(aOptions, options);

  IPCPaymentCreateActionRequest action(internalId,
                                       methodData,
                                       details,
                                       options);

  rv = SendRequestPayment(request, action, true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  mRequestQueue.AppendElement(request);
  request.forget(aRequest);
  return NS_OK;
}

nsresult
PaymentRequestManager::CanMakePayment(const nsAString& aRequestId)
{
  RefPtr<PaymentRequest> request = GetPaymentRequestById(aRequestId);
  if (!request) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString requestId(aRequestId);
  IPCPaymentCanMakeActionRequest action(requestId);

  return SendRequestPayment(request, action);
}

nsresult
PaymentRequestManager::ShowPayment(const nsAString& aRequestId)
{
  if (mShowingRequest) {
    return NS_ERROR_ABORT;
  }
  RefPtr<PaymentRequest> request = GetPaymentRequestById(aRequestId);
  if (!request) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString requestId(aRequestId);
  IPCPaymentShowActionRequest action(requestId);
  nsresult rv = SendRequestPayment(request, action);
  mShowingRequest = request;
  return rv;
}

nsresult
PaymentRequestManager::AbortPayment(const nsAString& aRequestId)
{
  RefPtr<PaymentRequest> request = GetPaymentRequestById(aRequestId);
  if (!request) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString requestId(aRequestId);
  IPCPaymentAbortActionRequest action(requestId);

  return SendRequestPayment(request, action);
}

nsresult
PaymentRequestManager::CompletePayment(const nsAString& aRequestId,
                                       const PaymentComplete& aComplete)
{
  RefPtr<PaymentRequest> request = GetPaymentRequestById(aRequestId);
  if (!request) {
    return NS_ERROR_FAILURE;
  }

  nsString completeStatusString(NS_LITERAL_STRING("unknown"));
  uint8_t completeIndex = static_cast<uint8_t>(aComplete);
  if (completeIndex < ArrayLength(PaymentCompleteValues::strings)) {
    completeStatusString.AssignASCII(
      PaymentCompleteValues::strings[completeIndex].value);
  }

  nsAutoString requestId(aRequestId);
  IPCPaymentCompleteActionRequest action(requestId, completeStatusString);

  return SendRequestPayment(request, action);
}

nsresult
PaymentRequestManager::UpdatePayment(const nsAString& aRequestId,
                                     const PaymentDetailsUpdate& aDetails)
{
  RefPtr<PaymentRequest> request = GetPaymentRequestById(aRequestId);
  if (!request) {
    return NS_ERROR_UNEXPECTED;
  }

  // [TODO] Process details.shippingOptions if presented.
  //        1) Check if there are duplicate IDs in details.shippingOptions,
  //           if so, reset details.shippingOptions to an empty sequence.
  //        2) Set request's selectedShippingOption to the ID of last selected
  //           option.

  IPCPaymentDetails details;
  nsresult rv = ConvertDetailsUpdate(aDetails, details);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString requestId(aRequestId);
  IPCPaymentUpdateActionRequest action(requestId, details);
  return SendRequestPayment(request, action);
}

nsresult
PaymentRequestManager::RespondPayment(const IPCPaymentActionResponse& aResponse)
{
  switch (aResponse.type()) {
    case IPCPaymentActionResponse::TIPCPaymentCanMakeActionResponse: {
      const IPCPaymentCanMakeActionResponse& response = aResponse;
      RefPtr<PaymentRequest> request = GetPaymentRequestById(response.requestId());
      if (NS_WARN_IF(!request)) {
        return NS_ERROR_FAILURE;
      }
      request->RespondCanMakePayment(response.result());
      nsresult rv = ReleasePaymentChild(request);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    case IPCPaymentActionResponse::TIPCPaymentShowActionResponse: {
      const IPCPaymentShowActionResponse& response = aResponse;
      RefPtr<PaymentRequest> request = GetPaymentRequestById(response.requestId());
      if (NS_WARN_IF(!request)) {
        return NS_ERROR_FAILURE;
      }
      request->RespondShowPayment(response.isAccepted(),
                                  response.methodName(),
                                  response.data(),
                                  response.payerName(),
                                  response.payerEmail(),
                                  response.payerPhone());
      if (!response.isAccepted()) {
        MOZ_ASSERT(mShowingRequest == request);
        mShowingRequest = nullptr;
        mRequestQueue.RemoveElement(request);
        nsresult rv = ReleasePaymentChild(request);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      break;
    }
    case IPCPaymentActionResponse::TIPCPaymentAbortActionResponse: {
      const IPCPaymentAbortActionResponse& response = aResponse;
      RefPtr<PaymentRequest> request = GetPaymentRequestById(response.requestId());
      if (NS_WARN_IF(!request)) {
        return NS_ERROR_FAILURE;
      }
      request->RespondAbortPayment(response.isSucceeded());
      if (response.isSucceeded()) {
        MOZ_ASSERT(mShowingRequest == request);
        mShowingRequest = nullptr;
        mRequestQueue.RemoveElement(request);
        nsresult rv = ReleasePaymentChild(request);
        if (NS_WARN_IF(NS_FAILED(rv))) {
          return rv;
        }
      }
      break;
    }
    case IPCPaymentActionResponse::TIPCPaymentCompleteActionResponse: {
      const IPCPaymentCompleteActionResponse& response = aResponse;
      RefPtr<PaymentRequest> request = GetPaymentRequestById(response.requestId());
      if (NS_WARN_IF(!request)) {
        return NS_ERROR_FAILURE;
      }
      request->RespondComplete();
      MOZ_ASSERT(mShowingRequest == request);
      mShowingRequest = nullptr;
      mRequestQueue.RemoveElement(request);
      nsresult rv = ReleasePaymentChild(request);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      break;
    }
    default: {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsresult
PaymentRequestManager::ChangeShippingAddress(const nsAString& aRequestId,
                                             const IPCPaymentAddress& aAddress)
{
  RefPtr<PaymentRequest> request = GetPaymentRequestById(aRequestId);
  if (NS_WARN_IF(!request)) {
    return NS_ERROR_FAILURE;
  }
  return request->UpdateShippingAddress(aAddress.country(),
                                        aAddress.addressLine(),
                                        aAddress.region(),
                                        aAddress.city(),
                                        aAddress.dependentLocality(),
                                        aAddress.postalCode(),
                                        aAddress.sortingCode(),
                                        aAddress.languageCode(),
                                        aAddress.organization(),
                                        aAddress.recipient(),
                                        aAddress.phone());
}

nsresult
PaymentRequestManager::ChangeShippingOption(const nsAString& aRequestId,
                                            const nsAString& aOption)
{
  RefPtr<PaymentRequest> request = GetPaymentRequestById(aRequestId);
  if (NS_WARN_IF(!request)) {
    return NS_ERROR_FAILURE;
  }
  return request->UpdateShippingOption(aOption);
}

} // end of namespace dom
} // end of namespace mozilla
