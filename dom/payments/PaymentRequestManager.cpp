/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "mozilla/dom/TabChild.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsIPrincipal.h"
#include "PaymentRequestManager.h"
#include "PaymentRequestUtils.h"
#include "PaymentResponse.h"

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
  IPCPaymentCurrencyAmount amount;
  ConvertCurrencyAmount(aItem.mAmount, amount);
  aIPCItem = IPCPaymentItem(aItem.mLabel, amount, aItem.mPending);
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
  nsAutoString id;
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
                                  EmptyString(), // payerErrors
                                  EmptyString()); // paymentMethodErrors
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
  nsAutoString error;
  if (aDetails.mError.WasPassed()) {
    error = aDetails.mError.Value();
  }

  nsAutoString shippingAddressErrors;
  if (!aDetails.mShippingAddressErrors.ToJSON(shippingAddressErrors)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString payerErrors;
  if (!aDetails.mPayerErrors.ToJSON(payerErrors)) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString paymentMethodErrors;
  if (aDetails.mPaymentMethodErrors.WasPassed()) {
    JS::RootedObject object(aCx, aDetails.mPaymentMethodErrors.Value());
    nsresult rv = SerializeFromJSObject(aCx, object, paymentMethodErrors);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  aIPCDetails = IPCPaymentDetails(EmptyString(), // id
                                  total,
                                  displayItems,
                                  shippingOptions,
                                  modifiers,
                                  error,
                                  shippingAddressErrors,
                                  payerErrors,
                                  paymentMethodErrors);
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

void
ConvertResponseData(const IPCPaymentResponseData& aIPCData,
                    ResponseData& aData)
{
  switch (aIPCData.type()) {
    case IPCPaymentResponseData::TIPCGeneralResponse : {
      const IPCGeneralResponse& data = aIPCData;
      GeneralData gData;
      gData.data = data.data();
      aData = gData;
      break;
    }
    case IPCPaymentResponseData::TIPCBasicCardResponse: {
      const IPCBasicCardResponse& data = aIPCData;
      BasicCardData bData;
      bData.cardholderName = data.cardholderName();
      bData.cardNumber = data.cardNumber();
      bData.expiryMonth = data.expiryMonth();
      bData.expiryYear = data.expiryYear();
      bData.cardSecurityCode = data.cardSecurityCode();
      bData.billingAddress.country = data.billingAddress().country();
      bData.billingAddress.addressLine = data.billingAddress().addressLine();
      bData.billingAddress.region = data.billingAddress().region();
      bData.billingAddress.regionCode = data.billingAddress().regionCode();
      bData.billingAddress.city = data.billingAddress().city();
      bData.billingAddress.dependentLocality =
        data.billingAddress().dependentLocality();
      bData.billingAddress.postalCode = data.billingAddress().postalCode();
      bData.billingAddress.sortingCode = data.billingAddress().sortingCode();
      bData.billingAddress.organization = data.billingAddress().organization();
      bData.billingAddress.recipient = data.billingAddress().recipient();
      bData.billingAddress.phone = data.billingAddress().phone();
      aData = bData;
      break;
    }
    default: {
      break;
    }
  }
}
} // end of namespace

/* PaymentRequestManager */

StaticRefPtr<PaymentRequestManager> gPaymentManager;

PaymentRequestChild*
PaymentRequestManager::GetPaymentChild(PaymentRequest* aRequest)
{
  MOZ_ASSERT(aRequest);

  if (PaymentRequestChild* child = aRequest->GetIPC()) {
    return child;
  }

  nsPIDOMWindowInner* win = aRequest->GetOwner();
  NS_ENSURE_TRUE(win, nullptr);
  TabChild* tabChild = TabChild::GetFrom(win->GetDocShell());
  NS_ENSURE_TRUE(tabChild, nullptr);
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);

  PaymentRequestChild* paymentChild = new PaymentRequestChild(aRequest);
  tabChild->SendPPaymentRequestConstructor(paymentChild);

  return paymentChild;
}

nsresult
PaymentRequestManager::SendRequestPayment(PaymentRequest* aRequest,
                                          const IPCPaymentActionRequest& aAction,
                                          bool aResponseExpected)
{
  PaymentRequestChild* requestChild = GetPaymentChild(aRequest);
  nsresult rv = requestChild->RequestPayment(aAction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aResponseExpected) {
    auto count = mActivePayments.LookupForAdd(aRequest);
    if (count) {
      count.Data()++;
    } else {
      count.OrInsert([]() { return 1; });
    }
  }
  return NS_OK;
}

