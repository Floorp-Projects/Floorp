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

namespace mozilla {
namespace dom {
namespace sms {

nsTArray<SmsParent*>* SmsParent::gSmsParents = nsnull;

NS_IMPL_ISUPPORTS1(SmsParent, nsIObserver)

/* static */ void
SmsParent::GetAll(nsTArray<SmsParent*>& aArray)
{
  if (!gSmsParents) {
    aArray.Clear();
    return;
  }

  aArray = *gSmsParents;
}

SmsParent::SmsParent()
{
  if (!gSmsParents) {
    gSmsParents = new nsTArray<SmsParent*>();
  }

  gSmsParents->AppendElement(this);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return;
  }

  obs->AddObserver(this, kSmsReceivedObserverTopic, false);
  obs->AddObserver(this, kSmsSentObserverTopic, false);
  obs->AddObserver(this, kSmsDeliveredObserverTopic, false);
}

void
SmsParent::ActorDestroy(ActorDestroyReason why)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (!obs) {
    return;
  }

  obs->RemoveObserver(this, kSmsReceivedObserverTopic);
  obs->RemoveObserver(this, kSmsSentObserverTopic);
  obs->RemoveObserver(this, kSmsDeliveredObserverTopic);

  NS_ASSERTION(gSmsParents, "gSmsParents can't be null at that point!");
  gSmsParents->RemoveElement(this);
  if (gSmsParents->Length() == 0) {
    delete gSmsParents;
    gSmsParents = nsnull;
  }
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

  if (!strcmp(aTopic, kSmsSentObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-sent' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifySentMessage(static_cast<SmsMessage*>(message.get())->GetData());
    return NS_OK;
  }

  if (!strcmp(aTopic, kSmsDeliveredObserverTopic)) {
    nsCOMPtr<nsIDOMMozSmsMessage> message = do_QueryInterface(aSubject);
    if (!message) {
      NS_ERROR("Got a 'sms-delivered' topic without a valid message!");
      return NS_OK;
    }

    unused << SendNotifyDeliveredMessage(static_cast<SmsMessage*>(message.get())->GetData());
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
SmsParent::RecvGetNumberOfMessagesForText(const nsString& aText, PRUint16* aResult)
{
  *aResult = 0;

  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, true);

  smsService->GetNumberOfMessagesForText(aText, aResult);
  return true;
}

bool
SmsParent::RecvSendMessage(const nsString& aNumber, const nsString& aMessage,
                           const PRInt32& aRequestId, const PRUint64& aProcessId)
{
  nsCOMPtr<nsISmsService> smsService = do_GetService(SMS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsService, true);

  smsService->Send(aNumber, aMessage, aRequestId, aProcessId);
  return true;
}

bool
SmsParent::RecvSaveSentMessage(const nsString& aRecipient,
                               const nsString& aBody,
                               const PRUint64& aDate, PRInt32* aId)
{
  *aId = -1;

  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  smsDBService->SaveSentMessage(aRecipient, aBody, aDate, aId);
  return true;
}

bool
SmsParent::RecvGetMessage(const PRInt32& aMessageId, const PRInt32& aRequestId,
                          const PRUint64& aProcessId)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  smsDBService->GetMessageMoz(aMessageId, aRequestId, aProcessId);
  return true;
}

bool
SmsParent::RecvDeleteMessage(const PRInt32& aMessageId, const PRInt32& aRequestId,
                             const PRUint64& aProcessId)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  smsDBService->DeleteMessage(aMessageId, aRequestId, aProcessId);
  return true;
}

bool
SmsParent::RecvCreateMessageList(const SmsFilterData& aFilter,
                                 const bool& aReverse,
                                 const PRInt32& aRequestId,
                                 const PRUint64& aProcessId)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  nsCOMPtr<nsIDOMMozSmsFilter> filter = new SmsFilter(aFilter);
  smsDBService->CreateMessageList(filter, aReverse, aRequestId, aProcessId);

  return true;
}

bool
SmsParent::RecvGetNextMessageInList(const PRInt32& aListId,
                                    const PRInt32& aRequestId,
                                    const PRUint64& aProcessId)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  smsDBService->GetNextMessageInList(aListId, aRequestId, aProcessId);

  return true;
}

bool
SmsParent::RecvClearMessageList(const PRInt32& aListId)
{
  nsCOMPtr<nsISmsDatabaseService> smsDBService =
    do_GetService(SMS_DATABASE_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE(smsDBService, true);

  smsDBService->ClearMessageList(aListId);

  return true;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
