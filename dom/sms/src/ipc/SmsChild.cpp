/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsChild.h"
#include "SmsMessage.h"
#include "Constants.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentChild.h"
#include "SmsRequest.h"

using namespace mozilla;
using namespace mozilla::dom::sms;

namespace {

void
NotifyObserversWithSmsMessage(const char* aEventName,
                              const SmsMessageData& aMessageData)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return;
  }

  nsCOMPtr<SmsMessage> message = new SmsMessage(aMessageData);
  obs->NotifyObservers(message, aEventName, nullptr);
}

} // anonymous namespace

namespace mozilla {
namespace dom {
namespace sms {

SmsChild::SmsChild()
{
  MOZ_COUNT_CTOR(SmsChild);
}

SmsChild::~SmsChild()
{
  MOZ_COUNT_DTOR(SmsChild);
}

void
SmsChild::ActorDestroy(ActorDestroyReason aWhy)
{
}

bool
SmsChild::RecvNotifyReceivedMessage(const SmsMessageData& aMessageData)
{
  NotifyObserversWithSmsMessage(kSmsReceivedObserverTopic, aMessageData);
  return true;
}

bool
SmsChild::RecvNotifySendingMessage(const SmsMessageData& aMessageData)
{
  NotifyObserversWithSmsMessage(kSmsSendingObserverTopic, aMessageData);
  return true;
}

bool
SmsChild::RecvNotifySentMessage(const SmsMessageData& aMessageData)
{
  NotifyObserversWithSmsMessage(kSmsSentObserverTopic, aMessageData);
  return true;
}

bool
SmsChild::RecvNotifyFailedMessage(const SmsMessageData& aMessageData)
{
  NotifyObserversWithSmsMessage(kSmsFailedObserverTopic, aMessageData);
  return true;
}

bool
SmsChild::RecvNotifyDeliverySuccessMessage(const SmsMessageData& aMessageData)
{
  NotifyObserversWithSmsMessage(kSmsDeliverySuccessObserverTopic, aMessageData);
  return true;
}

bool
SmsChild::RecvNotifyDeliveryErrorMessage(const SmsMessageData& aMessageData)
{
  NotifyObserversWithSmsMessage(kSmsDeliveryErrorObserverTopic, aMessageData);
  return true;
}

PSmsRequestChild*
SmsChild::AllocPSmsRequest(const IPCSmsRequest& aRequest)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a request!");
  return nullptr;
}

bool
SmsChild::DeallocPSmsRequest(PSmsRequestChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * SmsRequestChild
 ******************************************************************************/

SmsRequestChild::SmsRequestChild(nsISmsRequest* aReplyRequest)
: mReplyRequest(aReplyRequest)
{
  MOZ_COUNT_CTOR(SmsRequestChild);
  MOZ_ASSERT(aReplyRequest);
}

SmsRequestChild::~SmsRequestChild()
{
  MOZ_COUNT_DTOR(SmsRequestChild);
}

void
SmsRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  // Nothing needed here.
}

bool
SmsRequestChild::Recv__delete__(const MessageReply& aReply)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mReplyRequest);
  nsCOMPtr<SmsMessage> message;
  switch(aReply.type()) {
    case MessageReply::TReplyMessageSend:
      message = new SmsMessage(aReply.get_ReplyMessageSend().messageData());
      mReplyRequest->NotifyMessageSent(message);
      break;
    case MessageReply::TReplyMessageSendFail:
      mReplyRequest->NotifySendMessageFailed(aReply.get_ReplyMessageSendFail().error());
      break;
    case MessageReply::TReplyGetMessage:
      message = new SmsMessage(aReply.get_ReplyGetMessage().messageData());
      mReplyRequest->NotifyMessageGot(message);
      break;
    case MessageReply::TReplyGetMessageFail:
      mReplyRequest->NotifyGetMessageFailed(aReply.get_ReplyGetMessageFail().error());
      break;
    case MessageReply::TReplyMessageDelete:
      mReplyRequest->NotifyMessageDeleted(aReply.get_ReplyMessageDelete().deleted());
      break;
    case MessageReply::TReplyMessageDeleteFail:
      mReplyRequest->NotifyMessageDeleted(aReply.get_ReplyMessageDeleteFail().error());
      break;
    case MessageReply::TReplyNoMessageInList:
      mReplyRequest->NotifyNoMessageInList();
      break;
    case MessageReply::TReplyCreateMessageList:
      message = new SmsMessage(aReply.get_ReplyCreateMessageList().messageData());
      mReplyRequest->NotifyMessageListCreated(aReply.get_ReplyCreateMessageList().listId(), 
                                              message);
      break;
    case MessageReply::TReplyCreateMessageListFail:
      mReplyRequest->NotifyReadMessageListFailed(aReply.get_ReplyCreateMessageListFail().error());
      break;
    case MessageReply::TReplyGetNextMessage:
      message = new SmsMessage(aReply.get_ReplyGetNextMessage().messageData());
      mReplyRequest->NotifyNextMessageInListGot(message);
      break;
    case MessageReply::TReplyMarkeMessageRead:
      mReplyRequest->NotifyMessageMarkedRead(aReply.get_ReplyMarkeMessageRead().read());
      break;
    case MessageReply::TReplyMarkeMessageReadFail:
      mReplyRequest->NotifyMarkMessageReadFailed(aReply.get_ReplyMarkeMessageReadFail().error());
      break;
    case MessageReply::TReplyThreadList: {
      SmsRequestForwarder* forwarder = static_cast<SmsRequestForwarder*>(mReplyRequest.get());
      SmsRequest* request = static_cast<SmsRequest*>(forwarder->GetRealRequest());
      request->NotifyThreadList(aReply.get_ReplyThreadList().items());
    } break;
    case MessageReply::TReplyThreadListFail:
      mReplyRequest->NotifyThreadListFailed(aReply.get_ReplyThreadListFail().error());
      break;
    default:
      MOZ_NOT_REACHED("Received invalid response parameters!");
      return false;
  }

  return true;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
