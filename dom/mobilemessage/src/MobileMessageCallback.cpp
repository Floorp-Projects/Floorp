/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageCallback.h"
#include "nsContentUtils.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIDOMMozMmsMessage.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "MmsMessage.h"
#include "jsapi.h"
#include "xpcpublic.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

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

MobileMessageCallback::~MobileMessageCallback()
{
}


nsresult
MobileMessageCallback::NotifySuccess(const jsval& aResult)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  return rs ? rs->FireSuccess(mDOMRequest, aResult) : NS_ERROR_FAILURE;
}

nsresult
MobileMessageCallback::NotifySuccess(nsISupports *aMessage)
{
  nsresult rv;
  nsIScriptContext* scriptContext = mDOMRequest->GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(scriptContext, NS_ERROR_FAILURE);

  AutoPushJSContext cx(scriptContext->GetNativeContext());
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

  jsval wrappedMessage;
  rv = nsContentUtils::WrapNative(cx,
                                  JS_GetGlobalObject(cx),
                                  aMessage,
                                  &wrappedMessage);
  NS_ENSURE_SUCCESS(rv, rv);

  return NotifySuccess(wrappedMessage);
}

nsresult
MobileMessageCallback::NotifyError(int32_t aError)
{
  nsCOMPtr<nsIDOMRequestService> rs = do_GetService(DOMREQUEST_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(rs, NS_ERROR_FAILURE);

  switch (aError) {
    case nsIMobileMessageCallback::NO_SIGNAL_ERROR:
      return rs->FireError(mDOMRequest, NS_LITERAL_STRING("NoSignalError"));
    case nsIMobileMessageCallback::NOT_FOUND_ERROR:
      return rs->FireError(mDOMRequest, NS_LITERAL_STRING("NotFoundError"));
    case nsIMobileMessageCallback::UNKNOWN_ERROR:
      return rs->FireError(mDOMRequest, NS_LITERAL_STRING("UnknownError"));
    case nsIMobileMessageCallback::INTERNAL_ERROR:
      return rs->FireError(mDOMRequest, NS_LITERAL_STRING("InternalError"));
    default: // SUCCESS_NO_ERROR is handled above.
      MOZ_ASSERT(false, "Unknown error value.");
  }

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMessageSent(nsISupports *aMessage)
{
  return NotifySuccess(aMessage);
}

NS_IMETHODIMP
MobileMessageCallback::NotifySendMessageFailed(int32_t aError)
{
  return NotifyError(aError);
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
MobileMessageCallback::NotifyMessageDeleted(bool aDeleted)
{
  return NotifySuccess(aDeleted ? JSVAL_TRUE : JSVAL_FALSE);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyDeleteMessageFailed(int32_t aError)
{
  return NotifyError(aError);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMessageListCreated(int32_t aListId,
                                                nsISupports *aMessage)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileMessageCallback::NotifyReadMessageListFailed(int32_t aError)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileMessageCallback::NotifyNextMessageInListGot(nsISupports *aMessage)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileMessageCallback::NotifyNoMessageInList()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMessageMarkedRead(bool aRead)
{
  return NotifySuccess(aRead ? JSVAL_TRUE : JSVAL_FALSE);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyMarkMessageReadFailed(int32_t aError)
{
  return NotifyError(aError);
}

NS_IMETHODIMP
MobileMessageCallback::NotifyThreadList(const jsval& aThreadList, JSContext* aCx)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileMessageCallback::NotifyThreadListFailed(int32_t aError)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namesapce mobilemessage
} // namespace dom
} // namespace mozilla
