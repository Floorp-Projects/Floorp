/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MmsMessage.h"
#include "nsIDOMClassInfo.h"
#include "jsapi.h" // For OBJECT_TO_JSVAL and JS_NewDateObjectMsec
#include "jsfriendapi.h" // For js_DateGetMsecSinceEpoch
#include "nsJSUtils.h"
#include "nsContentUtils.h"
#include "nsIDOMFile.h"
#include "nsTArrayHelpers.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/mobilemessage/Constants.h" // For MessageType
#include "mozilla/dom/mobilemessage/SmsTypes.h"
#include "nsDOMFile.h"
#include "nsCxPusher.h"
#include "MessageUtils.h"

using namespace mozilla::idl;
using namespace mozilla::dom::mobilemessage;

DOMCI_DATA(MozMmsMessage, mozilla::dom::MmsMessage)

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(MmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozMmsMessage)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozMmsMessage)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(MmsMessage)
NS_IMPL_RELEASE(MmsMessage)

MmsMessage::MmsMessage(int32_t                          aId,
                       uint64_t                         aThreadId,
                       const nsAString&                 aIccId,
                       DeliveryState                    aDelivery,
                       const nsTArray<MmsDeliveryInfo>& aDeliveryInfo,
                       const nsAString&                 aSender,
                       const nsTArray<nsString>&        aReceivers,
                       uint64_t                         aTimestamp,
                       uint64_t                         aSentTimestamp,
                       bool                             aRead,
                       const nsAString&                 aSubject,
                       const nsAString&                 aSmil,
                       const nsTArray<MmsAttachment>&   aAttachments,
                       uint64_t                         aExpiryDate,
                       bool                             aReadReportRequested)
  : mId(aId),
    mThreadId(aThreadId),
    mIccId(aIccId),
    mDelivery(aDelivery),
    mDeliveryInfo(aDeliveryInfo),
    mSender(aSender),
    mReceivers(aReceivers),
    mTimestamp(aTimestamp),
    mSentTimestamp(aSentTimestamp),
    mRead(aRead),
    mSubject(aSubject),
    mSmil(aSmil),
    mAttachments(aAttachments),
    mExpiryDate(aExpiryDate),
    mReadReportRequested(aReadReportRequested)
{
}

MmsMessage::MmsMessage(const mobilemessage::MmsMessageData& aData)
  : mId(aData.id())
  , mThreadId(aData.threadId())
  , mIccId(aData.iccId())
  , mDelivery(aData.delivery())
  , mSender(aData.sender())
  , mReceivers(aData.receivers())
  , mTimestamp(aData.timestamp())
  , mSentTimestamp(aData.sentTimestamp())
  , mRead(aData.read())
  , mSubject(aData.subject())
  , mSmil(aData.smil())
  , mExpiryDate(aData.expiryDate())
  , mReadReportRequested(aData.readReportRequested())
{
  uint32_t len = aData.attachments().Length();
  mAttachments.SetCapacity(len);
  for (uint32_t i = 0; i < len; i++) {
    MmsAttachment att;
    const MmsAttachmentData &element = aData.attachments()[i];
    att.id = element.id();
    att.location = element.location();
    if (element.contentParent()) {
      att.content = static_cast<BlobParent*>(element.contentParent())->GetBlob();
    } else if (element.contentChild()) {
      att.content = static_cast<BlobChild*>(element.contentChild())->GetBlob();
    } else {
      NS_WARNING("MmsMessage: Unable to get attachment content.");
    }
    mAttachments.AppendElement(att);
  }

  len = aData.deliveryInfo().Length();
  mDeliveryInfo.SetCapacity(len);
  for (uint32_t i = 0; i < len; i++) {
    MmsDeliveryInfo info;
    const MmsDeliveryInfoData &infoData = aData.deliveryInfo()[i];

    // Prepare |info.receiver|.
    info.receiver = infoData.receiver();

    // Prepare |info.deliveryStatus|.
    nsString statusStr;
    switch (infoData.deliveryStatus()) {
      case eDeliveryStatus_NotApplicable:
        statusStr = DELIVERY_STATUS_NOT_APPLICABLE;
        break;
      case eDeliveryStatus_Success:
        statusStr = DELIVERY_STATUS_SUCCESS;
        break;
      case eDeliveryStatus_Pending:
        statusStr = DELIVERY_STATUS_PENDING;
        break;
      case eDeliveryStatus_Error:
        statusStr = DELIVERY_STATUS_ERROR;
        break;
      case eDeliveryStatus_Reject:
        statusStr = DELIVERY_STATUS_REJECTED;
        break;
      case eDeliveryStatus_Manual:
        statusStr = DELIVERY_STATUS_MANUAL;
        break;
      case eDeliveryStatus_EndGuard:
      default:
        MOZ_CRASH("We shouldn't get any other delivery status!");
    }
    info.deliveryStatus = statusStr;

    // Prepare |info.deliveryTimestamp|.
    info.deliveryTimestamp = infoData.deliveryTimestamp();

    // Prepare |info.readStatus|.
    nsString statusReadString;
    switch(infoData.readStatus()) {
      case eReadStatus_NotApplicable:
        statusReadString = READ_STATUS_NOT_APPLICABLE;
        break;
      case eReadStatus_Success:
        statusReadString = READ_STATUS_SUCCESS;
        break;
      case eReadStatus_Pending:
        statusReadString = READ_STATUS_PENDING;
        break;
      case eReadStatus_Error:
        statusReadString = READ_STATUS_ERROR;
        break;
      case eReadStatus_EndGuard:
      default:
        MOZ_CRASH("We shouldn't get any other read status!");
    }
    info.readStatus = statusReadString;

    // Prepare |info.readTimestamp|.
    info.readTimestamp = infoData.readTimestamp();

    mDeliveryInfo.AppendElement(info);
  }
}

