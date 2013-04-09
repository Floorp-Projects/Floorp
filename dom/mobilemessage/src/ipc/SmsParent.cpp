/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsParent.h"
#include "nsISmsService.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "Constants.h"
#include "nsIDOMMozSmsMessage.h"
#include "mozilla/unused.h"
#include "SmsMessage.h"
#include "nsIMobileMessageDatabaseService.h"
#include "SmsFilter.h"
#include "SmsSegmentInfo.h"
#include "MobileMessageThread.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

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
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-received' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyReceivedMessage(static_cast<SmsMessage*>(message.get())->GetData());
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsSendingObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-sending' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifySendingMessage(static_cast<SmsMessage*>(message.get())->GetData());
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsSentObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-sent' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifySentMessage(static_cast<SmsMessage*>(message.get())->GetData());
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsFailedObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-failed' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyFailedMessage(static_cast<SmsMessage*>(message.get())->GetData());
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsDeliverySuccessObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-delivery-success' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyDeliverySuccessMessage(static_cast<SmsMessage*>(message.get())->GetData());
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsDeliveryErrorObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-delivery-error' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyDeliveryErrorMessage(static_cast<SmsMessage*>(message.get())->GetData());
    return NS_OK;
  }

  return NS_OK;
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
  nsresult rv = NS_ERROR_FAILURE;

  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  if (smsService) {
    rv = smsService->Send(aRequest.number(), aRequest.message(), this);
  }

  if (NS_FAILED(rv)) {
    return NS_SUCCEEDED(NotifySendMessageFailed(nsIMobileMessageCallback::INTERNAL_ERROR));
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
    rv = dbService->DeleteMessage(aRequest.messageId(), this);
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
  SmsMessage* message = static_cast<SmsMessage*>(aMessage);
  return SendReply(ReplyMessageSend(message->GetData()));
}

NS_IMETHODIMP
SmsRequestParent::NotifySendMessageFailed(int32_t aError)
{
  return SendReply(ReplyMessageSendFail(aError));
}

NS_IMETHODIMP
SmsRequestParent::NotifyMessageGot(nsISupports *aMessage)
{
  SmsMessage* message = static_cast<SmsMessage*>(aMessage);
  return SendReply(ReplyGetMessage(message->GetData()));
}

NS_IMETHODIMP
SmsRequestParent::NotifyGetMessageFailed(int32_t aError)
{
  return SendReply(ReplyGetMessageFail(aError));
}

NS_IMETHODIMP
SmsRequestParent::NotifyMessageDeleted(bool aDeleted)
{
  return SendReply(ReplyMessageDelete(aDeleted));
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

  nsCOMPtr<nsIDOMMozSmsMessage> iMessage = do_QueryInterface(aResult);
  if (iMessage) {
    SmsMessage* message = static_cast<SmsMessage*>(aResult);
    return SendNotifyResult(MobileMessageCursorData(message->GetData()))
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
