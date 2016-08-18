/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsManager.h"

#include "mozilla/dom/mobilemessage/Constants.h"
#include "mozilla/dom/mobilemessage/PSms.h"
#include "mozilla/dom/mobilemessage/SmsParent.h"
#include "mozilla/dom/mobilemessage/SmsTypes.h"
#include "mozilla/dom/mobilemessage/Types.h"
#include "MobileMessageThreadInternal.h"
#include "SmsMessageInternal.h"
#include "mozilla/Services.h"
#include "nsIMobileMessageDatabaseService.h"
#include "nsIObserverService.h"
#include "nsThreadUtils.h"
#include "AndroidJavaWrappers.h"

using namespace mozilla::dom;
using namespace mozilla::dom::mobilemessage;

namespace mozilla {

/*static*/
void
SmsManager::NotifySmsReceived(int32_t aId,
                              jni::String::Param aSender,
                              jni::String::Param aBody,
                              int32_t aMessageClass,
                              int64_t aSentTimestamp,
                              int64_t aTimestamp)
{
    // TODO Need to correct the message `threadId` parameter value. Bug 859098
    SmsMessageData message;
    message.id() = aId;
    message.threadId() = 0;
    message.iccId() = EmptyString();
    message.delivery() = eDeliveryState_Received;
    message.deliveryStatus() = eDeliveryStatus_Success;
    message.sender() = aSender ? aSender->ToString() : EmptyString();
    message.receiver() = EmptyString();
    message.body() = aBody ? aBody->ToString() : EmptyString();
    message.messageClass() = static_cast<MessageClass>(aMessageClass);
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aSentTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = false;

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!obs) {
        return;
    }

    nsCOMPtr<nsISmsMessage> domMessage = new SmsMessageInternal(message);
    obs->NotifyObservers(domMessage, kSmsReceivedObserverTopic, nullptr);
}

/*static*/
void
SmsManager::NotifySmsSent(int32_t aId,
                          jni::String::Param aReceiver,
                          jni::String::Param aBody,
                          int64_t aTimestamp,
                          int32_t aRequestId)
{
    // TODO Need to add the message `messageClass` parameter value. Bug 804476
    // TODO Need to correct the message `threadId` parameter value. Bug 859098
    SmsMessageData message;
    message.id() = aId;
    message.threadId() = 0;
    message.iccId() = EmptyString();
    message.delivery() = eDeliveryState_Sent;
    message.deliveryStatus() = eDeliveryStatus_Pending;
    message.sender() = EmptyString();
    message.receiver() = aReceiver ? aReceiver->ToString() : EmptyString();
    message.body() = aBody ? aBody->ToString() : EmptyString();
    message.messageClass() = eMessageClass_Normal;
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = true;

    /*
     * First, we are going to notify all SmsManager that a message has
     * been sent. Then, we will notify the SmsRequest object about it.
     */
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!obs) {
        return;
    }

    nsCOMPtr<nsISmsMessage> domMessage = new SmsMessageInternal(message);
    obs->NotifyObservers(domMessage, kSmsSentObserverTopic, nullptr);

    nsCOMPtr<nsIMobileMessageCallback> request =
        AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
    if (!request) {
        return;
    }

    request->NotifyMessageSent(domMessage);
}

