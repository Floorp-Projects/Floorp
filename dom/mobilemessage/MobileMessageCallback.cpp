/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageCallback.h"
#include "mozilla/dom/ToJSValue.h"
#include "nsContentUtils.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIDOMMozMmsMessage.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "MmsMessage.h"
#include "mozilla/dom/ScriptSettings.h"
#include "jsapi.h"
#include "xpcpublic.h"
#include "nsServiceManagerUtils.h"
#include "nsTArrayHelpers.h"
#include "DOMMobileMessageError.h"
#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

static nsAutoString
ConvertErrorCodeToErrorString(int32_t aError)
{
  nsAutoString errorStr;
  switch (aError) {
    case nsIMobileMessageCallback::NO_SIGNAL_ERROR:
      errorStr = NS_LITERAL_STRING("NoSignalError");
      break;
    case nsIMobileMessageCallback::NOT_FOUND_ERROR:
      errorStr = NS_LITERAL_STRING("NotFoundError");
      break;
    case nsIMobileMessageCallback::UNKNOWN_ERROR:
      errorStr = NS_LITERAL_STRING("UnknownError");
      break;
    case nsIMobileMessageCallback::INTERNAL_ERROR:
      errorStr = NS_LITERAL_STRING("InternalError");
      break;
    case nsIMobileMessageCallback::NO_SIM_CARD_ERROR:
      errorStr = NS_LITERAL_STRING("NoSimCardError");
      break;
    case nsIMobileMessageCallback::RADIO_DISABLED_ERROR:
      errorStr = NS_LITERAL_STRING("RadioDisabledError");
      break;
    case nsIMobileMessageCallback::INVALID_ADDRESS_ERROR:
      errorStr = NS_LITERAL_STRING("InvalidAddressError");
      break;
    case nsIMobileMessageCallback::FDN_CHECK_ERROR:
      errorStr = NS_LITERAL_STRING("FdnCheckError");
      break;
    case nsIMobileMessageCallback::NON_ACTIVE_SIM_CARD_ERROR:
      errorStr = NS_LITERAL_STRING("NonActiveSimCardError");
      break;
    case nsIMobileMessageCallback::STORAGE_FULL_ERROR:
      errorStr = NS_LITERAL_STRING("StorageFullError");
      break;
    case nsIMobileMessageCallback::SIM_NOT_MATCHED_ERROR:
      errorStr = NS_LITERAL_STRING("SimNotMatchedError");
      break;
    default: // SUCCESS_NO_ERROR is handled above.
      MOZ_CRASH("Should never get here!");
  }

  return errorStr;
}

NS_IMPL_ADDREF(MobileMessageCallback)
NS_IMPL_RELEASE(MobileMessageCallback)

NS_INTERFACE_MAP_BEGIN(MobileMessageCallback)
  NS_INTERFACE_MAP_ENTRY(nsIMobileMessageCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

MobileMessageCallback::MobileMessageCallback(DOMRequest* aDOMRequest)
  : mDOMRequest(aDOMRequest)
{
}

MobileMessageCallback::MobileMessageCallback(Promise* aPromise)
  : mPromise(aPromise)
{
}

MobileMessageCallback::~MobileMessageCallback()
{
}


nsresult
MobileMessageCallback::NotifySuccess(JS::Handle<JS::Value> aResult, bool aAsync)
{
  if (aAsync) {
    nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

    return rs->FireSuccessAsync(mDOMRequest, aResult);
  }

  mDOMRequest->FireSuccess(aResult);
  return NS_OK;
}

nsresult
MobileMessageCallback::NotifySuccess(nsISupports *aMessage, bool aAsync)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mDOMRequest->GetOwner()))) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = jsapi.cx();

  JS::Rooted<JS::Value> wrappedMessage(cx);
  nsresult rv = nsContentUtils::WrapNative(cx, aMessage, &wrappedMessage);
  NS_ENSURE_SUCCESS(rv, rv);

  return NotifySuccess(wrappedMessage, aAsync);
}