/* static */ nsresult
MmsMessage::Create(int32_t               aId,
                   uint64_t              aThreadId,
                   const nsAString&      aIccId,
                   const nsAString&      aDelivery,
                   const JS::Value&      aDeliveryInfo,
                   const nsAString&      aSender,
                   const JS::Value&      aReceivers,
                   const JS::Value&      aTimestamp,
                   const JS::Value&      aSentTimestamp,
                   bool                  aRead,
                   const nsAString&      aSubject,
                   const nsAString&      aSmil,
                   const JS::Value&      aAttachments,
                   const JS::Value&      aExpiryDate,
                   bool                  aIsReadReportRequested,
                   JSContext*            aCx,
                   nsIDOMMozMmsMessage** aMessage)
{
  *aMessage = nullptr;

  // Set |delivery|.
  DeliveryState delivery;
  if (aDelivery.Equals(DELIVERY_SENT)) {
    delivery = eDeliveryState_Sent;
  } else if (aDelivery.Equals(DELIVERY_RECEIVED)) {
    delivery = eDeliveryState_Received;
  } else if (aDelivery.Equals(DELIVERY_SENDING)) {
    delivery = eDeliveryState_Sending;
  } else if (aDelivery.Equals(DELIVERY_NOT_DOWNLOADED)) {
    delivery = eDeliveryState_NotDownloaded;
  } else if (aDelivery.Equals(DELIVERY_ERROR)) {
    delivery = eDeliveryState_Error;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  // Set |deliveryInfo|.
  if (!aDeliveryInfo.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  JS::Rooted<JSObject*> deliveryInfoObj(aCx, &aDeliveryInfo.toObject());
  if (!JS_IsArrayObject(aCx, deliveryInfoObj)) {
    return NS_ERROR_INVALID_ARG;
  }

  uint32_t length;
  JS_ALWAYS_TRUE(JS_GetArrayLength(aCx, deliveryInfoObj, &length));

  nsTArray<MmsDeliveryInfo> deliveryInfo;
  JS::Rooted<JS::Value> infoJsVal(aCx);
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(aCx, deliveryInfoObj, i, &infoJsVal) ||
        !infoJsVal.isObject()) {
      return NS_ERROR_INVALID_ARG;
    }

    MmsDeliveryInfo info;
    nsresult rv = info.Init(aCx, infoJsVal.address());
    NS_ENSURE_SUCCESS(rv, rv);

    deliveryInfo.AppendElement(info);
  }

  // Set |receivers|.
  if (!aReceivers.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  JS::Rooted<JSObject*> receiversObj(aCx, &aReceivers.toObject());
  if (!JS_IsArrayObject(aCx, receiversObj)) {
    return NS_ERROR_INVALID_ARG;
  }

  JS_ALWAYS_TRUE(JS_GetArrayLength(aCx, receiversObj, &length));

  nsTArray<nsString> receivers;
  JS::Rooted<JS::Value> receiverJsVal(aCx);
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(aCx, receiversObj, i, &receiverJsVal) ||
        !receiverJsVal.isString()) {
      return NS_ERROR_INVALID_ARG;
    }

    nsDependentJSString receiverStr;
    receiverStr.init(aCx, receiverJsVal.toString());
    receivers.AppendElement(receiverStr);
  }

  // Set |timestamp|.
  uint64_t timestamp;
  nsresult rv = convertTimeToInt(aCx, aTimestamp, timestamp);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set |sentTimestamp|.
  uint64_t sentTimestamp;
  rv = convertTimeToInt(aCx, aSentTimestamp, sentTimestamp);
  NS_ENSURE_SUCCESS(rv, rv);

  // Set |attachments|.
  if (!aAttachments.isObject()) {
    return NS_ERROR_INVALID_ARG;
  }
  JS::Rooted<JSObject*> attachmentsObj(aCx, &aAttachments.toObject());
  if (!JS_IsArrayObject(aCx, attachmentsObj)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsTArray<MmsAttachment> attachments;
  JS_ALWAYS_TRUE(JS_GetArrayLength(aCx, attachmentsObj, &length));

  JS::Rooted<JS::Value> attachmentJsVal(aCx);
  for (uint32_t i = 0; i < length; ++i) {
    if (!JS_GetElement(aCx, attachmentsObj, i, &attachmentJsVal)) {
      return NS_ERROR_INVALID_ARG;
    }

    MmsAttachment attachment;
    rv = attachment.Init(aCx, attachmentJsVal.address());
    NS_ENSURE_SUCCESS(rv, rv);

    attachments.AppendElement(attachment);
  }

  // Set |expiryDate|.
  uint64_t expiryDate;
  rv = convertTimeToInt(aCx, aExpiryDate, expiryDate);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMMozMmsMessage> message = new MmsMessage(aId,
                                                         aThreadId,
                                                         aIccId,
                                                         delivery,
                                                         deliveryInfo,
                                                         aSender,
                                                         receivers,
                                                         timestamp,
                                                         sentTimestamp,
                                                         aRead,
                                                         aSubject,
                                                         aSmil,
                                                         attachments,
                                                         expiryDate,
                                                         aIsReadReportRequested);
  message.forget(aMessage);
  return NS_OK;
}