/*static*/
void
SmsManager::NotifySmsDelivery(int32_t aId,
                              int32_t aDeliveryStatus,
                              jni::String::Param aReceiver,
                              jni::String::Param aBody,
                              int64_t aTimestamp)
{
    // TODO Need to add the message `messageClass` parameter value. Bug 804476
    // TODO Need to correct the message `threadId` parameter value. Bug 859098
    SmsMessageData message;
    message.id() = aId;
    message.threadId() = 0;
    message.iccId() = EmptyString();
    message.delivery() = eDeliveryState_Sent;
    message.deliveryStatus() = static_cast<DeliveryStatus>(aDeliveryStatus);
    message.sender() = EmptyString();
    message.receiver() = aReceiver ? aReceiver->ToString() : EmptyString();
    message.body() = aBody ? aBody->ToString() : EmptyString();
    message.messageClass() = eMessageClass_Normal;
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = true;

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (!obs) {
        return;
    }

    nsCOMPtr<nsISmsMessage> domMessage = new SmsMessageInternal(message);
    const char* topic = (message.deliveryStatus() == eDeliveryStatus_Success)
                        ? kSmsDeliverySuccessObserverTopic
                        : kSmsDeliveryErrorObserverTopic;
    obs->NotifyObservers(domMessage, topic, nullptr);
}

/*static*/
void
SmsManager::NotifySmsSendFailed(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIMobileMessageCallback> request =
        AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
    if(!request) {
        return;
    }

    request->NotifySendMessageFailed(aError, nullptr);
}

/*static*/
void
SmsManager::NotifyGetSms(int32_t aId,
                         int32_t aDeliveryStatus,
                         jni::String::Param aReceiver,
                         jni::String::Param aSender,
                         jni::String::Param aBody,
                         int64_t aTimestamp,
                         bool aRead,
                         int32_t aRequestId)
{
    nsString receiver(aReceiver->ToString());
    DeliveryState state = receiver.IsEmpty() ? eDeliveryState_Received
                                             : eDeliveryState_Sent;

    // TODO Need to add the message `messageClass` parameter value. Bug 804476
    // TODO Need to correct the message `threadId` parameter value. Bug 859098
    SmsMessageData message;
    message.id() = aId;
    message.threadId() = 0;
    message.iccId() = EmptyString();
    message.delivery() = state;
    message.deliveryStatus() = static_cast<DeliveryStatus>(aDeliveryStatus);
    message.sender() = aSender ? aSender->ToString() : EmptyString();
    message.receiver() = receiver;
    message.body() = aBody ? aBody->ToString() : EmptyString();
    message.messageClass() = eMessageClass_Normal;
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = aRead;

    nsCOMPtr<nsIMobileMessageCallback> request =
        AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
    if (!request) {
        return;
    }

    nsCOMPtr<nsISmsMessage> domMessage = new SmsMessageInternal(message);
    request->NotifyMessageGot(domMessage);
}

/*static*/
void
SmsManager::NotifyGetSmsFailed(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIMobileMessageCallback> request =
        AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
    if (!request) {
        return;
    }

    request->NotifyGetMessageFailed(aError);
}

/*static*/
void
SmsManager::NotifySmsDeleted(bool aDeleted, int32_t aRequestId)
{
    nsCOMPtr<nsIMobileMessageCallback> request =
        AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
    if (!request) {
        return;
    }

    // For android, we support only single SMS deletion.
    bool deleted = aDeleted;
    request->NotifyMessageDeleted(&deleted, 1);
}

/*static*/
void
SmsManager::NotifySmsDeleteFailed(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIMobileMessageCallback> request =
        AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
    if (!request) {
        return;
    }

    request->NotifyDeleteMessageFailed(aError);
}

/*static*/
void
SmsManager::NotifyCursorError(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIMobileMessageCursorCallback> request =
        AndroidBridge::Bridge()->DequeueSmsCursorRequest(aRequestId);
    if (!request) {
        return;
    }

    request->NotifyCursorError(aError);
}

