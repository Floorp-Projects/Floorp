/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsChild.h"
#include "SmsMessage.h"
#include "Constants.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentChild.h"
#include "MobileMessageThread.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobilemessage;

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
namespace mobilemessage {

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

PMobileMessageCursorChild*
SmsChild::AllocPMobileMessageCursor(const IPCMobileMessageCursor& aCursor)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a cursor!");
  return nullptr;
}

bool
SmsChild::DeallocPMobileMessageCursor(PMobileMessageCursorChild* aActor)
{
  // MobileMessageCursorChild is refcounted, must not be freed manually.
  // Originally AddRefed in SendCursorRequest() in SmsIPCService.cpp.
  static_cast<MobileMessageCursorChild*>(aActor)->Release();
  return true;
}

/*******************************************************************************
 * SmsRequestChild
 ******************************************************************************/

SmsRequestChild::SmsRequestChild(nsIMobileMessageCallback* aReplyRequest)
  : mReplyRequest(aReplyRequest)
{
  MOZ_COUNT_CTOR(SmsRequestChild);
  MOZ_ASSERT(aReplyRequest);
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
    case MessageReply::TReplyMarkeMessageRead:
      mReplyRequest->NotifyMessageMarkedRead(aReply.get_ReplyMarkeMessageRead().read());
      break;
    case MessageReply::TReplyMarkeMessageReadFail:
      mReplyRequest->NotifyMarkMessageReadFailed(aReply.get_ReplyMarkeMessageReadFail().error());
      break;
    default:
      MOZ_NOT_REACHED("Received invalid response parameters!");
      return false;
  }

  return true;
}

/*******************************************************************************
 * MobileMessageCursorChild
 ******************************************************************************/

NS_IMPL_ISUPPORTS1(MobileMessageCursorChild, nsICursorContinueCallback)

MobileMessageCursorChild::MobileMessageCursorChild(nsIMobileMessageCursorCallback* aCallback)
  : mCursorCallback(aCallback)
{
  MOZ_COUNT_CTOR(MobileMessageCursorChild);
  MOZ_ASSERT(aCallback);
}

void
MobileMessageCursorChild::ActorDestroy(ActorDestroyReason aWhy)
{
  // Nothing needed here.
}

bool
MobileMessageCursorChild::RecvNotifyResult(const MobileMessageCursorData& aData)
{
  MOZ_ASSERT(mCursorCallback);

  nsCOMPtr<nsISupports> result;
  switch(aData.type()) {
    case MobileMessageCursorData::TSmsMessageData:
      result = new SmsMessage(aData.get_SmsMessageData());
      break;
    case MobileMessageCursorData::TThreadData:
      result = new MobileMessageThread(aData.get_ThreadData());
      break;
    default:
      MOZ_NOT_REACHED("Received invalid response parameters!");
      return false;
  }

  mCursorCallback->NotifyCursorResult(result);
  return true;
}

bool
MobileMessageCursorChild::Recv__delete__(const int32_t& aError)
{
  MOZ_ASSERT(mCursorCallback);

  if (aError != nsIMobileMessageCallback::SUCCESS_NO_ERROR) {
    mCursorCallback->NotifyCursorError(aError);
  } else {
    mCursorCallback->NotifyCursorDone();
  }
  mCursorCallback = nullptr;

  return true;
}

// nsICursorContinueCallback

NS_IMETHODIMP
MobileMessageCursorChild::HandleContinue()
{
  MOZ_ASSERT(mCursorCallback);

  SendContinue();
  return NS_OK;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
