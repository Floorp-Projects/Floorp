/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this file,
* You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileConnectionCallback.h"

#include "DOMMMIError.h"
#include "mozilla/dom/MobileNetworkInfo.h"
#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;

NS_IMPL_ISUPPORTS(MobileConnectionCallback, nsIMobileConnectionCallback)

MobileConnectionCallback::MobileConnectionCallback(nsPIDOMWindow* aWindow,
                                                   DOMRequest* aRequest)
  : mWindow(aWindow)
  , mRequest(aRequest)
{
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