nsresult
MobileMessageCallback::NotifyError(int32_t aError, DOMError *aDetailedError, bool aAsync)
{
  if (aAsync) {
    NS_ASSERTION(!aDetailedError,
      "No Support to FireDetailedErrorAsync() in nsIDOMRequestService!");

    nsCOMPtr<nsIDOMRequestService> rs =
      do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
    NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

    return rs->FireErrorAsync(mDOMRequest,
                              ConvertErrorCodeToErrorString(aError));
  }

  if (aDetailedError) {
    mDOMRequest->FireDetailedError(aDetailedError);
  } else {
    mDOMRequest->FireError(ConvertErrorCodeToErrorString(aError));
  }

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMessageSent(nsISupports *aMessage)
{
  return NotifySuccess(aMessage);
}

NS_IMETHODIMP
MobileMessageCallback::NotifySendMessageFailed(int32_t aError, nsISupports *aMessage)
{
  nsRefPtr<DOMMobileMessageError> domMobileMessageError;
  if (aMessage) {
    nsAutoString errorStr = ConvertErrorCodeToErrorString(aError);
    nsCOMPtr<nsIDOMMozSmsMessage> smsMsg = do_QueryInterface(aMessage);
    if (smsMsg) {
      domMobileMessageError =
        new DOMMobileMessageError(mDOMRequest->GetOwner(), errorStr, smsMsg);
    }
    else {
      nsCOMPtr<nsIDOMMozMmsMessage> mmsMsg = do_QueryInterface(aMessage);
      domMobileMessageError =
        new DOMMobileMessageError(mDOMRequest->GetOwner(), errorStr, mmsMsg);
    }
    NS_ASSERTION(domMobileMessageError, "Invalid DOMMobileMessageError!");
  }

  return NotifyError(aError, domMobileMessageError);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMessageGot(nsISupports *aMessage)
{
  return NotifySuccess(aMessage);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyGetMessageFailed(int32_t aError)
{
  return NotifyError(aError);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMessageDeleted(bool *aDeleted, uint32_t aSize)
{
  if (aSize == 1) {
    AutoJSContext cx;
    JS::Rooted<JS::Value> val(cx, aDeleted[0] ? JSVAL_TRUE : JSVAL_FALSE);
    return NotifySuccess(val);
  }

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mDOMRequest->GetOwner()))) {
    return NS_ERROR_FAILURE;
  }
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSObject*> deleteArrayObj(cx, JS_NewArrayObject(cx, aSize));
  for (uint32_t i = 0; i < aSize; i++) {
    JS_DefineElement(cx, deleteArrayObj, i, aDeleted[i], JSPROP_ENUMERATE);
  }

  JS::Rooted<JS::Value> deleteArrayVal(cx, JS::ObjectValue(*deleteArrayObj));
  return NotifySuccess(deleteArrayVal);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyDeleteMessageFailed(int32_t aError)
{
  return NotifyError(aError);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMessageMarkedRead(bool aRead)
{
  AutoJSContext cx;
  JS::Rooted<JS::Value> val(cx, aRead ? JSVAL_TRUE : JSVAL_FALSE);
  return NotifySuccess(val);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMarkMessageReadFailed(int32_t aError)
{
  return NotifyError(aError);
}

NS_IMETHODIMP
MobileMessageCallback::NotifySegmentInfoForTextGot(int32_t aSegments,
                                                   int32_t aCharsPerSegment,
                                                   int32_t aCharsAvailableInLastSegment)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mDOMRequest->GetOwner()))) {
    return NotifyError(nsIMobileMessageCallback::INTERNAL_ERROR);
  }

  SmsSegmentInfo info;
  info.mSegments = aSegments;
  info.mCharsPerSegment = aCharsPerSegment;
  info.mCharsAvailableInLastSegment = aCharsAvailableInLastSegment;

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> val(cx);
  if (!ToJSValue(cx, info, &val)) {
    JS_ClearPendingException(cx);
    return NotifyError(nsIMobileMessageCallback::INTERNAL_ERROR);
  }

  return NotifySuccess(val, true);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyGetSegmentInfoForTextFailed(int32_t aError)
{
  return NotifyError(aError, nullptr, true);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyGetSmscAddress(const nsAString& aSmscAddress)
{
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mDOMRequest->GetOwner()))) {
    return NotifyError(nsIMobileMessageCallback::INTERNAL_ERROR);
  }
  JSContext* cx = jsapi.cx();
  JSString* smsc = JS_NewUCStringCopyN(cx, aSmscAddress.BeginReading(),
                                       aSmscAddress.Length());

  if (!smsc) {
    return NotifyError(nsIMobileMessageCallback::INTERNAL_ERROR);
  }

  JS::Rooted<JS::Value> val(cx, STRING_TO_JSVAL(smsc));
  return NotifySuccess(val);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyGetSmscAddressFailed(int32_t aError)
{
  return NotifyError(aError);
}

NS_IMETHODIMP
MobileMessageCallback::NotifySetSmscAddress()
{
  mPromise->MaybeResolve(JS::UndefinedHandleValue);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageCallback::NotifySetSmscAddressFailed(int32_t aError)
{
  const nsAString& errorStr = ConvertErrorCodeToErrorString(aError);
  mPromise->MaybeRejectBrokenly(errorStr);
  return NS_OK;
}

} // namesapce mobilemessage
} // namespace dom
} // namespace mozilla
