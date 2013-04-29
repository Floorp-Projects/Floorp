/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsParent.h"
#include "nsISmsService.h"
#include "nsIMmsService.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "Constants.h"
#include "nsIDOMMozSmsMessage.h"
#include "nsIDOMMozMmsMessage.h"
#include "mozilla/unused.h"
#include "SmsMessage.h"
#include "MmsMessage.h"
#include "nsIMobileMessageDatabaseService.h"
#include "SmsFilter.h"
#include "SmsSegmentInfo.h"
#include "MobileMessageThread.h"
#include "nsIDOMFile.h"
#include "mozilla/dom/ipc/Blob.h"
#include "mozilla/dom/ContentParent.h"
#include "nsTArrayHelpers.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

static JSObject*
MmsAttachmentDataToJSObject(JSContext* aContext,
                            const MmsAttachmentData& aAttachment)
{
  JSAutoRequest ar(aContext);

  JS::Rooted<JSObject*> obj(aContext, JS_NewObject(aContext, nullptr, nullptr, nullptr));
  NS_ENSURE_TRUE(obj, nullptr);

  JSString* idStr = JS_NewUCStringCopyN(aContext,
                                        aAttachment.id().get(),
                                        aAttachment.id().Length());
  NS_ENSURE_TRUE(idStr, nullptr);
  if (!JS_DefineProperty(aContext, obj, "id", JS::StringValue(idStr),
                         nullptr, nullptr, 0)) {
    return nullptr;
  }

  JSString* locStr = JS_NewUCStringCopyN(aContext,
                                         aAttachment.location().get(),
                                         aAttachment.location().Length());
  NS_ENSURE_TRUE(locStr, nullptr);
  if (!JS_DefineProperty(aContext, obj, "location", JS::StringValue(locStr),
                         nullptr, nullptr, 0)) {
    return nullptr;
  }

  nsCOMPtr<nsIDOMBlob> blob = static_cast<BlobParent*>(aAttachment.contentParent())->GetBlob();
  JS::Value content;
  JS::Rooted<JSObject*> global (aContext, JS_GetGlobalForScopeChain(aContext));
  nsresult rv = nsContentUtils::WrapNative(aContext,
                                           global,
                                           blob,
                                           &NS_GET_IID(nsIDOMBlob),
                                           &content);
  NS_ENSURE_SUCCESS(rv, nullptr);
  if (!JS_DefineProperty(aContext, obj, "content", content,
                         nullptr, nullptr, 0)) {
    return nullptr;
  }

  return obj;
}

static bool
GetParamsFromSendMmsMessageRequest(JSContext* aCx,
                                   const SendMmsMessageRequest& aRequest,
                                   JS::Value* aParam)
{
  JSAutoRequest ar(aCx);

  JS::Rooted<JSObject*> paramsObj(aCx, JS_NewObject(aCx, nullptr, nullptr, nullptr));
  NS_ENSURE_TRUE(paramsObj, false);

  // smil
  JSString* smilStr = JS_NewUCStringCopyN(aCx,
                                          aRequest.smil().get(),
                                          aRequest.smil().Length());
  NS_ENSURE_TRUE(smilStr, false);
  if(!JS_DefineProperty(aCx, paramsObj, "smil", JS::StringValue(smilStr),
                        nullptr, nullptr, 0)) {
    return false;
  }

  // subject
  JSString* subjectStr = JS_NewUCStringCopyN(aCx,
                                             aRequest.subject().get(),
                                             aRequest.subject().Length());
  NS_ENSURE_TRUE(subjectStr, false);
  if(!JS_DefineProperty(aCx, paramsObj, "subject",
                        JS::StringValue(subjectStr), nullptr, nullptr, 0)) {
    return false;
  }

  // receivers
  JS::Rooted<JSObject*> receiverArray(aCx);
  if (NS_FAILED(nsTArrayToJSArray(aCx,
                                  aRequest.receivers(),
                                  receiverArray.address()))) {
    return false;
  }
  if (!JS_DefineProperty(aCx, paramsObj, "receivers",
                         JS::ObjectValue(*receiverArray), nullptr, nullptr, 0)) {
    return false;
  }

  // attachments
  JS::Rooted<JSObject*> attachmentArray(aCx, JS_NewArrayObject(aCx,
                                                               aRequest.attachments().Length(),
                                                               nullptr));
  for (uint32_t i = 0; i < aRequest.attachments().Length(); i++) {
    JS::Rooted<JSObject*> obj(aCx,
      MmsAttachmentDataToJSObject(aCx, aRequest.attachments().ElementAt(i)));
    NS_ENSURE_TRUE(obj, false);
    JS::Rooted<JS::Value> val(aCx, JS::ObjectValue(*obj));
    if (!JS_SetElement(aCx, attachmentArray, i, val.address())) {
      return false;
    }
  }

  if (!JS_DefineProperty(aCx, paramsObj, "attachments",
                         JS::ObjectValue(*attachmentArray), nullptr, nullptr, 0)) {
    return false;
  }

  aParam->setObject(*paramsObj);
  return true;
}