bool
MmsMessage::GetData(ContentParent* aParent,
                    mobilemessage::MmsMessageData& aData)
{
  NS_ASSERTION(aParent, "aParent is null");

  aData.id() = mId;
  aData.threadId() = mThreadId;
  aData.iccId() = mIccId;
  aData.delivery() = mDelivery;
  aData.sender().Assign(mSender);
  aData.receivers() = mReceivers;
  aData.timestamp() = mTimestamp;
  aData.sentTimestamp() = mSentTimestamp;
  aData.read() = mRead;
  aData.subject() = mSubject;
  aData.smil() = mSmil;
  aData.expiryDate() = mExpiryDate;
  aData.readReportRequested() = mReadReportRequested;

  aData.deliveryInfo().SetCapacity(mDeliveryInfo.Length());
  for (uint32_t i = 0; i < mDeliveryInfo.Length(); i++) {
    MmsDeliveryInfoData infoData;
    const MmsDeliveryInfo &info = mDeliveryInfo[i];

    // Prepare |infoData.receiver|.
    infoData.receiver().Assign(info.receiver);

    // Prepare |infoData.deliveryStatus|.
    DeliveryStatus status;
    if (info.deliveryStatus.Equals(DELIVERY_STATUS_NOT_APPLICABLE)) {
      status = eDeliveryStatus_NotApplicable;
    } else if (info.deliveryStatus.Equals(DELIVERY_STATUS_SUCCESS)) {
      status = eDeliveryStatus_Success;
    } else if (info.deliveryStatus.Equals(DELIVERY_STATUS_PENDING)) {
      status = eDeliveryStatus_Pending;
    } else if (info.deliveryStatus.Equals(DELIVERY_STATUS_ERROR)) {
      status = eDeliveryStatus_Error;
    } else if (info.deliveryStatus.Equals(DELIVERY_STATUS_REJECTED)) {
      status = eDeliveryStatus_Reject;
    } else if (info.deliveryStatus.Equals(DELIVERY_STATUS_MANUAL)) {
      status = eDeliveryStatus_Manual;
    } else {
      return false;
    }
    infoData.deliveryStatus() = status;

    // Prepare |infoData.deliveryTimestamp|.
    infoData.deliveryTimestamp() = info.deliveryTimestamp;

    // Prepare |infoData.readStatus|.
    ReadStatus readStatus;
    if (info.readStatus.Equals(READ_STATUS_NOT_APPLICABLE)) {
      readStatus = eReadStatus_NotApplicable;
    } else if (info.readStatus.Equals(READ_STATUS_SUCCESS)) {
      readStatus = eReadStatus_Success;
    } else if (info.readStatus.Equals(READ_STATUS_PENDING)) {
      readStatus = eReadStatus_Pending;
    } else if (info.readStatus.Equals(READ_STATUS_ERROR)) {
      readStatus = eReadStatus_Error;
    } else {
      return false;
    }
    infoData.readStatus() = readStatus;

    // Prepare |infoData.readTimestamp|.
    infoData.readTimestamp() = info.readTimestamp;

    aData.deliveryInfo().AppendElement(infoData);
  }

  aData.attachments().SetCapacity(mAttachments.Length());
  for (uint32_t i = 0; i < mAttachments.Length(); i++) {
    MmsAttachmentData mma;
    const MmsAttachment &element = mAttachments[i];
    mma.id().Assign(element.id);
    mma.location().Assign(element.location);

    // This is a workaround. Sometimes the blob we get from the database
    // doesn't have a valid last modified date, making the ContentParent
    // send a "Mystery Blob" to the ContentChild. Attempting to get the
    // last modified date of blob can force that value to be initialized.
    nsDOMFileBase* file = static_cast<nsDOMFileBase*>(element.content.get());
    if (file->IsDateUnknown()) {
      uint64_t date;
      if (NS_FAILED(file->GetMozLastModifiedDate(&date))) {
        NS_WARNING("Failed to get last modified date!");
      }
    }

    mma.contentParent() = aParent->GetOrCreateActorForBlob(element.content);
    if (!mma.contentParent()) {
      return false;
    }
    aData.attachments().AppendElement(mma);
  }

  return true;
}

