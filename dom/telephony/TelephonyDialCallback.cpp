/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyDialCallback.h"

#include "mozilla/dom/DOMMMIError.h"
#include "nsServiceManagerUtils.h"

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;

NS_IMPL_ISUPPORTS_INHERITED(TelephonyDialCallback, TelephonyCallback,
                            nsITelephonyDialCallback)

TelephonyDialCallback::TelephonyDialCallback(nsPIDOMWindow* aWindow,
                                             Telephony* aTelephony,
                                             Promise* aPromise,
                                             uint32_t aServiceId)
  : TelephonyCallback(aPromise), mWindow(aWindow), mTelephony(aTelephony),
    mServiceId(aServiceId)
{
  MOZ_ASSERT(mTelephony);
}

nsresult
TelephonyDialCallback::NotifyDialMMISuccess(const nsAString& aStatusMessage)
{
  AutoJSAPI jsapi;
  if (!NS_WARN_IF(jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  MozMMIResult result;

  result.mServiceCode.Assign(mServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);

  return NotifyDialMMISuccess(cx, result);
}

nsresult
TelephonyDialCallback::NotifyDialMMISuccess(JSContext* aCx,
                                            const nsAString& aStatusMessage,
                                            JS::Handle<JS::Value> aInfo)
{
  RootedDictionary<MozMMIResult> result(aCx);

  result.mServiceCode.Assign(mServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);
  result.mAdditionalInformation.Construct().SetAsObject() = &aInfo.toObject();

  return NotifyDialMMISuccess(aCx, result);
}

nsresult
TelephonyDialCallback::NotifyDialMMISuccess(JSContext* aCx,
                                            const MozMMIResult& aResult)
{
  JS::Rooted<JS::Value> jsResult(aCx);

  if (!ToJSValue(aCx, aResult, &jsResult)) {
    JS_ClearPendingException(aCx);
    return NS_ERROR_TYPE_ERR;
  }

  return NotifyDialMMISuccess(jsResult);
}

// nsITelephonyDialCallback

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMI(const nsAString& aServiceCode)
{
  mMMIRequest = new DOMRequest(mWindow);
  mServiceCode.Assign(aServiceCode);

  mPromise->MaybeResolve(mMMIRequest);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialCallSuccess(uint32_t aCallIndex,
                                             const nsAString& aNumber)
{
  nsRefPtr<TelephonyCallId> id = mTelephony->CreateCallId(aNumber);
  nsRefPtr<TelephonyCall> call =
      mTelephony->CreateCall(id, mServiceId, aCallIndex,
                             nsITelephonyService::CALL_STATE_DIALING);

  mPromise->MaybeResolve(call);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMISuccess(JS::Handle<JS::Value> aResult)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireSuccessAsync(mMMIRequest, aResult);
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMIError(const nsAString& aError)
{
  Nullable<int16_t> info;

  nsRefPtr<DOMError> error =
      new DOMMMIError(mWindow, aError, EmptyString(), mServiceCode, info);

  nsCOMPtr<nsIDOMRequestService> rs = do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireDetailedError(mMMIRequest, error);
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMIErrorWithInfo(const nsAString& aError,
                                                  uint16_t aInfo)
{
  Nullable<int16_t> info(aInfo);

  nsRefPtr<DOMError> error =
      new DOMMMIError(mWindow, aError, EmptyString(), mServiceCode, info);

  nsCOMPtr<nsIDOMRequestService> rs = do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireDetailedError(mMMIRequest, error);
}
