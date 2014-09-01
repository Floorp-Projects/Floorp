/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsMessage.h"
#include "nsIDOMClassInfo.h"
#include "jsapi.h" // For OBJECT_TO_JSVAL and JS_NewDateObjectMsec
#include "jsfriendapi.h" // For js_DateGetMsecSinceEpoch
#include "mozilla/dom/mobilemessage/Constants.h" // For MessageType

using namespace mozilla::dom::mobilemessage;

DOMCI_DATA(MozSmsMessage, mozilla::dom::SmsMessage)

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(SmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozSmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozSmsMessage)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(SmsMessage)
NS_IMPL_RELEASE(SmsMessage)

SmsMessage::SmsMessage(int32_t aId,
                       uint64_t aThreadId,
                       const nsString& aIccId,
                       DeliveryState aDelivery,
                       DeliveryStatus aDeliveryStatus,
                       const nsString& aSender,
                       const nsString& aReceiver,
                       const nsString& aBody,
                       MessageClass aMessageClass,
                       uint64_t aTimestamp,
                       uint64_t aSentTimestamp,
                       uint64_t aDeliveryTimestamp,
                       bool aRead)
  : mData(aId, aThreadId, aIccId, aDelivery, aDeliveryStatus,
          aSender, aReceiver, aBody, aMessageClass, aTimestamp, aSentTimestamp,
          aDeliveryTimestamp, aRead)
{
}

SmsMessage::SmsMessage(const SmsMessageData& aData)
  : mData(aData)
{
}

