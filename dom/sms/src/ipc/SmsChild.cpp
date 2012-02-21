/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mounir Lamouri <mounir.lamouri@mozilla.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

} // namespace sms
} // namespace dom
} // namespace mozilla
