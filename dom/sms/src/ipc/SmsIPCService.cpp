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
#include "SmsRequest.h"
#include "SmsSegmentInfo.h"

namespace mozilla {
namespace dom {
namespace sms {

PSmsChild* gSmsChild;

NS_IMPL_ISUPPORTS2(SmsIPCService, nsISmsService, nsIMobileMessageDatabaseService)

void
SendRequest(const IPCSmsRequest& aRequest, nsISmsRequest* aRequestReply)
{
  MOZ_ASSERT(NS_IsMainThread());

  NS_WARN_IF_FALSE(gSmsChild,
                   "Calling methods on SmsIPCService during "
                   "shutdown!");

  if (gSmsChild) {
    SmsRequestChild* actor = new SmsRequestChild(aRequestReply);
    gSmsChild->SendPSmsRequestConstructor(actor, aRequest);
  }
}

PSmsChild*
SmsIPCService::GetSmsChild()
{
  if (!gSmsChild) {
    gSmsChild = ContentChild::GetSingleton()->SendPSmsConstructor();
  }

  return gSmsChild;
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
SmsIPCService::GetSegmentInfoForText(const nsAString & aText,
                                     nsIDOMMozSmsSegmentInfo** aResult)
{
  SmsSegmentInfoData data;
  bool ok = GetSmsChild()->SendGetSegmentInfoForText(nsString(aText), &data);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMMozSmsSegmentInfo> info = new SmsSegmentInfo(data);
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::Send(const nsAString& aNumber,
                    const nsAString& aMessage,
                    nsISmsRequest* aRequest)
{
  SendRequest(SendMessageRequest(nsString(aNumber), nsString(aMessage)), aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::CreateSmsMessage(int32_t aId,
                                const nsAString& aDelivery,
                                const nsAString& aDeliveryStatus,
                                const nsAString& aSender,
                                const nsAString& aReceiver,
                                const nsAString& aBody,
                                const nsAString& aMessageClass,
                                const jsval& aTimestamp,
                                const bool aRead,
                                JSContext* aCx,
                                nsIDOMMozSmsMessage** aMessage)
{
  return SmsMessage::Create(aId, aDelivery, aDeliveryStatus,
                            aSender, aReceiver,
                            aBody, aMessageClass, aTimestamp, aRead,
                            aCx, aMessage);
}

NS_IMETHODIMP
SmsIPCService::CreateSmsSegmentInfo(int32_t aSegments,
                                    int32_t aCharsPerSegment,
                                    int32_t aCharsAvailableInLastSegment,
                                    nsIDOMMozSmsSegmentInfo** aSegmentInfo)
{
  nsCOMPtr<nsIDOMMozSmsSegmentInfo> info =
      new SmsSegmentInfo(aSegments, aCharsPerSegment, aCharsAvailableInLastSegment);
  info.forget(aSegmentInfo);
  return NS_OK;
}

/*
 * Implementation of nsIMobileMessageDatabaseService.
 */
NS_IMETHODIMP
SmsIPCService::GetMessageMoz(int32_t aMessageId,
                             nsISmsRequest* aRequest)
{
  SendRequest(GetMessageRequest(aMessageId), aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::DeleteMessage(int32_t aMessageId,
                             nsISmsRequest* aRequest)
{
  SendRequest(DeleteMessageRequest(aMessageId), aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::CreateMessageList(nsIDOMMozSmsFilter* aFilter,
                                 bool aReverse,
                                 nsISmsRequest* aRequest)
{
  SmsFilterData data = SmsFilterData(static_cast<SmsFilter*>(aFilter)->GetData());
  SendRequest(CreateMessageListRequest(data, aReverse), aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::GetNextMessageInList(int32_t aListId,
                                    nsISmsRequest* aRequest)
{
  SendRequest(GetNextMessageInListRequest(aListId), aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::ClearMessageList(int32_t aListId)
{
  GetSmsChild()->SendClearMessageList(aListId);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::MarkMessageRead(int32_t aMessageId,
                               bool aValue,
                               nsISmsRequest* aRequest)
{
  SendRequest(MarkMessageReadRequest(aMessageId, aValue), aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::GetThreadList(nsISmsRequest* aRequest)
{
  SendRequest(GetThreadListRequest(), aRequest);
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
