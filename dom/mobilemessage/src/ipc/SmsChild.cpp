/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsChild.h"
#include "SmsMessage.h"
#include "MmsMessage.h"
#include "SmsSegmentInfo.h"
#include "Constants.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentChild.h"
#include "MobileMessageThread.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::mobilemessage;

namespace {

already_AddRefed<nsISupports>
CreateMessageFromMessageData(const MobileMessageData& aData)
{
  nsCOMPtr<nsISupports> message;

  switch(aData.type()) {
    case MobileMessageData::TMmsMessageData:
      message = new MmsMessage(aData.get_MmsMessageData());
      break;
    case MobileMessageData::TSmsMessageData:
      message = new SmsMessage(aData.get_SmsMessageData());
      break;
    default:
      MOZ_CRASH("Unexpected type of MobileMessageData");
  }

  return message.forget();
}

void
NotifyObserversWithMobileMessage(const char* aEventName,
                                 const MobileMessageData& aData)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return;
  }

  nsCOMPtr<nsISupports> msg = CreateMessageFromMessageData(aData);
  obs->NotifyObservers(msg, aEventName, nullptr);
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
SmsChild::RecvNotifyReceivedMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsReceivedObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifyRetrievingMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsRetrievingObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifySendingMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsSendingObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifySentMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsSentObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifyFailedMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsFailedObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifyDeliverySuccessMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsDeliverySuccessObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifyDeliveryErrorMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsDeliveryErrorObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifyReceivedSilentMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSilentSmsReceivedObserverTopic, aData);
  return true;
}

PSmsRequestChild*
SmsChild::AllocPSmsRequestChild(const IPCSmsRequest& aRequest)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
SmsChild::DeallocPSmsRequestChild(PSmsRequestChild* aActor)
{
  delete aActor;
  return true;
}

PMobileMessageCursorChild*
SmsChild::AllocPMobileMessageCursorChild(const IPCMobileMessageCursor& aCursor)
{
  MOZ_CRASH("Caller is supposed to manually construct a cursor!");
}

bool
SmsChild::DeallocPMobileMessageCursorChild(PMobileMessageCursorChild* aActor)
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
    case MessageReply::TReplyMessageSend: {
        const MobileMessageData& data =
          aReply.get_ReplyMessageSend().messageData();
        nsCOMPtr<nsISupports> msg = CreateMessageFromMessageData(data);
        mReplyRequest->NotifyMessageSent(msg);
      }
      break;
    case MessageReply::TReplyMessageSendFail:
      mReplyRequest->NotifySendMessageFailed(aReply.get_ReplyMessageSendFail().error());
      break;
    case MessageReply::TReplyGetMessage: {
        const MobileMessageData& data =
          aReply.get_ReplyGetMessage().messageData();
        nsCOMPtr<nsISupports> msg = CreateMessageFromMessageData(data);
        mReplyRequest->NotifyMessageGot(msg);
      }
      break;
    case MessageReply::TReplyGetMessageFail:
      mReplyRequest->NotifyGetMessageFailed(aReply.get_ReplyGetMessageFail().error());
      break;
    case MessageReply::TReplyMessageDelete: {
        const InfallibleTArray<bool>& deletedResult = aReply.get_ReplyMessageDelete().deleted();
        mReplyRequest->NotifyMessageDeleted(const_cast<bool *>(deletedResult.Elements()),
                                            deletedResult.Length());
      }
      break;
    case MessageReply::TReplyMessageDeleteFail:
      mReplyRequest->NotifyDeleteMessageFailed(aReply.get_ReplyMessageDeleteFail().error());
      break;
    case MessageReply::TReplyMarkeMessageRead:
      mReplyRequest->NotifyMessageMarkedRead(aReply.get_ReplyMarkeMessageRead().read());
      break;
    case MessageReply::TReplyMarkeMessageReadFail:
      mReplyRequest->NotifyMarkMessageReadFailed(aReply.get_ReplyMarkeMessageReadFail().error());
      break;
    case MessageReply::TReplyGetSegmentInfoForText: {
        const SmsSegmentInfoData& data =
          aReply.get_ReplyGetSegmentInfoForText().infoData();
        nsCOMPtr<nsIDOMMozSmsSegmentInfo> info = new SmsSegmentInfo(data);
        mReplyRequest->NotifySegmentInfoForTextGot(info);
      }
      break;
    case MessageReply::TReplyGetSegmentInfoForTextFail:
      mReplyRequest->NotifyGetSegmentInfoForTextFailed(
        aReply.get_ReplyGetSegmentInfoForTextFail().error());
      break;
    default:
      MOZ_CRASH("Received invalid response parameters!");
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
    case MobileMessageCursorData::TMmsMessageData:
      result = new MmsMessage(aData.get_MmsMessageData());
      break;
    case MobileMessageCursorData::TSmsMessageData:
      result = new SmsMessage(aData.get_SmsMessageData());
      break;
    case MobileMessageCursorData::TThreadData:
      result = new MobileMessageThread(aData.get_ThreadData());
      break;
    default:
      MOZ_CRASH("Received invalid response parameters!");
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