NS_IMPL_ISUPPORTS1(SmsParent, nsIObserver)

SmsParent::SmsParent()
{
  MOZ_COUNT_CTOR(SmsParent);
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return;
  }

  obs->AddObserver(this, kSmsReceivedObserverTopic, false);
  obs->AddObserver(this, kSmsSendingObserverTopic, false);
  obs->AddObserver(this, kSmsSentObserverTopic, false);
  obs->AddObserver(this, kSmsFailedObserverTopic, false);
  obs->AddObserver(this, kSmsDeliverySuccessObserverTopic, false);
  obs->AddObserver(this, kSmsDeliveryErrorObserverTopic, false);
}

void
SmsParent::ActorDestroy(ActorDestroyReason why)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return;
  }

  obs->RemoveObserver(this, kSmsReceivedObserverTopic);
  obs->RemoveObserver(this, kSmsSendingObserverTopic);
  obs->RemoveObserver(this, kSmsSentObserverTopic);
  obs->RemoveObserver(this, kSmsFailedObserverTopic);
  obs->RemoveObserver(this, kSmsDeliverySuccessObserverTopic);
  obs->RemoveObserver(this, kSmsDeliveryErrorObserverTopic);
}

NS_IMETHODIMP
SmsParent::Observe(nsISupports* aSubject, const char* aTopic,
                   const PRUnichar* aData)
{
  if (!strcmp(aTopic, kSmsReceivedObserverTopic)) {
    MobileMessageData msgData;
    if (!GetMobileMessageDataFromMessage(aSubject, msgData)) {
      NS_ERROR("Got a 'sms-received' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyReceivedMessage(msgData);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsSendingObserverTopic)) {
    MobileMessageData msgData;
    if (!GetMobileMessageDataFromMessage(aSubject, msgData)) {
      NS_ERROR("Got a 'sms-sending' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifySendingMessage(msgData);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsSentObserverTopic)) {
    MobileMessageData msgData;
    if (!GetMobileMessageDataFromMessage(aSubject, msgData)) {
      NS_ERROR("Got a 'sms-sent' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifySentMessage(msgData);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsFailedObserverTopic)) {
    MobileMessageData msgData;
    if (!GetMobileMessageDataFromMessage(aSubject, msgData)) {
      NS_ERROR("Got a 'sms-failed' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyFailedMessage(msgData);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsDeliverySuccessObserverTopic)) {
    MobileMessageData msgData;
    if (!GetMobileMessageDataFromMessage(aSubject, msgData)) {
      NS_ERROR("Got a 'sms-sending' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyDeliverySuccessMessage(msgData);
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsDeliveryErrorObserverTopic)) {
    MobileMessageData msgData;
    if (!GetMobileMessageDataFromMessage(aSubject, msgData)) {
      NS_ERROR("Got a 'sms-delivery-error' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyDeliveryErrorMessage(msgData);
    return NS_OK;
  }

  return NS_OK;
}

bool
SmsParent::GetMobileMessageDataFromMessage(nsISupports *aMsg,
                                           MobileMessageData &aData)
{
  nsCOMPtr<nsIDOMMozMmsMessage> mmsMsg = do_QueryInterface(aMsg);
  if (mmsMsg) {
    MmsMessageData data;
    ContentParent *parent = static_cast<ContentParent*>(Manager());
    if (!static_cast<MmsMessage*>(mmsMsg.get())->GetData(parent, data)) {
      return false;
    }
    aData = data;
    return true;
  }

  nsCOMPtr<nsIDOMMozSmsMessage> smsMsg = do_QueryInterface(aMsg);
  if (smsMsg) {
    aData = static_cast<SmsMessage*>(smsMsg.get())->GetData();
    return true;
  }

  NS_WARNING("Cannot get MobileMessageData");
  return false;
}

bool
SmsParent::RecvHasSupport(bool* aHasSupport)
{
  *aHasSupport = false;

  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, true);

  smsService->HasSupport(aHasSupport);
  return true;
}

bool
SmsParent::RecvGetSegmentInfoForText(const nsString& aText,
                                     SmsSegmentInfoData* aResult)
{
  aResult->segments() = 0;
  aResult->charsPerSegment() = 0;
  aResult->charsAvailableInLastSegment() = 0;

  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, true);

  nsCOMPtr<nsIDOMMozSmsSegmentInfo> info;
  nsresult rv = smsService->GetSegmentInfoForText(aText, getter_AddRefs(info));
  NS_ENSURE_SUCCESS(rv, true);

  int segments, charsPerSegment, charsAvailableInLastSegment;
  if (NS_FAILED(info->GetSegments(&segments)) ||
      NS_FAILED(info->GetCharsPerSegment(&charsPerSegment)) ||
      NS_FAILED(info->GetCharsAvailableInLastSegment(&charsAvailableInLastSegment))) {
    NS_ERROR("Can't get attribute values from nsIDOMMozSmsSegmentInfo");
    return true;
  }

  aResult->segments() = segments;
  aResult->charsPerSegment() = charsPerSegment;
  aResult->charsAvailableInLastSegment() = charsAvailableInLastSegment;
  return true;
}

bool
SmsParent::RecvPSmsRequestConstructor(PSmsRequestParent* aActor,
                                      const IPCSmsRequest& aRequest)
{
  SmsRequestParent* actor = static_cast<SmsRequestParent*>(aActor);

  switch (aRequest.type()) {
    case IPCSmsRequest::TSendMessageRequest:
      return actor->DoRequest(aRequest.get_SendMessageRequest());
    case IPCSmsRequest::TRetrieveMessageRequest:
      return actor->DoRequest(aRequest.get_RetrieveMessageRequest());
    case IPCSmsRequest::TGetMessageRequest:
      return actor->DoRequest(aRequest.get_GetMessageRequest());
    case IPCSmsRequest::TDeleteMessageRequest:
      return actor->DoRequest(aRequest.get_DeleteMessageRequest());
    case IPCSmsRequest::TMarkMessageReadRequest:
      return actor->DoRequest(aRequest.get_MarkMessageReadRequest());
    default:
      MOZ_NOT_REACHED("Unknown type!");
      break;
  }

  return false;
}

PSmsRequestParent*
SmsParent::AllocPSmsRequest(const IPCSmsRequest& aRequest)
{
  SmsRequestParent* actor = new SmsRequestParent();
  // Add an extra ref for IPDL. Will be released in
  // SmsParent::DeallocPSmsRequest().
  actor->AddRef();

  return actor;
}

bool
SmsParent::DeallocPSmsRequest(PSmsRequestParent* aActor)
{
  // SmsRequestParent is refcounted, must not be freed manually.
  static_cast<SmsRequestParent*>(aActor)->Release();
  return true;
}

bool
SmsParent::RecvPMobileMessageCursorConstructor(PMobileMessageCursorParent* aActor,
                                               const IPCMobileMessageCursor& aRequest)
{
  MobileMessageCursorParent* actor =
    static_cast<MobileMessageCursorParent*>(aActor);

  switch (aRequest.type()) {
    case IPCMobileMessageCursor::TCreateMessageCursorRequest:
      return actor->DoRequest(aRequest.get_CreateMessageCursorRequest());
    case IPCMobileMessageCursor::TCreateThreadCursorRequest:
      return actor->DoRequest(aRequest.get_CreateThreadCursorRequest());
    default:
      MOZ_NOT_REACHED("Unknown type!");
      break;
  }

  return false;
}

PMobileMessageCursorParent*
SmsParent::AllocPMobileMessageCursor(const IPCMobileMessageCursor& aRequest)
{
  MobileMessageCursorParent* actor = new MobileMessageCursorParent();
  // Add an extra ref for IPDL. Will be released in
  // SmsParent::DeallocPMobileMessageCursor().
  actor->AddRef();

  return actor;
}

bool
SmsParent::DeallocPMobileMessageCursor(PMobileMessageCursorParent* aActor)
{
  // MobileMessageCursorParent is refcounted, must not be freed manually.
  static_cast<MobileMessageCursorParent*>(aActor)->Release();
  return true;
}

/*******************************************************************************
 * SmsRequestParent
 ******************************************************************************/

NS_IMPL_ISUPPORTS1(SmsRequestParent, nsIMobileMessageCallback)

void
SmsRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
}

bool
SmsRequestParent::DoRequest(const SendMessageRequest& aRequest)
{
  switch(aRequest.type()) {
  case SendMessageRequest::TSendSmsMessageRequest: {
      nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
      NS_ENSURE_TRUE(smsService, true);

      const SendSmsMessageRequest &data = aRequest.get_SendSmsMessageRequest();
      smsService->Send(data.number(), data.message(), this);
    }
    break;
  case SendMessageRequest::TSendMmsMessageRequest: {
      nsCOMPtr<nsIMmsService> mmsService = do_GetService(MMS_SERVICE_CONTRACTID);
      NS_ENSURE_TRUE(mmsService, true);

      AutoJSContext cx;
      JS::Rooted<JS::Value> params(cx);
      if (!GetParamsFromSendMmsMessageRequest(
              cx,
              aRequest.get_SendMmsMessageRequest(),
              params.address())) {
        NS_WARNING("SmsRequestParent: Fail to build MMS params.");
        return true;
      }
      mmsService->Send(params, this);
    }
    break;
  default:
    MOZ_NOT_REACHED("Unknown type of SendMessageRequest!");
    return false;
  }
  return true;
}

bool
SmsRequestParent::DoRequest(const RetrieveMessageRequest& aRequest)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIMmsService> mmsService = do_GetService(MMS_SERVICE_CONTRACTID);
  if (mmsService) {
    rv = mmsService->Retrieve(aRequest.messageId(), this);
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(NotifyGetMessageFailed(nsIMobileMessageCallback::INTERNAL_ERROR));
  }

  return true;
}

bool
SmsRequestParent::DoRequest(const GetMessageRequest& aRequest)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (dbService) {
    rv = dbService->GetMessageMoz(aRequest.messageId(), this);
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(NotifyGetMessageFailed(nsIMobileMessageCallback::INTERNAL_ERROR));
  }

  return true;
}

bool
SmsRequestParent::DoRequest(const DeleteMessageRequest& aRequest)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (dbService) {
    const InfallibleTArray<int32_t>& messageIds = aRequest.messageIds();
    rv = dbService->DeleteMessage(const_cast<int32_t *>(messageIds.Elements()),
                                  messageIds.Length(), this);
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(NotifyDeleteMessageFailed(nsIMobileMessageCallback::INTERNAL_ERROR));
  }

  return true;
}

bool
SmsRequestParent::DoRequest(const MarkMessageReadRequest& aRequest)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (dbService) {
    rv = dbService->MarkMessageRead(aRequest.messageId(), aRequest.value(),
                                    this);
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(NotifyMarkMessageReadFailed(nsIMobileMessageCallback::INTERNAL_ERROR));
  }

  return true;
}

nsresult
SmsRequestParent::SendReply(const MessageReply& aReply)
{
  // The child process could die before this asynchronous notification, in which
  // case ActorDestroy() was called and mActorDestroyed is set to true. Return
  // an error here to avoid sending a message to the dead process.
  NS_ENSURE_TRUE(!mActorDestroyed, NS_ERROR_FAILURE);

  return Send__delete__(this, aReply) ? NS_OK : NS_ERROR_FAILURE;
}

// nsIMobileMessageCallback

NS_IMETHODIMP
SmsRequestParent::NotifyMessageSent(nsISupports *aMessage)
{
  nsCOMPtr<nsIDOMMozMmsMessage> mms = do_QueryInterface(aMessage);
  if (mms) {
    MmsMessage *msg = static_cast<MmsMessage*>(mms.get());
    ContentParent *parent = static_cast<ContentParent*>(Manager()->Manager());
    MmsMessageData data;
    if (!msg->GetData(parent, data)) {
      return NS_ERROR_FAILURE;
    }
    return SendReply(ReplyMessageSend(MobileMessageData(data)));
  }

  nsCOMPtr<nsIDOMMozSmsMessage> sms = do_QueryInterface(aMessage);
  if (sms) {
    SmsMessage* msg = static_cast<SmsMessage*>(sms.get());
    return SendReply(ReplyMessageSend(MobileMessageData(msg->GetData())));
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SmsRequestParent::NotifySendMessageFailed(int32_t aError)
{
  return SendReply(ReplyMessageSendFail(aError));
}

NS_IMETHODIMP
SmsRequestParent::NotifyMessageGot(nsISupports *aMessage)
{
  nsCOMPtr<nsIDOMMozMmsMessage> mms = do_QueryInterface(aMessage);
  if (mms) {
    MmsMessage *msg = static_cast<MmsMessage*>(mms.get());
    ContentParent *parent = static_cast<ContentParent*>(Manager()->Manager());
    MmsMessageData data;
    if (!msg->GetData(parent, data)) {
      return NS_ERROR_FAILURE;
    }
    return SendReply(ReplyGetMessage(MobileMessageData(data)));
  }

  nsCOMPtr<nsIDOMMozSmsMessage> sms = do_QueryInterface(aMessage);
  if (sms) {
    SmsMessage* msg = static_cast<SmsMessage*>(sms.get());
    return SendReply(ReplyGetMessage(MobileMessageData(msg->GetData())));
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SmsRequestParent::NotifyGetMessageFailed(int32_t aError)
{
  return SendReply(ReplyGetMessageFail(aError));
}

NS_IMETHODIMP
SmsRequestParent::NotifyMessageDeleted(bool *aDeleted, uint32_t aSize)
{
  ReplyMessageDelete data;
  data.deleted().AppendElements(aDeleted, aSize);
  return SendReply(data);
}

NS_IMETHODIMP
SmsRequestParent::NotifyDeleteMessageFailed(int32_t aError)
{
  return SendReply(ReplyMessageDeleteFail(aError));
}

NS_IMETHODIMP
SmsRequestParent::NotifyMessageMarkedRead(bool aRead)
{
  return SendReply(ReplyMarkeMessageRead(aRead));
}

NS_IMETHODIMP
SmsRequestParent::NotifyMarkMessageReadFailed(int32_t aError)
{
  return SendReply(ReplyMarkeMessageReadFail(aError));
}

/*******************************************************************************
 * MobileMessageCursorParent
 ******************************************************************************/

NS_IMPL_ISUPPORTS1(MobileMessageCursorParent, nsIMobileMessageCursorCallback)

void
MobileMessageCursorParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Two possible scenarios here:
  // 1) When parent fails to SendNotifyResult() in NotifyCursorResult(), it's
  //    destroyed without nulling out mContinueCallback.
  // 2) When parent dies normally, mContinueCallback should have been cleared in
  //    NotifyCursorError(), but just ensure this again.
  mContinueCallback = nullptr;
}

bool
MobileMessageCursorParent::RecvContinue()
{
  MOZ_ASSERT(mContinueCallback);

  if (NS_FAILED(mContinueCallback->HandleContinue())) {
    return NS_SUCCEEDED(NotifyCursorError(nsIMobileMessageCallback::INTERNAL_ERROR));
  }

  return true;
}

bool
MobileMessageCursorParent::DoRequest(const CreateMessageCursorRequest& aRequest)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (dbService) {
    nsCOMPtr<nsIDOMMozSmsFilter> filter = new SmsFilter(aRequest.filter());
    bool reverse = aRequest.reverse();

    rv = dbService->CreateMessageCursor(filter, reverse, this,
                                        getter_AddRefs(mContinueCallback));
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(NotifyCursorError(nsIMobileMessageCallback::INTERNAL_ERROR));
  }

  return true;
}

bool
MobileMessageCursorParent::DoRequest(const CreateThreadCursorRequest& aRequest)
{
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsIMobileMessageDatabaseService> dbService =
    do_GetService(MOBILE_MESSAGE_DATABASE_SERVICE_CONTRACTID);
  if (dbService) {
    rv = dbService->CreateThreadCursor(this,
                                       getter_AddRefs(mContinueCallback));
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(NotifyCursorError(nsIMobileMessageCallback::INTERNAL_ERROR));
  }

  return true;
}

// nsIMobileMessageCursorCallback

NS_IMETHODIMP
MobileMessageCursorParent::NotifyCursorError(int32_t aError)
{
  // The child process could die before this asynchronous notification, in which
  // case ActorDestroy() was called and mContinueCallback is now null. Return an
  // error here to avoid sending a message to the dead process.
  NS_ENSURE_TRUE(mContinueCallback, NS_ERROR_FAILURE);

  mContinueCallback = nullptr;

  return Send__delete__(this, aError) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileMessageCursorParent::NotifyCursorResult(nsISupports* aResult)
{
  // The child process could die before this asynchronous notification, in which
  // case ActorDestroy() was called and mContinueCallback is now null. Return an
  // error here to avoid sending a message to the dead process.
  NS_ENSURE_TRUE(mContinueCallback, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMMozSmsMessage> iSms = do_QueryInterface(aResult);
  if (iSms) {
    SmsMessage* message = static_cast<SmsMessage*>(aResult);
    return SendNotifyResult(MobileMessageCursorData(message->GetData()))
      ? NS_OK : NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMMozMmsMessage> iMms = do_QueryInterface(aResult);
  if (iMms) {
    MmsMessage* message = static_cast<MmsMessage*>(aResult);
    ContentParent* parent = static_cast<ContentParent*>(Manager()->Manager());
    MmsMessageData data;
    if (!message->GetData(parent, data)) {
      return NS_ERROR_FAILURE;
    }
    return SendNotifyResult(MobileMessageCursorData(data))
      ? NS_OK : NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMMozMobileMessageThread> iThread = do_QueryInterface(aResult);
  if (iThread) {
    MobileMessageThread* thread = static_cast<MobileMessageThread*>(aResult);
    return SendNotifyResult(MobileMessageCursorData(thread->GetData()))
      ? NS_OK : NS_ERROR_FAILURE;
  }

  MOZ_NOT_REACHED("Received invalid response parameters!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
MobileMessageCursorParent::NotifyCursorDone()
{
  return NotifyCursorError(nsIMobileMessageCallback::SUCCESS_NO_ERROR);
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
