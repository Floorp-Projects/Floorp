/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsIPrincipal.h"

namespace mozilla {
namespace dom {
namespace {

/*
 *  Following Convert* functions are used for convert PaymentRequest structs
 *  to transferable structs for IPC.
 */
nsresult
ConvertMethodData(JSContext* aCx,
                  const PaymentMethodData& aMethodData,
                  IPCPaymentMethodData& aIPCMethodData)
{
  NS_ENSURE_ARG_POINTER(aCx);
  // Convert JSObject to a serialized string
  nsAutoString serializedData;
  if (aMethodData.mData.WasPassed()) {
    JS::RootedObject object(aCx, aMethodData.mData.Value());
    nsresult rv = SerializeFromJSObject(aCx, object, serializedData);
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
  uint8_t typeIndex = UINT8_MAX;
  if (aItem.mType.WasPassed()) {
    typeIndex = static_cast<uint8_t>(aItem.mType.Value());
  }
  nsString type;
  if (typeIndex < ArrayLength(PaymentItemTypeValues::strings)) {
    type.AssignASCII(
      PaymentItemTypeValues::strings[typeIndex].value);
  }
  IPCPaymentCurrencyAmount amount;
  ConvertCurrencyAmount(aItem.mAmount, amount);
  aIPCItem = IPCPaymentItem(aItem.mLabel, amount, aItem.mPending, type);
}

nsresult
ConvertModifier(JSContext* aCx,
                const PaymentDetailsModifier& aModifier,
                IPCPaymentDetailsModifier& aIPCModifier)
{
  NS_ENSURE_ARG_POINTER(aCx);
  // Convert JSObject to a serialized string
  nsAutoString serializedData;
  if (aModifier.mData.WasPassed()) {
    JS::RootedObject object(aCx, aModifier.mData.Value());
    nsresult rv = SerializeFromJSObject(aCx, object, serializedData);
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
ConvertDetailsBase(JSContext* aCx,
                   const PaymentDetailsBase& aDetails,
                   nsTArray<IPCPaymentItem>& aDisplayItems,
                   nsTArray<IPCPaymentShippingOption>& aShippingOptions,
                   nsTArray<IPCPaymentDetailsModifier>& aModifiers,
                   bool aRequestShipping)
{
  NS_ENSURE_ARG_POINTER(aCx);
  if (aDetails.mDisplayItems.WasPassed()) {
    for (const PaymentItem& item : aDetails.mDisplayItems.Value()) {
      IPCPaymentItem displayItem;
      ConvertItem(item, displayItem);
      aDisplayItems.AppendElement(displayItem);
    }
  }
  if (aRequestShipping && aDetails.mShippingOptions.WasPassed()) {
    for (const PaymentShippingOption& option : aDetails.mShippingOptions.Value()) {
      IPCPaymentShippingOption shippingOption;
      ConvertShippingOption(option, shippingOption);
      aShippingOptions.AppendElement(shippingOption);
    }
  }
  if (aDetails.mModifiers.WasPassed()) {
    for (const PaymentDetailsModifier& modifier : aDetails.mModifiers.Value()) {
      IPCPaymentDetailsModifier detailsModifier;
      nsresult rv = ConvertModifier(aCx, modifier, detailsModifier);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
      aModifiers.AppendElement(detailsModifier);
    }
  }
  return NS_OK;
}

nsresult
ConvertDetailsInit(JSContext* aCx,
                   const PaymentDetailsInit& aDetails,
                   IPCPaymentDetails& aIPCDetails,
                   bool aRequestShipping)
{
  NS_ENSURE_ARG_POINTER(aCx);
  // Convert PaymentDetailsBase members
  nsTArray<IPCPaymentItem> displayItems;
  nsTArray<IPCPaymentShippingOption> shippingOptions;
  nsTArray<IPCPaymentDetailsModifier> modifiers;
  nsresult rv = ConvertDetailsBase(aCx, aDetails, displayItems, shippingOptions,
                                   modifiers, aRequestShipping);
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
                                  EmptyString(), // shippingAddressErrors
                                  aDetails.mDisplayItems.WasPassed(),
                                  aDetails.mShippingOptions.WasPassed(),
                                  aDetails.mModifiers.WasPassed());
  return NS_OK;
}

nsresult
ConvertDetailsUpdate(JSContext* aCx,
                     const PaymentDetailsUpdate& aDetails,
                     IPCPaymentDetails& aIPCDetails,
                     bool aRequestShipping)
{
  NS_ENSURE_ARG_POINTER(aCx);
  // Convert PaymentDetailsBase members
  nsTArray<IPCPaymentItem> displayItems;
  nsTArray<IPCPaymentShippingOption> shippingOptions;
  nsTArray<IPCPaymentDetailsModifier> modifiers;
  nsresult rv = ConvertDetailsBase(aCx, aDetails, displayItems, shippingOptions,
                                   modifiers, aRequestShipping);
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

  nsString shippingAddressErrors(EmptyString());
  if (!aDetails.mShippingAddressErrors.ToJSON(shippingAddressErrors)) {
    return NS_ERROR_FAILURE;
  }

  aIPCDetails = IPCPaymentDetails(EmptyString(), // id
                                  total,
                                  displayItems,
                                  shippingOptions,
                                  modifiers,
                                  error,
                                  shippingAddressErrors,
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
GetSelectedShippingOption(const PaymentDetailsBase& aDetails,
                          nsAString& aOption)
{
  SetDOMStringToNull(aOption);
  if (!aDetails.mShippingOptions.WasPassed()) {
    return;
  }

  const Sequence<PaymentShippingOption>& shippingOptions =
    aDetails.mShippingOptions.Value();
  for (const PaymentShippingOption& shippingOption : shippingOptions) {
    // set aOption to last selected option's ID
    if (shippingOption.mSelected) {
      aOption = shippingOption.mId;
    }
  }
}

nsresult
PaymentRequestManager::CreatePayment(JSContext* aCx,
                                     nsPIDOMWindowInner* aWindow,
                                     nsIPrincipal* aTopLevelPrincipal,
                                     const Sequence<PaymentMethodData>& aMethodData,
                                     const PaymentDetailsInit& aDetails,
                                     const PaymentOptions& aOptions,
                                     PaymentRequest** aRequest)
{
  MOZ_ASSERT(NS_IsMainThread());
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aRequest);
  NS_ENSURE_ARG_POINTER(aTopLevelPrincipal);
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
   * Set request's |mShippingType| and |mShippingOption| if shipping is required.
   * Set request's mShippingOption to last selected option's ID if
   * details.shippingOptions exists, otherwise set it as null.
   */
  nsAutoString shippingOption;
  SetDOMStringToNull(shippingOption);
  if (aOptions.mRequestShipping) {
    request->ShippingWasRequested();
    request->SetShippingType(
        Nullable<PaymentShippingType>(aOptions.mShippingType));
    GetSelectedShippingOption(aDetails, shippingOption);
  }
  request->SetShippingOption(shippingOption);


  nsAutoString internalId;
  request->GetInternalId(internalId);

  nsTArray<IPCPaymentMethodData> methodData;
  for (const PaymentMethodData& data : aMethodData) {
    IPCPaymentMethodData ipcMethodData;
    rv = ConvertMethodData(aCx, data, ipcMethodData);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    methodData.AppendElement(ipcMethodData);
  }

  IPCPaymentDetails details;
  rv = ConvertDetailsInit(aCx, aDetails, details, aOptions.mRequestShipping);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  IPCPaymentOptions options;
  ConvertOptions(aOptions, options);

  IPCPaymentCreateActionRequest action(internalId,
                                       IPC::Principal(aTopLevelPrincipal),
                                       methodData,
                                       details,
                                       options,
				       shippingOption);

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
  nsresult rv = NS_OK;
  if (!request->IsUpdating()) {
    nsAutoString requestId(aRequestId);
    IPCPaymentShowActionRequest action(requestId);
    rv = SendRequestPayment(request, action);
  }
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
PaymentRequestManager::UpdatePayment(JSContext* aCx,
                                     const nsAString& aRequestId,
                                     const PaymentDetailsUpdate& aDetails,
                                     bool aRequestShipping)
{
  NS_ENSURE_ARG_POINTER(aCx);
  RefPtr<PaymentRequest> request = GetPaymentRequestById(aRequestId);
  if (!request) {
    return NS_ERROR_UNEXPECTED;
  }
  IPCPaymentDetails details;
  nsresult rv = ConvertDetailsUpdate(aCx, aDetails, details, aRequestShipping);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString shippingOption;
  SetDOMStringToNull(shippingOption);
  if (aRequestShipping) {
    GetSelectedShippingOption(aDetails, shippingOption);
    request->SetShippingOption(shippingOption);
  }

  nsAutoString requestId(aRequestId);
  IPCPaymentUpdateActionRequest action(requestId, details, shippingOption);
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
      nsresult rejectedReason = NS_ERROR_DOM_ABORT_ERR;
      switch (response.status()) {
        case nsIPaymentActionResponse::PAYMENT_ACCEPTED: {
          rejectedReason = NS_OK;
          break;
        }
        case nsIPaymentActionResponse::PAYMENT_REJECTED: {
          rejectedReason = NS_ERROR_DOM_ABORT_ERR;
          break;
        }
        case nsIPaymentActionResponse::PAYMENT_NOTSUPPORTED: {
          rejectedReason = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
          break;
        }
        default: {
          rejectedReason = NS_ERROR_UNEXPECTED;
          break;
        }
      }
      request->RespondShowPayment(response.methodName(),
                                  response.data(),
                                  response.payerName(),
                                  response.payerEmail(),
                                  response.payerPhone(),
                                  rejectedReason);
      if (NS_FAILED(rejectedReason)) {
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
        mRequestQueue.RemoveElement(request);
      }
      mShowingRequest = nullptr;
      nsresult rv = ReleasePaymentChild(request);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
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