/* static */ nsresult
SmsMessage::Create(int32_t aId,
                   uint64_t aThreadId,
                   const nsAString& aIccId,
                   const nsAString& aDelivery,
                   const nsAString& aDeliveryStatus,
                   const nsAString& aSender,
                   const nsAString& aReceiver,
                   const nsAString& aBody,
                   const nsAString& aMessageClass,
                   uint64_t aTimestamp,
                   uint64_t aSentTimestamp,
                   uint64_t aDeliveryTimestamp,
                   bool aRead,
                   JSContext* aCx,
                   nsIDOMMozSmsMessage** aMessage)
{
  *aMessage = nullptr;

  // SmsMessageData exposes these as references, so we can simply assign
  // to them.
  SmsMessageData data;
  data.id() = aId;
  data.threadId() = aThreadId;
  data.iccId() = nsString(aIccId);
  data.sender() = nsString(aSender);
  data.receiver() = nsString(aReceiver);
  data.body() = nsString(aBody);
  data.read() = aRead;

  if (aDelivery.Equals(DELIVERY_RECEIVED)) {
    data.delivery() = eDeliveryState_Received;
  } else if (aDelivery.Equals(DELIVERY_SENDING)) {
    data.delivery() = eDeliveryState_Sending;
  } else if (aDelivery.Equals(DELIVERY_SENT)) {
    data.delivery() = eDeliveryState_Sent;
  } else if (aDelivery.Equals(DELIVERY_ERROR)) {
    data.delivery() = eDeliveryState_Error;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  if (aDeliveryStatus.Equals(DELIVERY_STATUS_NOT_APPLICABLE)) {
    data.deliveryStatus() = eDeliveryStatus_NotApplicable;
  } else if (aDeliveryStatus.Equals(DELIVERY_STATUS_SUCCESS)) {
    data.deliveryStatus() = eDeliveryStatus_Success;
  } else if (aDeliveryStatus.Equals(DELIVERY_STATUS_PENDING)) {
    data.deliveryStatus() = eDeliveryStatus_Pending;
  } else if (aDeliveryStatus.Equals(DELIVERY_STATUS_ERROR)) {
    data.deliveryStatus() = eDeliveryStatus_Error;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  if (aMessageClass.Equals(MESSAGE_CLASS_NORMAL)) {
    data.messageClass() = eMessageClass_Normal;
  } else if (aMessageClass.Equals(MESSAGE_CLASS_CLASS_0)) {
    data.messageClass() = eMessageClass_Class0;
  } else if (aMessageClass.Equals(MESSAGE_CLASS_CLASS_1)) {
    data.messageClass() = eMessageClass_Class1;
  } else if (aMessageClass.Equals(MESSAGE_CLASS_CLASS_2)) {
    data.messageClass() = eMessageClass_Class2;
  } else if (aMessageClass.Equals(MESSAGE_CLASS_CLASS_3)) {
    data.messageClass() = eMessageClass_Class3;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  // Set |timestamp|.
  data.timestamp() = aTimestamp;

  // Set |sentTimestamp|.
  data.sentTimestamp() = aSentTimestamp;

  // Set |deliveryTimestamp|.
  data.deliveryTimestamp() = aDeliveryTimestamp;

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
SmsMessage::GetType(nsAString& aType)
{
  aType = NS_LITERAL_STRING("sms");
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetId(int32_t* aId)
{
  *aId = mData.id();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetThreadId(uint64_t* aThreadId)
{
  *aThreadId = mData.threadId();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetIccId(nsAString& aIccId)
{
  aIccId = mData.iccId();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetDelivery(nsAString& aDelivery)
{
  switch (mData.delivery()) {
    case eDeliveryState_Received:
      aDelivery = DELIVERY_RECEIVED;
      break;
    case eDeliveryState_Sending:
      aDelivery = DELIVERY_SENDING;
      break;
    case eDeliveryState_Sent:
      aDelivery = DELIVERY_SENT;
      break;
    case eDeliveryState_Error:
      aDelivery = DELIVERY_ERROR;
      break;
    case eDeliveryState_Unknown:
    case eDeliveryState_EndGuard:
    default:
      MOZ_CRASH("We shouldn't get any other delivery state!");
  }

  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetDeliveryStatus(nsAString& aDeliveryStatus)
{
  switch (mData.deliveryStatus()) {
    case eDeliveryStatus_NotApplicable:
      aDeliveryStatus = DELIVERY_STATUS_NOT_APPLICABLE;
      break;
    case eDeliveryStatus_Success:
      aDeliveryStatus = DELIVERY_STATUS_SUCCESS;
      break;
    case eDeliveryStatus_Pending:
      aDeliveryStatus = DELIVERY_STATUS_PENDING;
      break;
    case eDeliveryStatus_Error:
      aDeliveryStatus = DELIVERY_STATUS_ERROR;
      break;
    case eDeliveryStatus_EndGuard:
    default:
      MOZ_CRASH("We shouldn't get any other delivery status!");
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
SmsMessage::GetMessageClass(nsAString& aMessageClass)
{
  switch (mData.messageClass()) {
    case eMessageClass_Normal:
      aMessageClass = MESSAGE_CLASS_NORMAL;
      break;
    case eMessageClass_Class0:
      aMessageClass = MESSAGE_CLASS_CLASS_0;
      break;
    case eMessageClass_Class1:
      aMessageClass = MESSAGE_CLASS_CLASS_1;
      break;
    case eMessageClass_Class2:
      aMessageClass = MESSAGE_CLASS_CLASS_2;
      break;
    case eMessageClass_Class3:
      aMessageClass = MESSAGE_CLASS_CLASS_3;
      break;
    default:
      MOZ_CRASH("We shouldn't get any other message class!");
  }

  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetTimestamp(DOMTimeStamp* aTimestamp)
{
  *aTimestamp = mData.timestamp();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetSentTimestamp(DOMTimeStamp* aSentTimestamp)
{
  *aSentTimestamp = mData.sentTimestamp();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetDeliveryTimestamp(DOMTimeStamp* aDate)
{
  *aDate = mData.deliveryTimestamp();
  return NS_OK;
}

NS_IMETHODIMP
SmsMessage::GetRead(bool* aRead)
{
  *aRead = mData.read();
  return NS_OK;
}

} // namespace dom
} // namespace mozilla
