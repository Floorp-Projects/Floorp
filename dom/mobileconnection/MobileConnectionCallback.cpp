/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnectionCallback.h"

#include "mozilla/dom/DOMMMIError.h"
#include "mozilla/dom/MobileNetworkInfo.h"
#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

NS_IMPL_ISUPPORTS(MobileConnectionCallback, nsIMobileConnectionCallback)

MobileConnectionCallback::MobileConnectionCallback(nsPIDOMWindow* aWindow,
                                                   DOMRequest* aRequest)
  : mWindow(aWindow)
  , mRequest(aRequest)
{
}

/**
 * Notify Success for Send/CancelMmi.
 */
nsresult
MobileConnectionCallback::NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                                                     const nsAString& aStatusMessage)
{
  MozMMIResult result;
  result.mServiceCode.Assign(aServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);

  return NotifySendCancelMmiSuccess(result);
}

nsresult
MobileConnectionCallback::NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                                                     const nsAString& aStatusMessage,
                                                     JS::Handle<JS::Value> aAdditionalInformation)
{
  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  RootedDictionary<MozMMIResult> result(cx);

  result.mServiceCode.Assign(aServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);
  result.mAdditionalInformation.Construct().SetAsObject() = &aAdditionalInformation.toObject();

  return NotifySendCancelMmiSuccess(result);
}

nsresult
MobileConnectionCallback::NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                                                     const nsAString& aStatusMessage,
                                                     uint16_t aAdditionalInformation)
{
  MozMMIResult result;
  result.mServiceCode.Assign(aServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);
  result.mAdditionalInformation.Construct().SetAsUnsignedShort() = aAdditionalInformation;

  return NotifySendCancelMmiSuccess(result);
}

nsresult
MobileConnectionCallback::NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                                                     const nsAString& aStatusMessage,
                                                     const nsTArray<nsString>& aAdditionalInformation)
{
  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> additionalInformation(cx);

  if (!ToJSValue(cx, aAdditionalInformation, &additionalInformation)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySendCancelMmiSuccess(aServiceCode, aStatusMessage,
                                    additionalInformation);
}

nsresult
MobileConnectionCallback::NotifySendCancelMmiSuccess(const nsAString& aServiceCode,
                                                     const nsAString& aStatusMessage,
                                                     const nsTArray<IPC::MozCallForwardingOptions>& aAdditionalInformation)
{
  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> additionalInformation(cx);

  if (!ToJSValue(cx, aAdditionalInformation, &additionalInformation)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySendCancelMmiSuccess(aServiceCode, aStatusMessage,
                                    additionalInformation);
}

nsresult
MobileConnectionCallback::NotifySendCancelMmiSuccess(const MozMMIResult& aResult)
{
  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);

  if (!ToJSValue(cx, aResult, &jsResult)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySuccess(jsResult);
}

/**
 * Notify Success for GetCallForwarding.
 */
nsresult
MobileConnectionCallback::NotifyGetCallForwardingSuccess(const nsTArray<IPC::MozCallForwardingOptions>& aResults)
{
  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);

  if (!ToJSValue(cx, aResults, &jsResult)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySuccess(jsResult);
}

/**
 * Notify Success.
 */
nsresult
MobileConnectionCallback::NotifySuccess(JS::Handle<JS::Value> aResult)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireSuccessAsync(mRequest, aResult);
}

// nsIMobileConnectionCallback

NS_IMETHODIMP
MobileConnectionCallback::NotifySuccess()
{
  return NotifySuccess(JS::UndefinedHandleValue);
}

NS_IMETHODIMP
MobileConnectionCallback::NotifySuccessWithString(const nsAString& aResult)
{
  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);

  if (!ToJSValue(cx, aResult, &jsResult)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySuccess(jsResult);
}

NS_IMETHODIMP
MobileConnectionCallback::NotifySuccessWithBoolean(bool aResult)
{
  return aResult ? NotifySuccess(JS::TrueHandleValue)
                 : NotifySuccess(JS::FalseHandleValue);
}

NS_IMETHODIMP
MobileConnectionCallback::NotifyGetNetworksSuccess(uint32_t aCount,
                                                   nsIMobileNetworkInfo** aNetworks)
{
  nsTArray<nsRefPtr<MobileNetworkInfo>> results;
  for (uint32_t i = 0; i < aCount; i++)
  {
    nsRefPtr<MobileNetworkInfo> networkInfo = new MobileNetworkInfo(mWindow);
    networkInfo->Update(aNetworks[i]);
    results.AppendElement(networkInfo);
  }

  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);

  if (!ToJSValue(cx, results, &jsResult)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySuccess(jsResult);
}

NS_IMETHODIMP
MobileConnectionCallback::NotifySendCancelMmiSuccess(JS::Handle<JS::Value> aResult)
{
  return NotifySuccess(aResult);
}

NS_IMETHODIMP
MobileConnectionCallback::NotifyGetCallForwardingSuccess(JS::Handle<JS::Value> aResults)
{
  return NotifySuccess(aResults);
}

NS_IMETHODIMP
MobileConnectionCallback::NotifyGetCallBarringSuccess(uint16_t aProgram,
                                                      bool aEnabled,
                                                      uint16_t aServiceClass)
{
  MozCallBarringOptions result;
  result.mProgram.Construct().SetValue(aProgram);
  result.mEnabled.Construct().SetValue(aEnabled);
  result.mServiceClass.Construct().SetValue(aServiceClass);

  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);
  if (!ToJSValue(cx, result, &jsResult)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySuccess(jsResult);
}

NS_IMETHODIMP
MobileConnectionCallback::NotifyGetClirStatusSuccess(uint16_t aN, uint16_t aM)
{
  MozClirStatus result;
  result.mN.Construct(aN);
  result.mM.Construct(aM);

  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsResult(cx);
  if (!ToJSValue(cx, result, &jsResult)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifySuccess(jsResult);
};

NS_IMETHODIMP
MobileConnectionCallback::NotifyError(const nsAString& aName,
                                      const nsAString& aMessage,
                                      const nsAString& aServiceCode,
                                      uint16_t aInfo,
                                      uint8_t aArgc)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  nsRefPtr<DOMError> error;
  switch (aArgc) {
    case 0:
      return rs->FireErrorAsync(mRequest, aName);
    case 1:
      error = new DOMMMIError(mWindow, aName, aMessage, EmptyString(),
                              Nullable<int16_t>());
      return rs->FireDetailedError(mRequest, error);
    case 2:
      error = new DOMMMIError(mWindow, aName, aMessage, aServiceCode,
                              Nullable<int16_t>());
      return rs->FireDetailedError(mRequest, error);
    case 3:
      error = new DOMMMIError(mWindow, aName, aMessage, aServiceCode,
                              Nullable<int16_t>(int16_t(aInfo)));
      return rs->FireDetailedError(mRequest, error);
  }

  return NS_ERROR_FAILURE;
}

} // namespace mobileconnection
} // namespace dom
} // namespace mozilla
