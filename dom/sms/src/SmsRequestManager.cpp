/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsRequestManager.h"
#include "nsIDOMSmsMessage.h"
#include "nsDOMEvent.h"
#include "SmsCursor.h"
#include "SmsManager.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define SUCCESS_EVENT_NAME NS_LITERAL_STRING("success")
#define ERROR_EVENT_NAME   NS_LITERAL_STRING("error")

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_ISUPPORTS1(SmsRequestManager, nsISmsRequestManager)

NS_IMETHODIMP
SmsRequestManager::AddRequest(nsIDOMMozSmsRequest* aRequest,
                              PRInt32* aRequestId)
{
  // TODO: merge with CreateRequest
  PRInt32 size = mRequests.Count();

  // Look for empty slots.
  for (PRInt32 i=0; i<size; ++i) {
    if (mRequests[i]) {
      continue;
    }

    mRequests.ReplaceObjectAt(aRequest, i);
    *aRequestId = i;
    return NS_OK;
  }

  mRequests.AppendObject(aRequest);
  *aRequestId = size;
  return NS_OK;
}


NS_IMETHODIMP
SmsRequestManager::CreateRequest(nsIDOMMozSmsManager* aManager,
                                 nsIDOMMozSmsRequest** aRequest,
                                 PRInt32* aRequestId)
{
  nsCOMPtr<nsIDOMMozSmsRequest> request =
    new SmsRequest(static_cast<SmsManager*>(aManager));

  PRInt32 size = mRequests.Count();

  // Look for empty slots.
  for (PRInt32 i=0; i<size; ++i) {
    if (mRequests[i]) {
      continue;
    }

    mRequests.ReplaceObjectAt(request, i);
    NS_ADDREF(*aRequest = request);
    *aRequestId = i;
    return NS_OK;
  }

  mRequests.AppendObject(request);
  NS_ADDREF(*aRequest = request);
  *aRequestId = size;
  return NS_OK;
}

nsresult
SmsRequestManager::DispatchTrustedEventToRequest(const nsAString& aEventName,
                                                 nsIDOMMozSmsRequest* aRequest)
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nullptr, nullptr);
  nsresult rv = event->InitEvent(aEventName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(true);
  NS_ENSURE_SUCCESS(rv, rv);

  bool dummy;
  return aRequest->DispatchEvent(event, &dummy);
}

SmsRequest*
SmsRequestManager::GetRequest(PRInt32 aRequestId)
{
  NS_ASSERTION(mRequests.Count() > aRequestId && mRequests[aRequestId],
               "Got an invalid request id or it has been already deleted!");

  // It's safe to use the static_cast here given that we did call
  // |new SmsRequest()|.
  return static_cast<SmsRequest*>(mRequests[aRequestId]);
}

template <class T>
nsresult
SmsRequestManager::NotifySuccess(PRInt32 aRequestId, T aParam)
{
  SmsRequest* request = GetRequest(aRequestId);
  request->SetSuccess(aParam);

  nsresult rv = DispatchTrustedEventToRequest(SUCCESS_EVENT_NAME, request);

  mRequests.ReplaceObjectAt(nullptr, aRequestId);
  return rv;
}

nsresult
SmsRequestManager::NotifyError(PRInt32 aRequestId, PRInt32 aError)
{
  SmsRequest* request = GetRequest(aRequestId);
  request->SetError(aError);

  nsresult rv = DispatchTrustedEventToRequest(ERROR_EVENT_NAME, request);

  mRequests.ReplaceObjectAt(nullptr, aRequestId);
  return rv;
}

NS_IMETHODIMP
SmsRequestManager::NotifySmsSent(PRInt32 aRequestId, nsIDOMMozSmsMessage* aMessage)
{
  return NotifySuccess<nsIDOMMozSmsMessage*>(aRequestId, aMessage);
}

NS_IMETHODIMP
SmsRequestManager::NotifySmsSendFailed(PRInt32 aRequestId, PRInt32 aError)
{
  return NotifyError(aRequestId, aError);
}

NS_IMETHODIMP
SmsRequestManager::NotifyGotSms(PRInt32 aRequestId, nsIDOMMozSmsMessage* aMessage)
{
  return NotifySuccess<nsIDOMMozSmsMessage*>(aRequestId, aMessage);
}

NS_IMETHODIMP
SmsRequestManager::NotifyGetSmsFailed(PRInt32 aRequestId, PRInt32 aError)
{
  return NotifyError(aRequestId, aError);
}

NS_IMETHODIMP
SmsRequestManager::NotifySmsDeleted(PRInt32 aRequestId, bool aDeleted)
{
  return NotifySuccess<bool>(aRequestId, aDeleted);
}

NS_IMETHODIMP
SmsRequestManager::NotifySmsDeleteFailed(PRInt32 aRequestId, PRInt32 aError)
{
  return NotifyError(aRequestId, aError);
}

NS_IMETHODIMP
SmsRequestManager::NotifyNoMessageInList(PRInt32 aRequestId)
{
  SmsRequest* request = GetRequest(aRequestId);

  nsCOMPtr<nsIDOMMozSmsCursor> cursor = request->GetCursor();
  if (!cursor) {
    cursor = new SmsCursor();
  } else {
    static_cast<SmsCursor*>(cursor.get())->Disconnect();
  }

  return NotifySuccess<nsIDOMMozSmsCursor*>(aRequestId, cursor);
}

NS_IMETHODIMP
SmsRequestManager::NotifyCreateMessageList(PRInt32 aRequestId, PRInt32 aListId,
                                           nsIDOMMozSmsMessage* aMessage)
{
  SmsRequest* request = GetRequest(aRequestId);

  nsCOMPtr<SmsCursor> cursor = new SmsCursor(aListId, request);
  cursor->SetMessage(aMessage);

  return NotifySuccess<nsIDOMMozSmsCursor*>(aRequestId, cursor);
}

NS_IMETHODIMP
SmsRequestManager::NotifyGotNextMessage(PRInt32 aRequestId, nsIDOMMozSmsMessage* aMessage)
{
  SmsRequest* request = GetRequest(aRequestId);

  nsCOMPtr<SmsCursor> cursor = static_cast<SmsCursor*>(request->GetCursor());
  NS_ASSERTION(cursor, "Request should have an cursor in that case!");
  cursor->SetMessage(aMessage);

  return NotifySuccess<nsIDOMMozSmsCursor*>(aRequestId, cursor);
}

NS_IMETHODIMP
SmsRequestManager::NotifyReadMessageListFailed(PRInt32 aRequestId, PRInt32 aError)
{
  SmsRequest* request = GetRequest(aRequestId);

  nsCOMPtr<nsIDOMMozSmsCursor> cursor = request->GetCursor();
  if (cursor) {
    static_cast<SmsCursor*>(cursor.get())->Disconnect();
  }

  return NotifyError(aRequestId, aError);
}

NS_IMETHODIMP
SmsRequestManager::NotifyMarkedMessageRead(PRInt32 aRequestId, bool aRead)
{
  return NotifySuccess<bool>(aRequestId, aRead);
}

NS_IMETHODIMP
SmsRequestManager::NotifyMarkMessageReadFailed(PRInt32 aRequestId, PRInt32 aError)
{
  return NotifyError(aRequestId, aError);
}

} // namespace sms
} // namespace dom
} // namespace mozilla