NS_IMETHODIMP
MmsMessage::GetType(nsAString& aType)
{
  aType = NS_LITERAL_STRING("mms");
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetId(int32_t* aId)
{
  *aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetThreadId(uint64_t* aThreadId)
{
  *aThreadId = mThreadId;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetIccId(nsAString& aIccId)
{
  aIccId = mIccId;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetDelivery(nsAString& aDelivery)
{
  switch (mDelivery) {
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
    case eDeliveryState_NotDownloaded:
      aDelivery = DELIVERY_NOT_DOWNLOADED;
      break;
    case eDeliveryState_Unknown:
    case eDeliveryState_EndGuard:
    default:
      MOZ_CRASH("We shouldn't get any other delivery state!");
  }

  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetDeliveryInfo(JSContext* aCx, JS::MutableHandle<JS::Value> aDeliveryInfo)
{
  // TODO Bug 850525 It'd be better to depend on the delivery of MmsMessage
  // to return a more correct value. Ex, if .delivery = 'received', we should
  // also make .deliveryInfo = null, since the .deliveryInfo is useless.
  uint32_t length = mDeliveryInfo.Length();
  if (length == 0) {
    aDeliveryInfo.setNull();
    return NS_OK;
  }

  JS::Rooted<JSObject*> deliveryInfo(
    aCx, JS_NewArrayObject(aCx, length, nullptr));
  NS_ENSURE_TRUE(deliveryInfo, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < length; ++i) {
    const MmsDeliveryInfo &info = mDeliveryInfo[i];

    JS::Rooted<JSObject*> infoJsObj(
      aCx, JS_NewObject(aCx, nullptr, nullptr, nullptr));
    NS_ENSURE_TRUE(infoJsObj, NS_ERROR_OUT_OF_MEMORY);

    JS::Rooted<JS::Value> tmpJsVal(aCx);
    JSString* tmpJsStr;

    // Get |info.receiver|.
    tmpJsStr = JS_NewUCStringCopyN(aCx,
                                   info.receiver.get(),
                                   info.receiver.Length());
    NS_ENSURE_TRUE(tmpJsStr, NS_ERROR_OUT_OF_MEMORY);

    tmpJsVal.setString(tmpJsStr);
    if (!JS_DefineProperty(aCx, infoJsObj, "receiver", tmpJsVal,
                           nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    // Get |info.deliveryStatus|.
    tmpJsStr = JS_NewUCStringCopyN(aCx,
                                   info.deliveryStatus.get(),
                                   info.deliveryStatus.Length());
    NS_ENSURE_TRUE(tmpJsStr, NS_ERROR_OUT_OF_MEMORY);

    tmpJsVal.setString(tmpJsStr);
    if (!JS_DefineProperty(aCx, infoJsObj, "deliveryStatus", tmpJsVal,
                           nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    // Get |info.deliveryTimestamp|.
    tmpJsVal.setNumber(static_cast<double>(info.deliveryTimestamp));
    if (!JS_DefineProperty(aCx, infoJsObj, "deliveryTimestamp", tmpJsVal,
                           nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    // Get |info.readStatus|.
    tmpJsStr = JS_NewUCStringCopyN(aCx,
                                   info.readStatus.get(),
                                   info.readStatus.Length());
    NS_ENSURE_TRUE(tmpJsStr, NS_ERROR_OUT_OF_MEMORY);

    tmpJsVal.setString(tmpJsStr);
    if (!JS_DefineProperty(aCx, infoJsObj, "readStatus", tmpJsVal,
                           nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    // Get |info.readTimestamp|.
    tmpJsVal.setNumber(static_cast<double>(info.readTimestamp));
    if (!JS_DefineProperty(aCx, infoJsObj, "readTimestamp", tmpJsVal,
                           nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    tmpJsVal = OBJECT_TO_JSVAL(infoJsObj);
    if (!JS_SetElement(aCx, deliveryInfo, i, &tmpJsVal)) {
      return NS_ERROR_FAILURE;
    }
  }

  aDeliveryInfo.setObject(*deliveryInfo);
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetSender(nsAString& aSender)
{
  aSender = mSender;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetReceivers(JSContext* aCx, JS::MutableHandle<JS::Value> aReceivers)
{
  JS::Rooted<JSObject*> reveiversObj(aCx);
  nsresult rv = nsTArrayToJSArray(aCx, mReceivers, reveiversObj.address());
  NS_ENSURE_SUCCESS(rv, rv);

  aReceivers.setObject(*reveiversObj);
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetTimestamp(DOMTimeStamp* aTimestamp)
{
  *aTimestamp = mTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetSentTimestamp(DOMTimeStamp* aSentTimestamp)
{
  *aSentTimestamp = mSentTimestamp;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetRead(bool* aRead)
{
  *aRead = mRead;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetSubject(nsAString& aSubject)
{
  aSubject = mSubject;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetSmil(nsAString& aSmil)
{
  aSmil = mSmil;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetAttachments(JSContext* aCx, JS::MutableHandle<JS::Value> aAttachments)
{
  uint32_t length = mAttachments.Length();

  JS::Rooted<JSObject*> attachments(
    aCx, JS_NewArrayObject(aCx, length, nullptr));
  NS_ENSURE_TRUE(attachments, NS_ERROR_OUT_OF_MEMORY);

  for (uint32_t i = 0; i < length; ++i) {
    const MmsAttachment &attachment = mAttachments[i];

    JS::Rooted<JSObject*> attachmentObj(
      aCx, JS_NewObject(aCx, nullptr, nullptr, nullptr));
    NS_ENSURE_TRUE(attachmentObj, NS_ERROR_OUT_OF_MEMORY);

    JS::Rooted<JS::Value> tmpJsVal(aCx);
    JSString* tmpJsStr;

    // Get |attachment.mId|.
    tmpJsStr = JS_NewUCStringCopyN(aCx,
                                   attachment.id.get(),
                                   attachment.id.Length());
    NS_ENSURE_TRUE(tmpJsStr, NS_ERROR_OUT_OF_MEMORY);

    tmpJsVal.setString(tmpJsStr);
    if (!JS_DefineProperty(aCx, attachmentObj, "id", tmpJsVal,
                           nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    // Get |attachment.mLocation|.
    tmpJsStr = JS_NewUCStringCopyN(aCx,
                                   attachment.location.get(),
                                   attachment.location.Length());
    NS_ENSURE_TRUE(tmpJsStr, NS_ERROR_OUT_OF_MEMORY);

    tmpJsVal.setString(tmpJsStr);
    if (!JS_DefineProperty(aCx, attachmentObj, "location", tmpJsVal,
                           nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    // Get |attachment.mContent|.
    JS::Rooted<JSObject*> global(aCx, JS::CurrentGlobalOrNull(aCx));
    nsresult rv = nsContentUtils::WrapNative(aCx,
                                             global,
                                             attachment.content,
                                             &NS_GET_IID(nsIDOMBlob),
                                             &tmpJsVal);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!JS_DefineProperty(aCx, attachmentObj, "content", tmpJsVal,
                           nullptr, nullptr, JSPROP_ENUMERATE)) {
      return NS_ERROR_FAILURE;
    }

    tmpJsVal = OBJECT_TO_JSVAL(attachmentObj);
    if (!JS_SetElement(aCx, attachments, i, &tmpJsVal)) {
      return NS_ERROR_FAILURE;
    }
  }

  aAttachments.setObject(*attachments);
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetExpiryDate(DOMTimeStamp* aExpiryDate)
{
  *aExpiryDate = mExpiryDate;
  return NS_OK;
}

NS_IMETHODIMP
MmsMessage::GetReadReportRequested(bool* aReadReportRequested)
{
  *aReadReportRequested = mReadReportRequested;
  return NS_OK;
}


} // namespace dom
} // namespace mozilla
