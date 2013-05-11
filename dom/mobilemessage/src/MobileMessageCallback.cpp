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
#include "nsTArrayHelpers.h"

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
MobileMessageCallback::NotifySuccess(const JS::Value& aResult)
{
  mDOMRequest->FireSuccess(aResult);
  return NS_OK;
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

  JS::Rooted<JSObject*> global(cx, scriptContext->GetNativeGlobal());
  NS_ENSURE_TRUE(global, NS_ERROR_FAILURE);

  JSAutoRequest ar(cx);
  JSAutoCompartment ac(cx, global);

  JS::Rooted<JS::Value> wrappedMessage(cx);
  rv = nsContentUtils::WrapNative(cx, global, aMessage,
                                  wrappedMessage.address());
  NS_ENSURE_SUCCESS(rv, rv);

  return NotifySuccess(wrappedMessage);
}

nsresult
MobileMessageCallback::NotifyError(int32_t aError)
{
  switch (aError) {
    case nsIMobileMessageCallback::NO_SIGNAL_ERROR:
      mDOMRequest->FireError(NS_LITERAL_STRING("NoSignalError"));
      break;
    case nsIMobileMessageCallback::NOT_FOUND_ERROR:
      mDOMRequest->FireError(NS_LITERAL_STRING("NotFoundError"));
      break;
    case nsIMobileMessageCallback::UNKNOWN_ERROR:
      mDOMRequest->FireError(NS_LITERAL_STRING("UnknownError"));
      break;
    case nsIMobileMessageCallback::INTERNAL_ERROR:
      mDOMRequest->FireError(NS_LITERAL_STRING("InternalError"));
      break;
    default: // SUCCESS_NO_ERROR is handled above.
      MOZ_NOT_REACHED("Should never get here!");
      return NS_ERROR_FAILURE;
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
MobileMessageCallback::NotifyMessageDeleted(bool *aDeleted, uint32_t aSize)
{
  if (aSize == 1) {
    return NotifySuccess(aDeleted[0] ? JSVAL_TRUE : JSVAL_FALSE);
  }

  nsresult rv;
  nsIScriptContext* sc = mDOMRequest->GetContextForEventHandlers(&rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(sc, NS_ERROR_FAILURE);

  AutoPushJSContext cx(sc->GetNativeContext());
  NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

  JSObject *deleteArrayObj = JS_NewArrayObject(cx, aSize, NULL);
  JS::Value jsValTrue = BOOLEAN_TO_JSVAL(1);
  JS::Value jsValFalse = BOOLEAN_TO_JSVAL(0);
  for (uint32_t i = 0; i < aSize; i++) {
    JS_SetElement(cx, deleteArrayObj, i,
                  aDeleted[i] ? &jsValTrue : &jsValFalse);
  }

  return NotifySuccess(OBJECT_TO_JSVAL(deleteArrayObj));
}

NS_IMETHODIMP
MobileMessageCallback::NotifyDeleteMessageFailed(int32_t aError)
{
  return NotifyError(aError);
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

} // namesapce mobilemessage
} // namespace dom
} // namespace mozilla
