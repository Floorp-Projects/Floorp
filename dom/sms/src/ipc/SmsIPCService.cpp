/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "SmsIPCService.h"
#include "nsXULAppAPI.h"
#include "jsapi.h"
#include "mozilla/dom/sms/SmsChild.h"
#include "mozilla/dom/sms/SmsMessage.h"
#include "SmsFilter.h"

namespace mozilla {
namespace dom {
namespace sms {

PSmsChild* SmsIPCService::sSmsChild = nsnull;

NS_IMPL_ISUPPORTS2(SmsIPCService, nsISmsService, nsISmsDatabaseService)

/* static */ PSmsChild*
SmsIPCService::GetSmsChild()
{
  if (!sSmsChild) {
    sSmsChild = ContentChild::GetSingleton()->SendPSmsConstructor();
  }

  return sSmsChild;
}

/*
 * Implementation of nsISmsService.
 */
NS_IMETHODIMP
SmsIPCService::HasSupport(bool* aHasSupport)
{
  GetSmsChild()->SendHasSupport(aHasSupport);

  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::GetNumberOfMessagesForText(const nsAString& aText, PRUint16* aResult)
{
  GetSmsChild()->SendGetNumberOfMessagesForText(nsString(aText), aResult);

  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::Send(const nsAString& aNumber, const nsAString& aMessage,
                    PRInt32 aRequestId, PRUint64 aProcessId)
{
  GetSmsChild()->SendSendMessage(nsString(aNumber), nsString(aMessage),
                                 aRequestId, ContentChild::GetSingleton()->GetID());

  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::CreateSmsMessage(PRInt32 aId,
                                const nsAString& aDelivery,
                                const nsAString& aSender,
                                const nsAString& aReceiver,
                                const nsAString& aBody,
                                const jsval& aTimestamp,
                                const bool aRead,
                                JSContext* aCx,
                                nsIDOMMozSmsMessage** aMessage)
{
  return SmsMessage::Create(aId, aDelivery, aSender, aReceiver, aBody,
                            aTimestamp, aRead, aCx, aMessage);
}

/*
 * Implementation of nsISmsDatabaseService.
 */
NS_IMETHODIMP
SmsIPCService::SaveReceivedMessage(const nsAString& aSender,
                                   const nsAString& aBody,
                                   PRUint64 aDate, PRInt32* aId)
{
  GetSmsChild()->SendSaveReceivedMessage(nsString(aSender), nsString(aBody),
                                         aDate, aId);

  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::SaveSentMessage(const nsAString& aReceiver,
                               const nsAString& aBody,
                               PRUint64 aDate, PRInt32* aId)
{
  GetSmsChild()->SendSaveSentMessage(nsString(aReceiver), nsString(aBody),
                                     aDate, aId);

  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::GetMessageMoz(PRInt32 aMessageId, PRInt32 aRequestId,
                             PRUint64 aProcessId)
{
  GetSmsChild()->SendGetMessage(aMessageId, aRequestId,
                                ContentChild::GetSingleton()->GetID());
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::DeleteMessage(PRInt32 aMessageId, PRInt32 aRequestId,
                             PRUint64 aProcessId)
{
  GetSmsChild()->SendDeleteMessage(aMessageId, aRequestId,
                                   ContentChild::GetSingleton()->GetID());
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::CreateMessageList(nsIDOMMozSmsFilter* aFilter, bool aReverse,
                                 PRInt32 aRequestId, PRUint64 aProcessId)
{
  SmsFilter* filter = static_cast<SmsFilter*>(aFilter);
  GetSmsChild()->SendCreateMessageList(filter->GetData(), aReverse, aRequestId,
                                       ContentChild::GetSingleton()->GetID());

  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::GetNextMessageInList(PRInt32 aListId, PRInt32 aRequestId,
                                    PRUint64 aProcessId)
{
  GetSmsChild()->SendGetNextMessageInList(aListId, aRequestId,
                                          ContentChild::GetSingleton()->GetID());
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::ClearMessageList(PRInt32 aListId)
{
  GetSmsChild()->SendClearMessageList(aListId);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::MarkMessageRead(PRInt32 aMessageId, bool aValue,
                               PRInt32 aRequestId, PRUint64 aProcessId)
{
  GetSmsChild()->SendMarkMessageRead(aMessageId, aValue, aRequestId,
                                     ContentChild::GetSingleton()->GetID());
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
