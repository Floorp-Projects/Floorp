/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TelephonyDialCallback.h"

#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "nsIMobileCallForwardingOptions.h"
#include "nsIMobileConnectionService.h"

using namespace mozilla::dom;
using namespace mozilla::dom::telephony;

NS_IMPL_ISUPPORTS_INHERITED(TelephonyDialCallback, TelephonyCallback,
                            nsITelephonyDialCallback)

TelephonyDialCallback::TelephonyDialCallback(nsPIDOMWindow* aWindow,
                                             Telephony* aTelephony,
                                             Promise* aPromise)
  : TelephonyCallback(aPromise), mWindow(aWindow), mTelephony(aTelephony)
{
  MOZ_ASSERT(mTelephony);
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

  mMMICall->NotifyResult(jsResult);
  return NS_OK;
}

// nsITelephonyDialCallback

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMI(const nsAString& aServiceCode)
{
  mServiceCode.Assign(aServiceCode);

  mMMICall = new MMICall(mWindow, aServiceCode);
  mPromise->MaybeResolve(mMMICall);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialCallSuccess(uint32_t aClientId,
                                             uint32_t aCallIndex,
                                             const nsAString& aNumber)
{
  RefPtr<TelephonyCallId> id = mTelephony->CreateCallId(aNumber);
  RefPtr<TelephonyCall> call =
      mTelephony->CreateCall(id, aClientId, aCallIndex,
                             TelephonyCallState::Dialing);

  mPromise->MaybeResolve(call);
  return NS_OK;
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMISuccess(const nsAString& aStatusMessage)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  MozMMIResult result;
  result.mSuccess = true;
  result.mServiceCode.Assign(mServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);

  return NotifyDialMMISuccess(cx, result);
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMISuccessWithInteger(const nsAString& aStatusMessage,
                                                       uint16_t aAdditionalInformation)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  MozMMIResult result;
  result.mSuccess = true;
  result.mServiceCode.Assign(mServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);
  result.mAdditionalInformation.Construct().SetAsUnsignedShort() = aAdditionalInformation;

  return NotifyDialMMISuccess(cx, result);
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMISuccessWithStrings(const nsAString& aStatusMessage,
                                                       uint32_t aCount,
                                                       const char16_t** aAdditionalInformation)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  RootedDictionary<MozMMIResult> result(cx);
  result.mSuccess = true;
  result.mServiceCode.Assign(mServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);

  nsTArray<nsString> additionalInformation;
  nsString* infos = additionalInformation.AppendElements(aCount);
  for (uint32_t i = 0; i < aCount; i++) {
    infos[i].Rebind(aAdditionalInformation[i],
                    nsCharTraits<char16_t>::length(aAdditionalInformation[i]));
  }

  JS::Rooted<JS::Value> jsAdditionalInformation(cx);
  if (!ToJSValue(cx, additionalInformation, &jsAdditionalInformation)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  result.mAdditionalInformation.Construct().SetAsObject() =
    &jsAdditionalInformation.toObject();

  return NotifyDialMMISuccess(cx, result);
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMISuccessWithCallForwardingOptions(const nsAString& aStatusMessage,
                                                                     uint32_t aCount,
                                                                     nsIMobileCallForwardingOptions** aResults)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  RootedDictionary<MozMMIResult> result(cx);
  result.mSuccess = true;
  result.mServiceCode.Assign(mServiceCode);
  result.mStatusMessage.Assign(aStatusMessage);

  nsTArray<MozCallForwardingOptions> additionalInformation;
  for (uint32_t i = 0; i < aCount; i++) {
    MozCallForwardingOptions options;

    bool active = false;
    aResults[i]->GetActive(&active);
    options.mActive.Construct(active);

    int16_t action = nsIMobileConnection::CALL_FORWARD_ACTION_UNKNOWN;
    aResults[i]->GetAction(&action);
    if (action != nsIMobileConnection::CALL_FORWARD_ACTION_UNKNOWN) {
      options.mAction.Construct(action);
    }

    int16_t reason = nsIMobileConnection::CALL_FORWARD_REASON_UNKNOWN;
    aResults[i]->GetReason(&reason);
    if (reason != nsIMobileConnection::CALL_FORWARD_REASON_UNKNOWN) {
      options.mReason.Construct(reason);
    }

    nsAutoString number;
    aResults[i]->GetNumber(number);
    options.mNumber.Construct(number.get());

    int16_t timeSeconds = -1;
    aResults[i]->GetTimeSeconds(&timeSeconds);
    if (timeSeconds >= 0) {
      options.mTimeSeconds.Construct(timeSeconds);
    }

    int16_t serviceClass = nsIMobileConnection::ICC_SERVICE_CLASS_NONE;
    aResults[i]->GetServiceClass(&serviceClass);
    if (serviceClass != nsIMobileConnection::ICC_SERVICE_CLASS_NONE) {
      options.mServiceClass.Construct(serviceClass);
    }

    additionalInformation.AppendElement(options);
  }

  JS::Rooted<JS::Value> jsAdditionalInformation(cx);
  if (!ToJSValue(cx, additionalInformation, &jsAdditionalInformation)) {
    JS_ClearPendingException(cx);
    return NS_ERROR_TYPE_ERR;
  }

  result.mAdditionalInformation.Construct().SetAsObject() =
    &jsAdditionalInformation.toObject();

  return NotifyDialMMISuccess(cx, result);
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMIError(const nsAString& aError)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  MozMMIResult result;
  result.mSuccess = false;
  result.mServiceCode.Assign(mServiceCode);
  result.mStatusMessage.Assign(aError);

  return NotifyDialMMISuccess(cx, result);
}

NS_IMETHODIMP
TelephonyDialCallback::NotifyDialMMIErrorWithInfo(const nsAString& aError,
                                                  uint16_t aInfo)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();

  MozMMIResult result;
  result.mSuccess = false;
  result.mServiceCode.Assign(mServiceCode);
  result.mStatusMessage.Assign(aError);
  result.mAdditionalInformation.Construct().SetAsUnsignedShort() = aInfo;

  return NotifyDialMMISuccess(cx, result);
}
