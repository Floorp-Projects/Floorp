/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaymentRequestManager.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/PaymentRequestChild.h"
#include "nsContentUtils.h"
#include "nsIJSON.h"
#include "nsString.h"

namespace mozilla {
namespace dom {
namespace {

/*
 *  Following Convert* functions are used for convert PaymentRequest structs
 *  to transferable structs for IPC.
 */
nsresult
SerializeFromJSObject(JSContext* aCx, JS::HandleObject aObject, nsAString& aSerializedObject){
  nsCOMPtr<nsIJSON> serializer = do_CreateInstance("@mozilla.org/dom/json;1");
  if (NS_WARN_IF(!serializer)) {
    return NS_ERROR_FAILURE;
  }
  JS::RootedValue value(aCx, JS::ObjectValue(*aObject));
  //JS::Value value = JS::ObjectValue(*aObject);
  return serializer->EncodeFromJSVal(value.address(), aCx, aSerializedObject);
}

nsresult
ConvertMethodData(const PaymentMethodData& aMethodData,
                  IPCPaymentMethodData& aIPCMethodData)
{
  // Convert Sequence<nsString> to nsTArray<nsString>
  nsTArray<nsString> supportedMethods;
  for (const nsString& method : aMethodData.mSupportedMethods) {
    supportedMethods.AppendElement(method);
  }

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
  aIPCMethodData = IPCPaymentMethodData(supportedMethods, serializedData);
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
  // Convert Sequence<nsString> to nsTArray<nsString>
  nsTArray<nsString> supportedMethods;
  for (const nsString& method : aModifier.mSupportedMethods) {
    supportedMethods.AppendElement(method);
  }

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
  aIPCModifier = IPCPaymentDetailsModifier(supportedMethods,
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
                   nsTArray<IPCPaymentDetailsModifier>& aModifiers)
{
  if (aDetails.mDisplayItems.WasPassed()) {
    for (const PaymentItem& item : aDetails.mDisplayItems.Value()) {
      IPCPaymentItem displayItem;
      ConvertItem(item, displayItem);
      aDisplayItems.AppendElement(displayItem);
    }
  }
  if (aDetails.mShippingOptions.WasPassed()) {
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
                   IPCPaymentDetails& aIPCDetails)
{
  // Convert PaymentDetailsBase members
  nsTArray<IPCPaymentItem> displayItems;
  nsTArray<IPCPaymentShippingOption> shippingOptions;
  nsTArray<IPCPaymentDetailsModifier> modifiers;
  nsresult rv = ConvertDetailsBase(aDetails, displayItems, shippingOptions, modifiers);
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
  rv = ConvertDetailsInit(aDetails, details);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  IPCPaymentOptions options;
  ConvertOptions(aOptions, options);

  RefPtr<PaymentRequest> paymentRequest = PaymentRequest::CreatePaymentRequest(aWindow, rv);
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
    paymentRequest->GetInternalId(requestId);
  }
  paymentRequest->SetId(requestId);

  RefPtr<PaymentRequestChild> requestChild;
  rv = GetPaymentChild(paymentRequest, getter_AddRefs(requestChild));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsAutoString internalId;
  paymentRequest->GetInternalId(internalId);
  IPCPaymentCreateActionRequest request(internalId,
                                        methodData,
                                        details,
                                        options);
  rv = requestChild->RequestPayment(request);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = ReleasePaymentChild(paymentRequest);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mRequestQueue.AppendElement(paymentRequest);
  paymentRequest.forget(aRequest);
  return NS_OK;
}

} // end of namespace dom
} // end of namespace mozilla
