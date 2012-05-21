/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsChild.h"
#include "SmsMessage.h"
#include "Constants.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozilla/dom/ContentChild.h"
#include "SmsRequestManager.h"
#include "SmsRequest.h"

namespace mozilla {
namespace dom {
namespace sms {

bool
SmsChild::RecvNotifyReceivedMessage(const SmsMessageData& aMessageData)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return true;
  }

  nsCOMPtr<SmsMessage> message = new SmsMessage(aMessageData);
  obs->NotifyObservers(message, kSmsReceivedObserverTopic, nsnull);

  return true;
}

bool
SmsChild::RecvNotifySentMessage(const SmsMessageData& aMessageData)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return true;
  }

  nsCOMPtr<SmsMessage> message = new SmsMessage(aMessageData);
  obs->NotifyObservers(message, kSmsSentObserverTopic, nsnull);

  return true;
}

bool
SmsChild::RecvNotifyDeliveredMessage(const SmsMessageData& aMessageData)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return true;
  }

  nsCOMPtr<SmsMessage> message = new SmsMessage(aMessageData);
  obs->NotifyObservers(message, kSmsDeliveredObserverTopic, nsnull);

  return true;
}

bool
SmsChild::RecvNotifyRequestSmsSent(const SmsMessageData& aMessage,
                                   const PRInt32& aRequestId,
                                   const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(aMessage);
  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifySmsSent(aRequestId, message);

  return true;
}

bool
SmsChild::RecvNotifyRequestSmsSendFailed(const PRInt32& aError,
                                         const PRInt32& aRequestId,
                                         const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifySmsSendFailed(aRequestId, aError);

  return true;
}

bool
SmsChild::RecvNotifyRequestGotSms(const SmsMessageData& aMessage,
                                  const PRInt32& aRequestId,
                                  const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(aMessage);
  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifyGotSms(aRequestId, message);

  return true;
}

bool
SmsChild::RecvNotifyRequestGetSmsFailed(const PRInt32& aError,
                                        const PRInt32& aRequestId,
                                        const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifyGetSmsFailed(aRequestId, aError);

  return true;
}

bool
SmsChild::RecvNotifyRequestSmsDeleted(const bool& aDeleted,
                                      const PRInt32& aRequestId,
                                      const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifySmsDeleted(aRequestId, aDeleted);

  return true;
}

bool
SmsChild::RecvNotifyRequestSmsDeleteFailed(const PRInt32& aError,
                                           const PRInt32& aRequestId,
                                           const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifySmsDeleteFailed(aRequestId, aError);

  return true;
}

bool
SmsChild::RecvNotifyRequestNoMessageInList(const PRInt32& aRequestId,
                                           const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifyNoMessageInList(aRequestId);
  return true;
}

bool
SmsChild::RecvNotifyRequestCreateMessageList(const PRInt32& aListId,
                                             const SmsMessageData& aMessageData,
                                             const PRInt32& aRequestId,
                                             const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(aMessageData);
  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifyCreateMessageList(aRequestId, aListId, message);
  return true;
}

bool
SmsChild::RecvNotifyRequestGotNextMessage(const SmsMessageData& aMessageData,
                                          const PRInt32& aRequestId,
                                          const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsIDOMMozSmsMessage> message = new SmsMessage(aMessageData);
  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifyGotNextMessage(aRequestId, message);
  return true;
}

bool
SmsChild::RecvNotifyRequestReadListFailed(const PRInt32& aError,
                                          const PRInt32& aRequestId,
                                          const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsISmsRequestManager> requestManager = do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifyReadMessageListFailed(aRequestId, aError);
  return true;
}

bool
SmsChild::RecvNotifyRequestMarkedMessageRead(const bool& aRead,
                                             const PRInt32& aRequestId,
                                             const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsISmsRequestManager> requestManager =
    do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifyMarkedMessageRead(aRequestId, aRead);
  return true;
}

bool
SmsChild::RecvNotifyRequestMarkMessageReadFailed(const PRInt32& aError,
                                                 const PRInt32& aRequestId,
                                                 const PRUint64& aProcessId)
{
  if (ContentChild::GetSingleton()->GetID() != aProcessId) {
    return true;
  }

  nsCOMPtr<nsISmsRequestManager> requestManager =
    do_GetService(SMS_REQUEST_MANAGER_CONTRACTID);
  requestManager->NotifyMarkMessageReadFailed(aRequestId, aError);

  return true;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
