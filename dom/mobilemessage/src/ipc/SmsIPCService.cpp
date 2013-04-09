/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "SmsIPCService.h"
#include "nsXULAppAPI.h"
#include "jsapi.h"
#include "mozilla/dom/mobilemessage/SmsChild.h"
#include "SmsMessage.h"
#include "SmsFilter.h"
#include "SmsSegmentInfo.h"

using namespace mozilla::dom;
using namespace mozilla::dom::mobilemessage;

namespace {

// TODO: Bug 767082 - WebSMS: sSmsChild leaks at shutdown
PSmsChild* gSmsChild;

PSmsChild*
GetSmsChild()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (!gSmsChild) {
    gSmsChild = ContentChild::GetSingleton()->SendPSmsConstructor();

    NS_WARN_IF_FALSE(gSmsChild,
                     "Calling methods on SmsIPCService during shutdown!");
  }

  return gSmsChild;
}

nsresult
SendRequest(const IPCSmsRequest& aRequest,
            nsIMobileMessageCallback* aRequestReply)
{
  PSmsChild* smsChild = GetSmsChild();
  NS_ENSURE_TRUE(smsChild, NS_ERROR_FAILURE);

  SmsRequestChild* actor = new SmsRequestChild(aRequestReply);
  smsChild->SendPSmsRequestConstructor(actor, aRequest);

  return NS_OK;
}

nsresult
SendCursorRequest(const IPCMobileMessageCursor& aRequest,
                  nsIMobileMessageCursorCallback* aRequestReply,
                  nsICursorContinueCallback** aResult)
{
  PSmsChild* smsChild = GetSmsChild();
  NS_ENSURE_TRUE(smsChild, NS_ERROR_FAILURE);

  nsRefPtr<MobileMessageCursorChild> actor =
    new MobileMessageCursorChild(aRequestReply);

  // Add an extra ref for IPDL. Will be released in
  // SmsChild::DeallocPMobileMessageCursor().
  actor->AddRef();

  smsChild->SendPMobileMessageCursorConstructor(actor, aRequest);

  actor.forget(aResult);
  return NS_OK;
}

} // anonymous namespace

NS_IMPL_ISUPPORTS2(SmsIPCService,
                   nsISmsService,
                   nsIMobileMessageDatabaseService);

/*
 * Implementation of nsISmsService.
 */
NS_IMETHODIMP
SmsIPCService::HasSupport(bool* aHasSupport)
{
  PSmsChild* smsChild = GetSmsChild();
  NS_ENSURE_TRUE(smsChild, NS_ERROR_FAILURE);

  smsChild->SendHasSupport(aHasSupport);

  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::GetSegmentInfoForText(const nsAString & aText,
                                     nsIDOMMozSmsSegmentInfo** aResult)
{
  PSmsChild* smsChild = GetSmsChild();
  NS_ENSURE_TRUE(smsChild, NS_ERROR_FAILURE);

  SmsSegmentInfoData data;
  bool ok = smsChild->SendGetSegmentInfoForText(nsString(aText), &data);
  NS_ENSURE_TRUE(ok, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMMozSmsSegmentInfo> info = new SmsSegmentInfo(data);
  info.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
SmsIPCService::Send(const nsAString& aNumber,
                    const nsAString& aMessage,
                    nsIMobileMessageCallback* aRequest)
{
  return SendRequest(SendMessageRequest(nsString(aNumber), nsString(aMessage)),
                     aRequest);
}

/*
 * Implementation of nsIMobileMessageDatabaseService.
 */
NS_IMETHODIMP
SmsIPCService::GetMessageMoz(int32_t aMessageId,
                             nsIMobileMessageCallback* aRequest)
{
  return SendRequest(GetMessageRequest(aMessageId), aRequest);
}

NS_IMETHODIMP
SmsIPCService::DeleteMessage(int32_t aMessageId,
                             nsIMobileMessageCallback* aRequest)
{
  return SendRequest(DeleteMessageRequest(aMessageId), aRequest);
}

NS_IMETHODIMP
SmsIPCService::CreateMessageCursor(nsIDOMMozSmsFilter* aFilter,
                                   bool aReverse,
                                   nsIMobileMessageCursorCallback* aCursorCallback,
                                   nsICursorContinueCallback** aResult)
{
  const SmsFilterData& data =
    SmsFilterData(static_cast<SmsFilter*>(aFilter)->GetData());

  return SendCursorRequest(CreateMessageCursorRequest(data, aReverse),
                           aCursorCallback, aResult);
}

NS_IMETHODIMP
SmsIPCService::MarkMessageRead(int32_t aMessageId,
                               bool aValue,
                               nsIMobileMessageCallback* aRequest)
{
  return SendRequest(MarkMessageReadRequest(aMessageId, aValue), aRequest);
}

NS_IMETHODIMP
SmsIPCService::CreateThreadCursor(nsIMobileMessageCursorCallback* aCursorCallback,
                                  nsICursorContinueCallback** aResult)
{
  return SendCursorRequest(CreateThreadCursorRequest(), aCursorCallback,
                           aResult);
}
