/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SmsManager_h__
#define SmsManager_h__

#include "GeneratedJNINatives.h"

namespace mozilla {

class SmsManager : public widget::GeckoSmsManager::Natives<SmsManager>
{
private:
    SmsManager();

public:
    static void NotifySmsReceived(jni::String::Param aSender,
                                  jni::String::Param aBody,
                                  int32_t aMessageClass,
                                  int64_t aTimestamp);
    static void NotifySmsSent(int32_t aId,
                              jni::String::Param aReceiver,
                              jni::String::Param aBody,
                              int64_t aTimestamp,
                              int32_t aRequestId);
    static void NotifySmsDelivery(int32_t aId,
                                  int32_t aDeliveryStatus,
                                  jni::String::Param aReceiver,
                                  jni::String::Param aBody,
                                  int64_t aTimestamp);
    static void NotifySmsSendFailed(int32_t aError,
                                    int32_t aRequestId);
    static void NotifyGetSms(int32_t aId,
                             int32_t aDeliveryStatus,
                             jni::String::Param aReceiver,
                             jni::String::Param aSender,
                             jni::String::Param aBody,
                             int64_t aTimestamp,
                             bool aRead,
                             int32_t aRequestId);
    static void NotifyGetSmsFailed(int32_t aError,
                                   int32_t aRequestId);
    static void NotifySmsDeleted(bool aDeleted,
                                 int32_t aRequestId);
    static void NotifySmsDeleteFailed(int32_t aError,
                                      int32_t aRequestId);
    static void NotifyCursorError(int32_t aError,
                                  int32_t aRequestId);
    static void NotifyThreadCursorResult(int64_t aId,
                                         jni::String::Param aLastMessageSubject,
                                         jni::String::Param aBody,
                                         int64_t aUnreadCount,
                                         jni::ObjectArray::Param aParticipants,
                                         int64_t aTimestamp,
                                         jni::String::Param aLastMessageType,
                                         int32_t aRequestId);
    static void NotifyMessageCursorResult(int32_t aMessageId,
                                          int32_t aDeliveryStatus,
                                          jni::String::Param aReceiver,
                                          jni::String::Param aSender,
                                          jni::String::Param aBody,
                                          int64_t aTimestamp,
                                          int64_t aThreadId,
                                          bool aRead,
                                          int32_t aRequestId);
    static void NotifyCursorDone(int32_t aRequestId);
    static void NotifySmsMarkedAsRead(bool aMarkedAsRead, int32_t aRequestId);
    static void NotifySmsMarkAsReadFailed(int32_t aError, int32_t aRequestId);
};

} // namespace

#endif // SmsManager_h__
