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
    message.sender() = aSender ? nsString(aSender) : EmptyString();
    message.receiver() = EmptyString();
    message.body() = aBody ? nsString(aBody) : EmptyString();
    message.messageClass() = static_cast<MessageClass>(aMessageClass);
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aSentTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = false;

    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=] () {
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        if (!obs) {
            return;
        }

        nsCOMPtr<nsISmsMessage> domMessage = new SmsMessageInternal(message);
        obs->NotifyObservers(domMessage, kSmsReceivedObserverTopic, nullptr);
    });
    NS_DispatchToMainThread(runnable);
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
    message.receiver() = aReceiver ? nsString(aReceiver) : EmptyString();
    message.body() = aBody ? nsString(aBody) : EmptyString();
    message.messageClass() = eMessageClass_Normal;
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = true;

    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
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
    });
    NS_DispatchToMainThread(runnable);
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
    message.receiver() = aReceiver ? nsString(aReceiver) : EmptyString();
    message.body() = aBody ? nsString(aBody) : EmptyString();
    message.messageClass() = eMessageClass_Normal;
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = true;

    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
        if (!obs) {
            return;
        }

        nsCOMPtr<nsISmsMessage> domMessage = new SmsMessageInternal(message);
        const char* topic = (message.deliveryStatus() == eDeliveryStatus_Success)
                            ? kSmsDeliverySuccessObserverTopic
                            : kSmsDeliveryErrorObserverTopic;
        obs->NotifyObservers(domMessage, topic, nullptr);
    });
    NS_DispatchToMainThread(runnable);
}

/*static*/
void
SmsManager::NotifySmsSendFailed(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCallback> request =
            AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
        if(!request) {
            return;
        }

        request->NotifySendMessageFailed(aError, nullptr);
    });
    NS_DispatchToMainThread(runnable);
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
    nsString receiver(aReceiver);
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
    message.sender() = aSender ? nsString(aSender) : EmptyString();
    message.receiver() = receiver;
    message.body() = aBody ? nsString(aBody) : EmptyString();
    message.messageClass() = eMessageClass_Normal;
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = aRead;

    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCallback> request =
            AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
        if (!request) {
            return;
        }

        nsCOMPtr<nsISmsMessage> domMessage = new SmsMessageInternal(message);
        request->NotifyMessageGot(domMessage);
    });
    NS_DispatchToMainThread(runnable);
}

/*static*/
void
SmsManager::NotifyGetSmsFailed(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCallback> request =
            AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
        if (!request) {
            return;
        }

        request->NotifyGetMessageFailed(aError);
    });
    NS_DispatchToMainThread(runnable);
}

/*static*/
void
SmsManager::NotifySmsDeleted(bool aDeleted, int32_t aRequestId)
{
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCallback> request =
            AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
        if (!request) {
            return;
        }

        // For android, we support only single SMS deletion.
        bool deleted = aDeleted;
        request->NotifyMessageDeleted(&deleted, 1);
    });
    NS_DispatchToMainThread(runnable);
}

/*static*/
void
SmsManager::NotifySmsDeleteFailed(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCallback> request =
            AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
        if (!request) {
            return;
        }

        request->NotifyDeleteMessageFailed(aError);
    });
    NS_DispatchToMainThread(runnable);
}

/*static*/
void
SmsManager::NotifyCursorError(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCursorCallback> request =
            AndroidBridge::Bridge()->DequeueSmsCursorRequest(aRequestId);
        if (!request) {
            return;
        }

        request->NotifyCursorError(aError);
    });
    NS_DispatchToMainThread(runnable);
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
                                    nsString(aLastMessageSubject) :
                                    EmptyString();
    thread.body() = aBody ? nsString(aBody) : EmptyString();
    thread.unreadCount() = aUnreadCount;
    thread.timestamp() = aTimestamp;
    thread.lastMessageType() = eMessageType_SMS;

    JNIEnv* const env = jni::GetEnvForThread();

    jobjectArray participants = aParticipants.Get();
    jsize length = env->GetArrayLength(participants);
    for (jsize i = 0; i < length; ++i) {
        jstring participant =
            static_cast<jstring>(env->GetObjectArrayElement(participants, i));
        if (participant) {
            thread.participants().AppendElement(nsJNIString(participant, env));
        }
    }

    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
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
    });
    NS_DispatchToMainThread(runnable);
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
    nsString receiver = nsString(aReceiver);
    DeliveryState state = receiver.IsEmpty() ? eDeliveryState_Received
                                             : eDeliveryState_Sent;

    // TODO Need to add the message `messageClass` parameter value. Bug 804476
    SmsMessageData message;
    message.id() = aMessageId;
    message.threadId() = aThreadId;
    message.iccId() = EmptyString();
    message.delivery() = state;
    message.deliveryStatus() = static_cast<DeliveryStatus>(aDeliveryStatus);
    message.sender() = aSender ? nsString(aSender) : EmptyString();
    message.receiver() = receiver;
    message.body() = aBody ? nsString(aBody) : EmptyString();
    message.messageClass() = eMessageClass_Normal;
    message.timestamp() = aTimestamp;
    message.sentTimestamp() = aTimestamp;
    message.deliveryTimestamp() = aTimestamp;
    message.read() = aRead;

    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
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
    });
    NS_DispatchToMainThread(runnable);
}

/*static*/
void
SmsManager::NotifyCursorDone(int32_t aRequestId)
{
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCursorCallback> request =
            AndroidBridge::Bridge()->DequeueSmsCursorRequest(aRequestId);
        if (!request) {
            return;
        }

        request->NotifyCursorDone();
    });
    NS_DispatchToMainThread(runnable);
}

/*static*/
void
SmsManager::NotifySmsMarkedAsRead(bool aMarkedAsRead, int32_t aRequestId)
{
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCallback> request =
            AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
        if (!request) {
            return;
        }

        request->NotifyMessageMarkedRead(aMarkedAsRead);
    });
    NS_DispatchToMainThread(runnable);
}

/*static*/
void
SmsManager::NotifySmsMarkAsReadFailed(int32_t aError, int32_t aRequestId)
{
    nsCOMPtr<nsIRunnable> runnable = NS_NewRunnableFunction([=]() {
        nsCOMPtr<nsIMobileMessageCallback> request =
            AndroidBridge::Bridge()->DequeueSmsRequest(aRequestId);
        if (!request) {
            return;
        }

        request->NotifyMarkMessageReadFailed(aError);
    });
    NS_DispatchToMainThread(runnable);
}

} // namespace
