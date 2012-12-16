/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsParent.h"
#include "nsISmsService.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "Constants.h"
#include "nsIDOMSmsMessage.h"
#include "mozilla/unused.h"
#include "SmsMessage.h"
#include "nsISmsDatabaseService.h"
#include "SmsFilter.h"
#include "SmsRequest.h"

namespace mozilla {
namespace dom {
namespace sms {

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

SmsParent::~SmsParent()
{
  MOZ_COUNT_DTOR(SmsParent);
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
SmsParent::RecvGetNumberOfMessagesForText(const nsString& aText, uint16_t* aResult)
{
  *aResult = 0;

  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, true);

  smsService->GetNumberOfMessagesForText(aText, aResult);
  return true;
}

bool
SmsParent::RecvClearMessageList(const int32_t& aListId)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  smsDBService->ClearMessageList(aListId);
  return true;
}

bool
SmsParent::RecvPSmsRequestConstructor(PSmsRequestParent* aActor,
                                      const IPCSmsRequest& aRequest)
{
  SmsRequestParent* actor = static_cast<SmsRequestParent*>(aActor);

  switch (aRequest.type()) {
    case IPCSmsRequest::TCreateMessageListRequest:
      return actor->DoRequest(aRequest.get_CreateMessageListRequest());
    case IPCSmsRequest::TSendMessageRequest:
      return actor->DoRequest(aRequest.get_SendMessageRequest());
    case IPCSmsRequest::TGetMessageRequest:
      return actor->DoRequest(aRequest.get_GetMessageRequest());
    case IPCSmsRequest::TDeleteMessageRequest:
      return actor->DoRequest(aRequest.get_DeleteMessageRequest());
    case IPCSmsRequest::TGetNextMessageInListRequest:
      return actor->DoRequest(aRequest.get_GetNextMessageInListRequest());
    case IPCSmsRequest::TMarkMessageReadRequest:
      return actor->DoRequest(aRequest.get_MarkMessageReadRequest());
    case IPCSmsRequest::TGetThreadListRequest:
      return actor->DoRequest(aRequest.get_GetThreadListRequest());
    default:
      MOZ_NOT_REACHED("Unknown type!");
      return false;
  }

  MOZ_NOT_REACHED("Should never get here!");
  return false;
}

PSmsRequestParent*
SmsParent::AllocPSmsRequest(const IPCSmsRequest& aRequest)
{
  return new SmsRequestParent();
}

bool
SmsParent::DeallocPSmsRequest(PSmsRequestParent* aActor)
{
  delete aActor;
  return true;
}


/*******************************************************************************
 * SmsRequestParent
 ******************************************************************************/
SmsRequestParent::SmsRequestParent()
{
  MOZ_COUNT_CTOR(SmsRequestParent);
}

SmsRequestParent::~SmsRequestParent()
{
  MOZ_COUNT_DTOR(SmsRequestParent);
}

void
SmsRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mSmsRequest) {
    mSmsRequest->SetActorDied();
    mSmsRequest = nullptr;
  }
}

void
SmsRequestParent::SendReply(const MessageReply& aReply) {
  nsRefPtr<SmsRequest> request;
  mSmsRequest.swap(request);
  if (!Send__delete__(this, aReply)) {
    NS_WARNING("Failed to send response to child process!");
  }
}

bool
SmsRequestParent::DoRequest(const SendMessageRequest& aRequest)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, true);

  mSmsRequest = SmsRequest::Create(this);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(mSmsRequest);
  nsresult rv = smsService->Send(aRequest.number(), aRequest.message(), forwarder);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
SmsRequestParent::DoRequest(const GetMessageRequest& aRequest)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  mSmsRequest = SmsRequest::Create(this);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(mSmsRequest);
  nsresult rv = smsDBService->GetMessageMoz(aRequest.messageId(), forwarder);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
SmsRequestParent::DoRequest(const DeleteMessageRequest& aRequest)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  mSmsRequest = SmsRequest::Create(this);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(mSmsRequest);
  nsresult rv = smsDBService->DeleteMessage(aRequest.messageId(), forwarder);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
SmsRequestParent::DoRequest(const CreateMessageListRequest& aRequest)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);

  NS_ENSURE_TRUE(smsDBService, true);
  mSmsRequest = SmsRequest::Create(this);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(mSmsRequest);
  SmsFilter *filter = new SmsFilter(aRequest.filter());
  nsresult rv = smsDBService->CreateMessageList(filter, aRequest.reverse(), forwarder);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
SmsRequestParent::DoRequest(const GetNextMessageInListRequest& aRequest)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);

  NS_ENSURE_TRUE(smsDBService, true);
  mSmsRequest = SmsRequest::Create(this);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(mSmsRequest);
  nsresult rv = smsDBService->GetNextMessageInList(aRequest.aListId(), forwarder);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
SmsRequestParent::DoRequest(const MarkMessageReadRequest& aRequest)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);

  NS_ENSURE_TRUE(smsDBService, true);
  mSmsRequest = SmsRequest::Create(this);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(mSmsRequest);
  nsresult rv = smsDBService->MarkMessageRead(aRequest.messageId(), aRequest.value(), forwarder);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
SmsRequestParent::DoRequest(const GetThreadListRequest& aRequest)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);

  NS_ENSURE_TRUE(smsDBService, true);
  mSmsRequest = SmsRequest::Create(this);
  nsCOMPtr<nsISmsRequest> forwarder = new SmsRequestForwarder(mSmsRequest);
  nsresult rv = smsDBService->GetThreadList(forwarder);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
