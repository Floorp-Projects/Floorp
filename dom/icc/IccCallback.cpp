/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IccCallback.h"

#include "mozilla/dom/IccCardLockError.h"
#include "mozilla/dom/MozIccBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsJSUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace icc {

NS_IMPL_ISUPPORTS(IccCallback, nsIIccCallback)

IccCallback::IccCallback(nsPIDOMWindow* aWindow, DOMRequest* aRequest,
                         bool aIsCardLockEnabled)
  : mWindow(aWindow)
  , mRequest(aRequest)
  , mIsCardLockEnabled(aIsCardLockEnabled)
{
}

IccCallback::IccCallback(nsPIDOMWindow* aWindow, Promise* aPromise)
  : mWindow(aWindow)
  , mPromise(aPromise)
{
}

nsresult
IccCallback::NotifySuccess(JS::Handle<JS::Value> aResult)
{
  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireSuccessAsync(mRequest, aResult);
}

nsresult
IccCallback::NotifyGetCardLockEnabled(bool aResult)
{
  IccCardLockStatus result;
  result.mEnabled.Construct(aResult);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
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
IccCallback::NotifySuccess()
{
  return NotifySuccess(JS::UndefinedHandleValue);
}

NS_IMETHODIMP
IccCallback::NotifySuccessWithBoolean(bool aResult)
{
  if (mPromise) {
    mPromise->MaybeResolve(aResult ? JS::TrueHandleValue : JS::FalseHandleValue);
    return NS_OK;
  }

  return mIsCardLockEnabled
    ? NotifyGetCardLockEnabled(aResult)
    : NotifySuccess(aResult ? JS::TrueHandleValue : JS::FalseHandleValue);
}

NS_IMETHODIMP
IccCallback::NotifyGetCardLockRetryCount(int32_t aCount)
{
  // TODO: Bug 1125018 - Simplify The Result of GetCardLock and
  // getCardLockRetryCount in MozIcc.webidl without a wrapper object.
  IccCardLockRetryCount result;
  result.mRetryCount.Construct(aCount);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mWindow))) {
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
IccCallback::NotifyError(const nsAString & aErrorMsg)
{
  if (mPromise) {
    mPromise->MaybeRejectBrokenly(aErrorMsg);
    return NS_OK;
  }

  nsCOMPtr<nsIDOMRequestService> rs =
    do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  return rs->FireErrorAsync(mRequest, aErrorMsg);
}

NS_IMETHODIMP
IccCallback::NotifyCardLockError(const nsAString & aErrorMsg,
                                 int32_t aRetryCount)
{
  nsRefPtr<IccCardLockError> error =
    new IccCardLockError(mWindow, aErrorMsg, aRetryCount);
  mRequest->FireDetailedError(error);

  return NS_OK;
}

} // namespace icc
} // namespace dom
} // namespace mozilla