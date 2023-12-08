/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/Preferences.h"
#include "nsContentUtils.h"
#include "nsString.h"
#include "nsIPrincipal.h"
#include "nsIPaymentActionResponse.h"
#include "PaymentRequestManager.h"
#include "PaymentRequestUtils.h"
#include "PaymentResponse.h"

namespace mozilla::dom {
namespace {

/*
 *  Following Convert* functions are used for convert PaymentRequest structs
 *  to transferable structs for IPC.
 */
void ConvertMethodData(JSContext* aCx, const PaymentMethodData& aMethodData,
                       IPCPaymentMethodData& aIPCMethodData, ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  // Convert JSObject to a serialized string
  nsAutoString serializedData;
  if (aMethodData.mData.WasPassed()) {
    JS::Rooted<JSObject*> object(aCx, aMethodData.mData.Value());
    if (NS_WARN_IF(
            NS_FAILED(SerializeFromJSObject(aCx, object, serializedData)))) {
      aRv.ThrowTypeError(
          "The PaymentMethodData.data must be a serializable object");
      return;
    }
  }
  aIPCMethodData =
      IPCPaymentMethodData(aMethodData.mSupportedMethods, serializedData);
}

void ConvertCurrencyAmount(const PaymentCurrencyAmount& aAmount,
                           IPCPaymentCurrencyAmount& aIPCCurrencyAmount) {
  aIPCCurrencyAmount =
      IPCPaymentCurrencyAmount(aAmount.mCurrency, aAmount.mValue);
}

void ConvertItem(const PaymentItem& aItem, IPCPaymentItem& aIPCItem) {
  IPCPaymentCurrencyAmount amount;
  ConvertCurrencyAmount(aItem.mAmount, amount);
  aIPCItem = IPCPaymentItem(aItem.mLabel, amount, aItem.mPending);
}

void ConvertModifier(JSContext* aCx, const PaymentDetailsModifier& aModifier,
                     IPCPaymentDetailsModifier& aIPCModifier,
                     ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  // Convert JSObject to a serialized string
  nsAutoString serializedData;
  if (aModifier.mData.WasPassed()) {
    JS::Rooted<JSObject*> object(aCx, aModifier.mData.Value());
    if (NS_WARN_IF(
            NS_FAILED(SerializeFromJSObject(aCx, object, serializedData)))) {
      aRv.ThrowTypeError("The Modifier.data must be a serializable object");
      return;
    }
  }

  IPCPaymentItem total;
  if (aModifier.mTotal.WasPassed()) {
    ConvertItem(aModifier.mTotal.Value(), total);
  }

  nsTArray<IPCPaymentItem> additionalDisplayItems;
  if (aModifier.mAdditionalDisplayItems.WasPassed()) {
    for (const PaymentItem& item : aModifier.mAdditionalDisplayItems.Value()) {
      IPCPaymentItem displayItem;
      ConvertItem(item, displayItem);
      additionalDisplayItems.AppendElement(displayItem);
    }
  }
  aIPCModifier = IPCPaymentDetailsModifier(
      aModifier.mSupportedMethods, total, additionalDisplayItems,
      serializedData, aModifier.mAdditionalDisplayItems.WasPassed());
}

void ConvertShippingOption(const PaymentShippingOption& aOption,
                           IPCPaymentShippingOption& aIPCOption) {
  IPCPaymentCurrencyAmount amount;
  ConvertCurrencyAmount(aOption.mAmount, amount);
  aIPCOption = IPCPaymentShippingOption(aOption.mId, aOption.mLabel, amount,
                                        aOption.mSelected);
}

void ConvertDetailsBase(JSContext* aCx, const PaymentDetailsBase& aDetails,
                        nsTArray<IPCPaymentItem>& aDisplayItems,
                        nsTArray<IPCPaymentShippingOption>& aShippingOptions,
                        nsTArray<IPCPaymentDetailsModifier>& aModifiers,
                        bool aRequestShipping, ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  if (aDetails.mDisplayItems.WasPassed()) {
    for (const PaymentItem& item : aDetails.mDisplayItems.Value()) {
      IPCPaymentItem displayItem;
      ConvertItem(item, displayItem);
      aDisplayItems.AppendElement(displayItem);
    }
  }
  if (aRequestShipping && aDetails.mShippingOptions.WasPassed()) {
    for (const PaymentShippingOption& option :
         aDetails.mShippingOptions.Value()) {
      IPCPaymentShippingOption shippingOption;
      ConvertShippingOption(option, shippingOption);
      aShippingOptions.AppendElement(shippingOption);
    }
  }
  if (aDetails.mModifiers.WasPassed()) {
    for (const PaymentDetailsModifier& modifier : aDetails.mModifiers.Value()) {
      IPCPaymentDetailsModifier detailsModifier;
      ConvertModifier(aCx, modifier, detailsModifier, aRv);
      if (aRv.Failed()) {
        return;
      }
      aModifiers.AppendElement(detailsModifier);
    }
  }
}

void ConvertDetailsInit(JSContext* aCx, const PaymentDetailsInit& aDetails,
                        IPCPaymentDetails& aIPCDetails, bool aRequestShipping,
                        ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  // Convert PaymentDetailsBase members
  nsTArray<IPCPaymentItem> displayItems;
  nsTArray<IPCPaymentShippingOption> shippingOptions;
  nsTArray<IPCPaymentDetailsModifier> modifiers;
  ConvertDetailsBase(aCx, aDetails, displayItems, shippingOptions, modifiers,
                     aRequestShipping, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Convert |id|
  nsAutoString id;
  if (aDetails.mId.WasPassed()) {
    id = aDetails.mId.Value();
  }

  // Convert required |total|
  IPCPaymentItem total;
  ConvertItem(aDetails.mTotal, total);

  aIPCDetails =
      IPCPaymentDetails(id, total, displayItems, shippingOptions, modifiers,
                        u""_ns,   // error message
                        u""_ns,   // shippingAddressErrors
                        u""_ns,   // payerErrors
                        u""_ns);  // paymentMethodErrors
}

void ConvertDetailsUpdate(JSContext* aCx, const PaymentDetailsUpdate& aDetails,
                          IPCPaymentDetails& aIPCDetails, bool aRequestShipping,
                          ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  // Convert PaymentDetailsBase members
  nsTArray<IPCPaymentItem> displayItems;
  nsTArray<IPCPaymentShippingOption> shippingOptions;
  nsTArray<IPCPaymentDetailsModifier> modifiers;
  ConvertDetailsBase(aCx, aDetails, displayItems, shippingOptions, modifiers,
                     aRequestShipping, aRv);
  if (aRv.Failed()) {
    return;
  }

  // Convert required |total|
  IPCPaymentItem total;
  if (aDetails.mTotal.WasPassed()) {
    ConvertItem(aDetails.mTotal.Value(), total);
  }

  // Convert |error|
  nsAutoString error;
  if (aDetails.mError.WasPassed()) {
    error = aDetails.mError.Value();
  }

  nsAutoString shippingAddressErrors;
  if (aDetails.mShippingAddressErrors.WasPassed()) {
    if (!aDetails.mShippingAddressErrors.Value().ToJSON(
            shippingAddressErrors)) {
      aRv.ThrowTypeError("The ShippingAddressErrors can not be serailized");
      return;
    }
  }

  nsAutoString payerErrors;
  if (aDetails.mPayerErrors.WasPassed()) {
    if (!aDetails.mPayerErrors.Value().ToJSON(payerErrors)) {
      aRv.ThrowTypeError("The PayerErrors can not be serialized");
      return;
    }
  }

  nsAutoString paymentMethodErrors;
  if (aDetails.mPaymentMethodErrors.WasPassed()) {
    JS::Rooted<JSObject*> object(aCx, aDetails.mPaymentMethodErrors.Value());
    if (NS_WARN_IF(NS_FAILED(
            SerializeFromJSObject(aCx, object, paymentMethodErrors)))) {
      aRv.ThrowTypeError("The PaymentMethodErrors can not be serialized");
      return;
    }
  }

  aIPCDetails = IPCPaymentDetails(u""_ns,  // id
                                  total, displayItems, shippingOptions,
                                  modifiers, error, shippingAddressErrors,
                                  payerErrors, paymentMethodErrors);
}

void ConvertOptions(const PaymentOptions& aOptions,
                    IPCPaymentOptions& aIPCOption) {
  NS_ConvertASCIItoUTF16 shippingType(
      PaymentShippingTypeValues::GetString(aOptions.mShippingType));
  aIPCOption =
      IPCPaymentOptions(aOptions.mRequestPayerName, aOptions.mRequestPayerEmail,
                        aOptions.mRequestPayerPhone, aOptions.mRequestShipping,
                        aOptions.mRequestBillingAddress, shippingType);
}

void ConvertResponseData(const IPCPaymentResponseData& aIPCData,
                         ResponseData& aData) {
  switch (aIPCData.type()) {
    case IPCPaymentResponseData::TIPCGeneralResponse: {
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
      bData.billingAddress.addressLine =
          data.billingAddress().addressLine().Clone();
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

void ConvertMethodChangeDetails(const IPCMethodChangeDetails& aIPCDetails,
                                ChangeDetails& aDetails) {
  switch (aIPCDetails.type()) {
    case IPCMethodChangeDetails::TIPCGeneralChangeDetails: {
      const IPCGeneralChangeDetails& details = aIPCDetails;
      GeneralDetails gDetails;
      gDetails.details = details.details();
      aDetails = gDetails;
      break;
    }
    case IPCMethodChangeDetails::TIPCBasicCardChangeDetails: {
      const IPCBasicCardChangeDetails& details = aIPCDetails;
      BasicCardDetails bDetails;
      bDetails.billingAddress.country = details.billingAddress().country();
      bDetails.billingAddress.addressLine =
          details.billingAddress().addressLine();
      bDetails.billingAddress.region = details.billingAddress().region();
      bDetails.billingAddress.regionCode =
          details.billingAddress().regionCode();
      bDetails.billingAddress.city = details.billingAddress().city();
      bDetails.billingAddress.dependentLocality =
          details.billingAddress().dependentLocality();
      bDetails.billingAddress.postalCode =
          details.billingAddress().postalCode();
      bDetails.billingAddress.sortingCode =
          details.billingAddress().sortingCode();
      bDetails.billingAddress.organization =
          details.billingAddress().organization();
      bDetails.billingAddress.recipient = details.billingAddress().recipient();
      bDetails.billingAddress.phone = details.billingAddress().phone();
      aDetails = bDetails;
      break;
    }
    default: {
      break;
    }
  }
}
}  // end of namespace

/* PaymentRequestManager */

StaticRefPtr<PaymentRequestManager> gPaymentManager;
const char kSupportedRegionsPref[] = "dom.payments.request.supportedRegions";

void SupportedRegionsPrefChangedCallback(const char* aPrefName, void* aRetval) {
  auto retval = static_cast<nsTArray<nsString>*>(aRetval);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aPrefName, kSupportedRegionsPref));

  nsAutoString supportedRegions;
  Preferences::GetString(aPrefName, supportedRegions);
  retval->Clear();
  for (const nsAString& each : supportedRegions.Split(',')) {
    retval->AppendElement(each);
  }
}

PaymentRequestManager::PaymentRequestManager() {
  Preferences::RegisterCallbackAndCall(SupportedRegionsPrefChangedCallback,
                                       kSupportedRegionsPref,
                                       &this->mSupportedRegions);
}

PaymentRequestManager::~PaymentRequestManager() {
  MOZ_ASSERT(mActivePayments.Count() == 0);
  Preferences::UnregisterCallback(SupportedRegionsPrefChangedCallback,
                                  kSupportedRegionsPref,
                                  &this->mSupportedRegions);
  mSupportedRegions.Clear();
}

bool PaymentRequestManager::IsRegionSupported(const nsAString& region) const {
  return mSupportedRegions.Contains(region);
}

PaymentRequestChild* PaymentRequestManager::GetPaymentChild(
    PaymentRequest* aRequest) {
  MOZ_ASSERT(aRequest);

  if (PaymentRequestChild* child = aRequest->GetIPC()) {
    return child;
  }

  nsPIDOMWindowInner* win = aRequest->GetOwner();
  NS_ENSURE_TRUE(win, nullptr);
  BrowserChild* browserChild = BrowserChild::GetFrom(win->GetDocShell());
  NS_ENSURE_TRUE(browserChild, nullptr);
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);

  PaymentRequestChild* paymentChild = new PaymentRequestChild(aRequest);
  if (!browserChild->SendPPaymentRequestConstructor(paymentChild)) {
    // deleted by Constructor
    return nullptr;
  }

  return paymentChild;
}

nsresult PaymentRequestManager::SendRequestPayment(
    PaymentRequest* aRequest, const IPCPaymentActionRequest& aAction,
    bool aResponseExpected) {
  PaymentRequestChild* requestChild = GetPaymentChild(aRequest);
  // bug 1580496, ignoring the case that requestChild is nullptr. It could be
  // nullptr while the corresponding nsPIDOMWindowInner is nullptr.
  if (NS_WARN_IF(!requestChild)) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = requestChild->RequestPayment(aAction);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aResponseExpected) {
    ++mActivePayments.LookupOrInsert(aRequest, 0);
  }
  return NS_OK;
}

void PaymentRequestManager::NotifyRequestDone(PaymentRequest* aRequest) {
  auto entry = mActivePayments.Lookup(aRequest);
  MOZ_ASSERT(entry);
  MOZ_ASSERT(entry.Data() > 0);

  uint32_t count = --entry.Data();
  if (count == 0) {
    entry.Remove();
  }
}

void PaymentRequestManager::RequestIPCOver(PaymentRequest* aRequest) {
  // This must only be called from ActorDestroy or if we're sure we won't
  // receive any more IPC for aRequest.
  mActivePayments.Remove(aRequest);
}

already_AddRefed<PaymentRequestManager> PaymentRequestManager::GetSingleton() {
  if (!gPaymentManager) {
    gPaymentManager = new PaymentRequestManager();
    ClearOnShutdown(&gPaymentManager);
  }
  RefPtr<PaymentRequestManager> manager = gPaymentManager;
  return manager.forget();
}

void GetSelectedShippingOption(const PaymentDetailsBase& aDetails,
                               nsAString& aOption) {
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

void PaymentRequestManager::CreatePayment(
    JSContext* aCx, nsPIDOMWindowInner* aWindow,
    nsIPrincipal* aTopLevelPrincipal,
    const Sequence<PaymentMethodData>& aMethodData,
    const PaymentDetailsInit& aDetails, const PaymentOptions& aOptions,
    PaymentRequest** aRequest, ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aTopLevelPrincipal);
  *aRequest = nullptr;

  RefPtr<PaymentRequest> request =
      PaymentRequest::CreatePaymentRequest(aWindow, aRv);
  if (aRv.Failed()) {
    return;
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
   * Set request's |mShippingType| and |mShippingOption| if shipping is
   * required. Set request's mShippingOption to last selected option's ID if
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
    ConvertMethodData(aCx, data, ipcMethodData, aRv);
    if (aRv.Failed()) {
      return;
    }
    methodData.AppendElement(ipcMethodData);
  }

  IPCPaymentDetails details;
  ConvertDetailsInit(aCx, aDetails, details, aOptions.mRequestShipping, aRv);
  if (aRv.Failed()) {
    return;
  }

  IPCPaymentOptions options;
  ConvertOptions(aOptions, options);

  uint64_t topOuterWindowId =
      aWindow->GetWindowContext()->TopWindowContext()->OuterWindowId();
  IPCPaymentCreateActionRequest action(topOuterWindowId, internalId,
                                       aTopLevelPrincipal, methodData, details,
                                       options, shippingOption);

  if (NS_WARN_IF(NS_FAILED(SendRequestPayment(request, action, false)))) {
    aRv.ThrowUnknownError("Internal error sending payment request");
    return;
  }
  request.forget(aRequest);
}

void PaymentRequestManager::CanMakePayment(PaymentRequest* aRequest,
                                           ErrorResult& aRv) {
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentCanMakeActionRequest action(requestId);
  if (NS_WARN_IF(NS_FAILED(SendRequestPayment(aRequest, action)))) {
    aRv.ThrowUnknownError("Internal error sending payment request");
  }
}

void PaymentRequestManager::ShowPayment(PaymentRequest* aRequest,
                                        ErrorResult& aRv) {
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentShowActionRequest action(requestId, aRequest->IsUpdating());
  if (NS_WARN_IF(NS_FAILED(SendRequestPayment(aRequest, action)))) {
    aRv.ThrowUnknownError("Internal error sending payment request");
  }
}

void PaymentRequestManager::AbortPayment(PaymentRequest* aRequest,
                                         ErrorResult& aRv) {
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentAbortActionRequest action(requestId);
  if (NS_WARN_IF(NS_FAILED(SendRequestPayment(aRequest, action)))) {
    aRv.ThrowUnknownError("Internal error sending payment request");
  }
}

void PaymentRequestManager::CompletePayment(PaymentRequest* aRequest,
                                            const PaymentComplete& aComplete,
                                            ErrorResult& aRv, bool aTimedOut) {
  nsString completeStatusString(u"unknown"_ns);
  if (aTimedOut) {
    completeStatusString.AssignLiteral("timeout");
  } else {
    completeStatusString.AssignASCII(
        PaymentCompleteValues::GetString(aComplete));
  }

  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentCompleteActionRequest action(requestId, completeStatusString);
  if (NS_WARN_IF(NS_FAILED(SendRequestPayment(aRequest, action, false)))) {
    aRv.ThrowUnknownError("Internal error sending payment request");
  }
}

void PaymentRequestManager::UpdatePayment(JSContext* aCx,
                                          PaymentRequest* aRequest,
                                          const PaymentDetailsUpdate& aDetails,
                                          bool aRequestShipping,
                                          ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  IPCPaymentDetails details;
  ConvertDetailsUpdate(aCx, aDetails, details, aRequestShipping, aRv);
  if (aRv.Failed()) {
    return;
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
  if (NS_WARN_IF(NS_FAILED(SendRequestPayment(aRequest, action, false)))) {
    aRv.ThrowUnknownError("Internal error sending payment request");
  }
}

nsresult PaymentRequestManager::ClosePayment(PaymentRequest* aRequest) {
  // for the case, the payment request is waiting for response from user.
  if (auto entry = mActivePayments.Lookup(aRequest)) {
    NotifyRequestDone(aRequest);
  }
  nsAutoString requestId;
  aRequest->GetInternalId(requestId);
  IPCPaymentCloseActionRequest action(requestId);
  return SendRequestPayment(aRequest, action, false);
}

void PaymentRequestManager::RetryPayment(JSContext* aCx,
                                         PaymentRequest* aRequest,
                                         const PaymentValidationErrors& aErrors,
                                         ErrorResult& aRv) {
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aRequest);

  nsAutoString requestId;
  aRequest->GetInternalId(requestId);

  nsAutoString error;
  if (aErrors.mError.WasPassed()) {
    error = aErrors.mError.Value();
  }

  nsAutoString shippingAddressErrors;
  if (aErrors.mShippingAddress.WasPassed()) {
    if (!aErrors.mShippingAddress.Value().ToJSON(shippingAddressErrors)) {
      aRv.ThrowTypeError("The ShippingAddressErrors can not be serialized");
      return;
    }
  }

  nsAutoString payerErrors;
  if (aErrors.mPayer.WasPassed()) {
    if (!aErrors.mPayer.Value().ToJSON(payerErrors)) {
      aRv.ThrowTypeError("The PayerErrors can not be serialized");
      return;
    }
  }

  nsAutoString paymentMethodErrors;
  if (aErrors.mPaymentMethod.WasPassed()) {
    JS::Rooted<JSObject*> object(aCx, aErrors.mPaymentMethod.Value());
    if (NS_WARN_IF(NS_FAILED(
            SerializeFromJSObject(aCx, object, paymentMethodErrors)))) {
      aRv.ThrowTypeError("The PaymentMethodErrors can not be serialized");
      return;
    }
  }
  IPCPaymentRetryActionRequest action(requestId, error, payerErrors,
                                      paymentMethodErrors,
                                      shippingAddressErrors);
  if (NS_WARN_IF(NS_FAILED(SendRequestPayment(aRequest, action)))) {
    aRv.ThrowUnknownError("Internal error sending payment request");
  }
}

nsresult PaymentRequestManager::RespondPayment(
    PaymentRequest* aRequest, const IPCPaymentActionResponse& aResponse) {
  switch (aResponse.type()) {
    case IPCPaymentActionResponse::TIPCPaymentCanMakeActionResponse: {
      const IPCPaymentCanMakeActionResponse& response = aResponse;
      aRequest->RespondCanMakePayment(response.result());
      NotifyRequestDone(aRequest);
      break;
    }
    case IPCPaymentActionResponse::TIPCPaymentShowActionResponse: {
      const IPCPaymentShowActionResponse& response = aResponse;
      ErrorResult rejectedReason;
      ResponseData responseData;
      ConvertResponseData(response.data(), responseData);
      switch (response.status()) {
        case nsIPaymentActionResponse::PAYMENT_ACCEPTED: {
          break;
        }
        case nsIPaymentActionResponse::PAYMENT_REJECTED: {
          rejectedReason.ThrowAbortError("The user rejected the payment");
          break;
        }
        case nsIPaymentActionResponse::PAYMENT_NOTSUPPORTED: {
          rejectedReason.ThrowNotSupportedError("No supported payment method");
          break;
        }
        default: {
          rejectedReason.ThrowUnknownError("Unknown response for the payment");
          break;
        }
      }
      // If PaymentActionResponse is not PAYMENT_ACCEPTED, no need to keep the
      // PaymentRequestChild instance. Otherwise, keep PaymentRequestChild for
      // merchants call PaymentResponse.complete()
      if (rejectedReason.Failed()) {
        NotifyRequestDone(aRequest);
      }
      aRequest->RespondShowPayment(response.methodName(), responseData,
                                   response.payerName(), response.payerEmail(),
                                   response.payerPhone(),
                                   std::move(rejectedReason));
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

nsresult PaymentRequestManager::ChangeShippingAddress(
    PaymentRequest* aRequest, const IPCPaymentAddress& aAddress) {
  return aRequest->UpdateShippingAddress(
      aAddress.country(), aAddress.addressLine(), aAddress.region(),
      aAddress.regionCode(), aAddress.city(), aAddress.dependentLocality(),
      aAddress.postalCode(), aAddress.sortingCode(), aAddress.organization(),
      aAddress.recipient(), aAddress.phone());
}

nsresult PaymentRequestManager::ChangeShippingOption(PaymentRequest* aRequest,
                                                     const nsAString& aOption) {
  return aRequest->UpdateShippingOption(aOption);
}

nsresult PaymentRequestManager::ChangePayerDetail(
    PaymentRequest* aRequest, const nsAString& aPayerName,
    const nsAString& aPayerEmail, const nsAString& aPayerPhone) {
  MOZ_ASSERT(aRequest);
  RefPtr<PaymentResponse> response = aRequest->GetResponse();
  // ignoring the case call changePayerDetail during show().
  if (!response) {
    return NS_OK;
  }
  return response->UpdatePayerDetail(aPayerName, aPayerEmail, aPayerPhone);
}

nsresult PaymentRequestManager::ChangePaymentMethod(
    PaymentRequest* aRequest, const nsAString& aMethodName,
    const IPCMethodChangeDetails& aMethodDetails) {
  NS_ENSURE_ARG_POINTER(aRequest);
  ChangeDetails methodDetails;
  ConvertMethodChangeDetails(aMethodDetails, methodDetails);
  return aRequest->UpdatePaymentMethod(aMethodName, methodDetails);
}

}  // namespace mozilla::dom
