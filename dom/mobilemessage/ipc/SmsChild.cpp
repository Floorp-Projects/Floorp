/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsChild.h"

#include "SmsMessageInternal.h"
#include "MmsMessageInternal.h"
#include "DeletedMessageInfo.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/mobilemessage/Constants.h" // For MessageType
#include "MobileMessageThreadInternal.h"
#include "MainThreadUtils.h"

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
      message = new MmsMessageInternal(aData.get_MmsMessageData());
      break;
    case MobileMessageData::TSmsMessageData:
      message = new SmsMessageInternal(aData.get_SmsMessageData());
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

} // namespace

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

bool
SmsChild::RecvNotifyReadSuccessMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsReadSuccessObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifyReadErrorMessage(const MobileMessageData& aData)
{
  NotifyObserversWithMobileMessage(kSmsReadErrorObserverTopic, aData);
  return true;
}

bool
SmsChild::RecvNotifyDeletedMessageInfo(const DeletedMessageInfoData& aDeletedInfo)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    NS_ERROR("Failed to get nsIObserverService!");
    return false;
  }

  nsCOMPtr<nsISupports> info = new DeletedMessageInfo(aDeletedInfo);
  obs->NotifyObservers(info, kSmsDeletedObserverTopic, nullptr);

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
  switch(aReply.type()) {
    case MessageReply::TReplyMessageSend: {
        const MobileMessageData& data =
          aReply.get_ReplyMessageSend().messageData();
        nsCOMPtr<nsISupports> msg = CreateMessageFromMessageData(data);
        mReplyRequest->NotifyMessageSent(msg);
      }
      break;
    case MessageReply::TReplyMessageSendFail: {
        const ReplyMessageSendFail &replyFail = aReply.get_ReplyMessageSendFail();
        nsCOMPtr<nsISupports> msg;

        if (replyFail.messageData().type() ==
            OptionalMobileMessageData::TMobileMessageData) {
          msg = CreateMessageFromMessageData(
            replyFail.messageData().get_MobileMessageData());
        }

        mReplyRequest->NotifySendMessageFailed(replyFail.error(), msg);
      }
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
        const ReplyGetSegmentInfoForText& reply =
          aReply.get_ReplyGetSegmentInfoForText();
        mReplyRequest->NotifySegmentInfoForTextGot(reply.segments(),
                                                   reply.charsPerSegment(),
                                                   reply.charsAvailableInLastSegment());
      }
      break;
    case MessageReply::TReplyGetSegmentInfoForTextFail:
      mReplyRequest->NotifyGetSegmentInfoForTextFailed(
        aReply.get_ReplyGetSegmentInfoForTextFail().error());
      break;
    case MessageReply::TReplyGetSmscAddress:
      mReplyRequest->NotifyGetSmscAddress(aReply.get_ReplyGetSmscAddress().smscAddress(),
                                          aReply.get_ReplyGetSmscAddress().typeOfNumber(),
                                          aReply.get_ReplyGetSmscAddress().numberPlanIdentification());
      break;
    case MessageReply::TReplyGetSmscAddressFail:
      mReplyRequest->NotifyGetSmscAddressFailed(aReply.get_ReplyGetSmscAddressFail().error());
      break;
    case MessageReply::TReplySetSmscAddress:
      mReplyRequest->NotifySetSmscAddress();
      break;
    case MessageReply::TReplySetSmscAddressFail:
      mReplyRequest->NotifySetSmscAddressFailed(aReply.get_ReplySetSmscAddressFail().error());
      break;
    default:
      MOZ_CRASH("Received invalid response parameters!");
  }

  return true;
}

/*******************************************************************************
 * MobileMessageCursorChild
 ******************************************************************************/

NS_IMPL_ISUPPORTS(MobileMessageCursorChild, nsICursorContinueCallback)

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

  switch(aData.type()) {
    case MobileMessageCursorData::TMobileMessageArrayData:
      DoNotifyResult(aData.get_MobileMessageArrayData().messages());
      break;
    case MobileMessageCursorData::TThreadArrayData:
      DoNotifyResult(aData.get_ThreadArrayData().threads());
      break;
    default:
      MOZ_CRASH("Received invalid response parameters!");
  }

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

void
MobileMessageCursorChild::DoNotifyResult(const nsTArray<MobileMessageData>& aDataArray)
{
  const uint32_t length = aDataArray.Length();
  MOZ_ASSERT(length);

  AutoTArray<nsISupports*, 1> autoArray;
  NS_ENSURE_TRUE_VOID(autoArray.SetCapacity(length, fallible));

  AutoTArray<nsCOMPtr<nsISupports>, 1> messages;
  NS_ENSURE_TRUE_VOID(messages.SetCapacity(length, fallible));

  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsISupports> message = CreateMessageFromMessageData(aDataArray[i]);
    NS_ENSURE_TRUE_VOID(messages.AppendElement(message, fallible));
    NS_ENSURE_TRUE_VOID(autoArray.AppendElement(message.get(), fallible));
  }

  mCursorCallback->NotifyCursorResult(autoArray.Elements(), length);
}

void
MobileMessageCursorChild::DoNotifyResult(const nsTArray<ThreadData>& aDataArray)
{
  const uint32_t length = aDataArray.Length();
  MOZ_ASSERT(length);

  AutoTArray<nsISupports*, 1> autoArray;
  NS_ENSURE_TRUE_VOID(autoArray.SetCapacity(length, fallible));

  AutoTArray<nsCOMPtr<nsISupports>, 1> threads;
  NS_ENSURE_TRUE_VOID(threads.SetCapacity(length, fallible));

  for (uint32_t i = 0; i < length; i++) {
    nsCOMPtr<nsISupports> thread =
      new MobileMessageThreadInternal(aDataArray[i]);
    NS_ENSURE_TRUE_VOID(threads.AppendElement(thread, fallible));
    NS_ENSURE_TRUE_VOID(autoArray.AppendElement(thread.get(), fallible));
  }

  mCursorCallback->NotifyCursorResult(autoArray.Elements(), length);
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