/*static*/
void
SmsManager::NotifyThreadCursorResult(int64_t aId,
                                     jni::String::Param aLastMessageSubject,
                                     jni::String::Param aBody,
                                     int64_t aUnreadCount,
                                     jni::ObjectArray::Param aParticipants,
                                     int64_t aTimestamp,
                                     jni::String::Param aLastMessageType,
                                     int32_t aRequestId)
{
    ThreadData thread;
    thread.id() = aId;
    thread.lastMessageSubject() = aLastMessageSubject ?
                                    aLastMessageSubject->ToString() :
                                    EmptyString();
    thread.body() = aBody ? aBody->ToString() : EmptyString();
    thread.unreadCount() = aUnreadCount;
    thread.timestamp() = aTimestamp;
    thread.lastMessageType() = eMessageType_SMS;

    JNIEnv* const env = jni::GetGeckoThreadEnv();

    jobjectArray participants = aParticipants.Get();
    jsize length = env->GetArrayLength(participants);
    for (jsize i = 0; i < length; ++i) {
        jstring participant =
            static_cast<jstring>(env->GetObjectArrayElement(participants, i));
        if (participant) {
            thread.participants().AppendElement(nsJNIString(participant, env));
            env->DeleteLocalRef(participant);
        }
    }

    nsCOMPtr<nsIMobileMessageCursorCallback> request =
        AndroidBridge::Bridge()->GetSmsCursorRequest(aRequestId);
    if (!request) {
        return;
    }

    nsCOMArray<nsIMobileMessageThread> arr;
    arr.AppendElement(new MobileMessageThreadInternal(thread));

    nsIMobileMessageThread** elements;
    int32_t size;
    size = arr.Forget(&elements);

    request->NotifyCursorResult(reinterpret_cast<nsISupports**>(elements),
                                size);
}

/*static*/
void
SmsManager::NotifyMessageCursorResult(int32_t aMessageId,
                                      int32_t aDeliveryStatus,
                                      jni::String::Param aReceiver,
                                      jni::String::Param aSender,
                                      jni::String::Param aBody,
                                      int64_t aTimestamp,
                                      int64_t aThreadId,
                                      bool aRead,
                                      int32_t aRequestId)
{
    nsString receiver = aReceiver->ToString();
    DeliveryState state = receiver.IsEmpty() ? eDeliveryState_Received
                                             : eDeliveryState_Sent;

    // TODO Need to add the message `messageClass` parameter value. Bug 804476
    SmsMessageData message;
    message.id() = aMessageId;
    message.threadId() = aThreadId;
    message.iccId() = EmptyString();
    message.delivery() = state;
    message.deliveryStatus() = static_cast<DeliveryStatus>(aDeliveryStatus);
    message.sender() = aSender ? aSender->ToString() : EmptyString();
    message.receiver() = receiver;
    message.body() = aBody ? aBody->ToString() : EmptyString();
    message.messageClass() = eMessageClass_Normal;
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = aRead;

    nsCOMPtr<nsIMobileMessageCursorCallback> request =
        AndroidBridge::Bridge()->GetSmsCursorRequest(aRequestId);
    if (!request) {
        return;
    }

    nsCOMArray<nsISmsMessage> arr;
    arr.AppendElement(new SmsMessageInternal(message));

    nsISmsMessage** elements;
    int32_t size;
    size = arr.Forget(&elements);

    request->NotifyCursorResult(reinterpret_cast<nsISupports**>(elements),
                                size);
}

/*static*/
void
SmsManager::NotifyCursorDone(int32_t aRequestId)
{
    nsCOMPtr<nsIMobileMessageCursorCallback> request =
        AndroidBridge::Bridge()->DequeueSmsCursorRequest(aRequestId);
    if (!request) {
        return;
    }

    request->NotifyCursorDone();
}

/*static*/
void
SmsManager::NotifySmsMarkedAsRead(bool aMarkedAsRead, int32_t aRequestId)
{
    nsCOMPtr<nsIMobileMessageCallback> request =
        AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
    if (!request) {
        return;
    }

    request->NotifyMessageMarkedRead(aMarkedAsRead);
}

/*static*/
void
SmsManager::NotifySmsMarkAsReadFailed(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIMobileMessageCallback> request =
        AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
    if (!request) {
        return;
    }

    request->NotifyMarkMessageReadFailed(aError);
}

} // namespace
