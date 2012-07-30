/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsMessage.h"
#include "nsIDOMClassInfo.h"
#include "jsapi.h" // For OBJECT_TO_JSVAL and JS_NewDateObjectMsec
#include "jsfriendapi.h" // For js_DateGetMsecSinceEpoch
#include "Constants.h"

DOMCI_DATA(MozSmsMessage, mozilla::dom::sms::SmsMessage)

namespace mozilla {
namespace dom {
namespace sms {

NS_INTERFACE_MAP_BEGIN(SmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsMessage)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(SmsMessage)
NS_IMPL_RELEASE(SmsMessage)

SmsMessage::SmsMessage(PRInt32 aId, DeliveryState aDelivery,
                       const nsString& aSender, const nsString& aReceiver,
                       const nsString& aBody, PRUint64 aTimestamp, bool aRead)
  : mData(aId, aDelivery, aSender, aReceiver, aBody, aTimestamp, aRead)
{
}

SmsMessage::SmsMessage(const SmsMessageData& aData)
  : mData(aData)
{
}

/* static */ nsresult
SmsMessage::Create(PRInt32 aId,
                   const nsAString& aDelivery,
                   const nsAString& aSender,
                   const nsAString& aReceiver,
                   const nsAString& aBody,
                   const jsval& aTimestamp,
                   const bool aRead,
                   JSContext* aCx,
                   nsIDOMMozSmsMessage** aMessage)
{
  *aMessage = nullptr;

  // SmsMessageData exposes these as references, so we can simply assign
  // to them.
  SmsMessageData data;
  data.id() = aId;
  data.sender() = nsString(aSender);
  data.receiver() = nsString(aReceiver);
  data.body() = nsString(aBody);
  data.read() = aRead;

  if (aDelivery.Equals(DELIVERY_RECEIVED)) {
    data.delivery() = eDeliveryState_Received;
  } else if (aDelivery.Equals(DELIVERY_SENT)) {
    data.delivery() = eDeliveryState_Sent;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  // We support both a Date object and a millisecond timestamp as a number.
  if (aTimestamp.isObject()) {
    JSObject& obj = aTimestamp.toObject();
    if (!JS_ObjectIsDate(aCx, &obj)) {
      return NS_ERROR_INVALID_ARG;
    }
    data.timestamp() = js_DateGetMsecSinceEpoch(aCx, &obj);
  } else {
    if (!aTimestamp.isNumber()) {
      return NS_ERROR_INVALID_ARG;
    }
    double number = aTimestamp.toNumber();
    if (static_cast<PRUint64>(number) != number) {
      return NS_ERROR_INVALID_ARG;
    }
    data.timestamp() = static_cast<PRUint64>(number);
  }

  nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(data);
  message.swap(*aMessage);
  return NS_OK;
}

const SmsMessageData&
SmsMessage::GetData() const
{
  return mData;
}

NS_IMETHODIMP
SmsMessage::GetId(PRInt32* aId)
{
  *aId = mData.id();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetDelivery(nsAString& aDelivery)
{
  switch (mData.delivery()) {
    case eDeliveryState_Received:
      aDelivery = DELIVERY_RECEIVED;
      break;
    case eDeliveryState_Sent:
      aDelivery = DELIVERY_SENT;
      break;
    case eDeliveryState_Unknown:
    case eDeliveryState_EndGuard:
    default:
      NS_ASSERTION(true, "We shouldn't get any other delivery state!");
      return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetSender(nsAString& aSender)
{
  aSender = mData.sender();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetReceiver(nsAString& aReceiver)
{
  aReceiver = mData.receiver();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetBody(nsAString& aBody)
{
  aBody = mData.body();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetTimestamp(JSContext* cx, jsval* aDate)
{
  *aDate = OBJECT_TO_JSVAL(JS_NewDateObjectMsec(cx, mData.timestamp()));
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetRead(bool* aRead)
{
  *aRead = mData.read();
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