void
PaymentRequestManager::NotifyRequestDone(PaymentRequest* aRequest)
{
  auto entry = mActivePayments.Lookup(aRequest);
  MOZ_ASSERT(entry);
  MOZ_ASSERT(entry.Data() > 0);

  uint32_t count = --entry.Data();
  if (count == 0) {
    entry.Remove();
  }
}

void
PaymentRequestManager::RequestIPCOver(PaymentRequest* aRequest)
{
  // This must only be called from ActorDestroy or if we're sure we won't
  // receive any more IPC for aRequest.
  mActivePayments.Remove(aRequest);
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
  request->SetOptions(aOptions);
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

  nsCOMPtr<nsPIDOMWindowOuter> outerWindow = aWindow->GetOuterWindow();
  MOZ_ASSERT(outerWindow);
  if (nsCOMPtr<nsPIDOMWindowOuter> topOuterWindow = outerWindow->GetTop()) {
    outerWindow = topOuterWindow;
  }
  uint64_t topOuterWindowId = outerWindow->WindowID();

  IPCPaymentCreateActionRequest action(topOuterWindowId,
                                       internalId,
                                       IPC::Principal(aTopLevelPrincipal),
                                       methodData,
                                       details,
                                       options,
                                       shippingOption);

  rv = SendRequestPayment(request, action, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  request.forget(aRequest);
  return NS_OK;
}

nsresult
PaymentRequestManager::CanMakePayment(PaymentRequest* aRequest)
{
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentCanMakeActionRequest action(requestId);

  return SendRequestPayment(aRequest, action);
}

nsresult
PaymentRequestManager::ShowPayment(PaymentRequest* aRequest)
{
  nsresult rv = NS_OK;
  if (!aRequest->IsUpdating()) {
    nsAutoString requestId;
    aRequest->GetInternalId(requestId);
    IPCPaymentShowActionRequest action(requestId);
    rv = SendRequestPayment(aRequest, action);
  }
  return rv;
}

nsresult
PaymentRequestManager::AbortPayment(PaymentRequest* aRequest, bool aDeferredShow)
{
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentAbortActionRequest action(requestId);

  // If aDeferredShow is true, then show was called with a promise that was
  // rejected. In that case, we need to remember that we called show earlier.
  return SendRequestPayment(aRequest, action, aDeferredShow);
}

nsresult
PaymentRequestManager::CompletePayment(PaymentRequest* aRequest,
                                       const PaymentComplete& aComplete,
                                       bool aTimedOut)
{
  nsString completeStatusString(NS_LITERAL_STRING("unknown"));
  if (aTimedOut) {
    completeStatusString.AssignLiteral("timeout");
  } else {
    uint8_t completeIndex = static_cast<uint8_t>(aComplete);
    if (completeIndex < ArrayLength(PaymentCompleteValues::strings)) {
      completeStatusString.AssignASCII(
        PaymentCompleteValues::strings[completeIndex].value);
    }
  }

  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentCompleteActionRequest action(requestId, completeStatusString);

  return SendRequestPayment(aRequest, action, false);
}

nsresult
PaymentRequestManager::UpdatePayment(JSContext* aCx,
                                     PaymentRequest* aRequest,
                                     const PaymentDetailsUpdate& aDetails,
                                     bool aRequestShipping,
                                     bool aDeferredShow)
{
  NS_ENSURE_ARG_POINTER(aCx);
  IPCPaymentDetails details;
  nsresult rv = ConvertDetailsUpdate(aCx, aDetails, details, aRequestShipping);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString shippingOption;
  SetDOMStringToNull(shippingOption);
  if (aRequestShipping) {
    GetSelectedShippingOption(aDetails, shippingOption);
    aRequest->SetShippingOption(shippingOption);
  }

  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentUpdateActionRequest action(requestId, details, shippingOption);

  // If aDeferredShow is true, then this call serves as the ShowUpdate call for
  // this request.
  return SendRequestPayment(aRequest, action, aDeferredShow);
}

nsresult
PaymentRequestManager::ClosePayment(PaymentRequest* aRequest)
{
  // for the case, the payment request is waiting for response from user.
  if (auto entry = mActivePayments.Lookup(aRequest)) {
    NotifyRequestDone(aRequest);
  }
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentCloseActionRequest action(requestId);
  return SendRequestPayment(aRequest, action, false);
}

nsresult
PaymentRequestManager::RetryPayment(JSContext* aCx,
                                    PaymentRequest* aRequest,
                                    const PaymentValidationErrors& aErrors)
{
  NS_ENSURE_ARG_POINTER(aCx);
  NS_ENSURE_ARG_POINTER(aRequest);

  nsAutoString requestId;
  aRequest->GetInternalId(requestId);

  nsAutoString error;
  if (aErrors.mError.WasPassed()) {
    error = aErrors.mError.Value();
  }

  nsAutoString shippingAddressErrors;
  aErrors.mShippingAddress.ToJSON(shippingAddressErrors);

  nsAutoString payerErrors;
  aErrors.mPayer.ToJSON(payerErrors);

  nsAutoString paymentMethodErrors;
  if (aErrors.mPaymentMethod.WasPassed()) {
    JS::RootedObject object(aCx, aErrors.mPaymentMethod.Value());
    nsresult rv = SerializeFromJSObject(aCx, object, paymentMethodErrors);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }
  IPCPaymentRetryActionRequest action(requestId,
                                      error,
                                      payerErrors,
                                      paymentMethodErrors,
                                      shippingAddressErrors);
  return SendRequestPayment(aRequest, action);
}

nsresult
PaymentRequestManager::RespondPayment(PaymentRequest* aRequest,
                                      const IPCPaymentActionResponse& aResponse)
{
  switch (aResponse.type()) {
    case IPCPaymentActionResponse::TIPCPaymentCanMakeActionResponse: {
      const IPCPaymentCanMakeActionResponse& response = aResponse;
      aRequest->RespondCanMakePayment(response.result());
      NotifyRequestDone(aRequest);
      break;
    }
    case IPCPaymentActionResponse::TIPCPaymentShowActionResponse: {
      const IPCPaymentShowActionResponse& response = aResponse;
      nsresult rejectedReason = NS_ERROR_DOM_ABORT_ERR;
      ResponseData responseData;
      ConvertResponseData(response.data(), responseData);
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
      aRequest->RespondShowPayment(response.methodName(),
                                   responseData,
                                   response.payerName(),
                                   response.payerEmail(),
                                   response.payerPhone(),
                                   rejectedReason);
      if (NS_FAILED(rejectedReason)) {
        NotifyRequestDone(aRequest);
      }
      break;
    }
    case IPCPaymentActionResponse::TIPCPaymentAbortActionResponse: {
      const IPCPaymentAbortActionResponse& response = aResponse;
      aRequest->RespondAbortPayment(response.isSucceeded());
      NotifyRequestDone(aRequest);
      break;
    }
    case IPCPaymentActionResponse::TIPCPaymentCompleteActionResponse: {
      aRequest->RespondComplete();
      NotifyRequestDone(aRequest);
      break;
    }
    default: {
      return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

nsresult
PaymentRequestManager::ChangeShippingAddress(PaymentRequest* aRequest,
                                             const IPCPaymentAddress& aAddress)
{
  return aRequest->UpdateShippingAddress(aAddress.country(),
                                         aAddress.addressLine(),
                                         aAddress.region(),
                                         aAddress.regionCode(),
                                         aAddress.city(),
                                         aAddress.dependentLocality(),
                                         aAddress.postalCode(),
                                         aAddress.sortingCode(),
                                         aAddress.organization(),
                                         aAddress.recipient(),
                                         aAddress.phone());
}

nsresult
PaymentRequestManager::ChangeShippingOption(PaymentRequest* aRequest,
                                            const nsAString& aOption)
{
  return aRequest->UpdateShippingOption(aOption);
}

nsresult
PaymentRequestManager::ChangePayerDetail(PaymentRequest* aRequest,
                                         const nsAString& aPayerName,
                                         const nsAString& aPayerEmail,
                                         const nsAString& aPayerPhone)
{
  MOZ_ASSERT(aRequest);
  RefPtr<PaymentResponse> response = aRequest->GetResponse();
  // ignoring the case call changePayerDetail during show().
  if (!response) {
    return NS_OK;
  }
  return response->UpdatePayerDetail(aPayerName, aPayerEmail, aPayerPhone);
}

} // end of namespace dom
} // end of namespace mozilla
