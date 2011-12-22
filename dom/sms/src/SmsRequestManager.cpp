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

#include "SmsRequestManager.h"
#include "nsIDOMSmsMessage.h"
#include "nsDOMEvent.h"
#include "SmsCursor.h"

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define SUCCESS_EVENT_NAME NS_LITERAL_STRING("success")
#define ERROR_EVENT_NAME   NS_LITERAL_STRING("error")

namespace mozilla {
namespace dom {
namespace sms {

SmsRequestManager* SmsRequestManager::sInstance = nsnull;

void
SmsRequestManager::Init()
{
  NS_PRECONDITION(!sInstance,
                  "sInstance shouldn't be set. Did you call Init() twice?");
  sInstance = new SmsRequestManager();
}

void
SmsRequestManager::Shutdown()
{
  NS_PRECONDITION(sInstance, "sInstance should be set. Did you call Init()?");

  delete sInstance;
  sInstance = nsnull;
}

/* static */ SmsRequestManager*
SmsRequestManager::GetInstance()
{
  return sInstance;
}

PRInt32
SmsRequestManager::AddRequest(nsIDOMMozSmsRequest* aRequest)
{
  // TODO: merge with CreateRequest
  PRInt32 size = mRequests.Count();

  // Look for empty slots.
  for (PRInt32 i=0; i<size; ++i) {
    if (mRequests[i]) {
      continue;
    }

    mRequests.ReplaceObjectAt(aRequest, i);
    return i;
  }

  mRequests.AppendObject(aRequest);
  return size;
}

PRInt32
SmsRequestManager::CreateRequest(nsPIDOMWindow* aWindow,
                                 nsIScriptContext* aScriptContext,
                                 nsIDOMMozSmsRequest** aRequest)
{
  nsCOMPtr<nsIDOMMozSmsRequest> request =
    new SmsRequest(aWindow, aScriptContext);

  PRInt32 size = mRequests.Count();

  // Look for empty slots.
  for (PRInt32 i=0; i<size; ++i) {
    if (mRequests[i]) {
      continue;
    }

    mRequests.ReplaceObjectAt(request, i);
    NS_ADDREF(*aRequest = request);
    return i;
  }

  mRequests.AppendObject(request);
  NS_ADDREF(*aRequest = request);
  return size;
}

nsresult
SmsRequestManager::DispatchTrustedEventToRequest(const nsAString& aEventName,
                                                 nsIDOMMozSmsRequest* aRequest)
{
  nsRefPtr<nsDOMEvent> event = new nsDOMEvent(nsnull, nsnull);
  nsresult rv = event->InitEvent(aEventName, false, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = event->SetTrusted(PR_TRUE);
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
void
SmsRequestManager::NotifySuccess(PRInt32 aRequestId, T aParam)
{
  SmsRequest* request = GetRequest(aRequestId);
  request->SetSuccess(aParam);

  DispatchTrustedEventToRequest(SUCCESS_EVENT_NAME, request);

  mRequests.ReplaceObjectAt(nsnull, aRequestId);
}

void
SmsRequestManager::NotifyError(PRInt32 aRequestId, SmsRequest::ErrorType aError)
{
  SmsRequest* request = GetRequest(aRequestId);
  request->SetError(aError);

  DispatchTrustedEventToRequest(ERROR_EVENT_NAME, request);

  mRequests.ReplaceObjectAt(nsnull, aRequestId);
}

void
SmsRequestManager::NotifySmsSent(PRInt32 aRequestId, nsIDOMMozSmsMessage* aMessage)
{
  NotifySuccess<nsIDOMMozSmsMessage*>(aRequestId, aMessage);
}

void
SmsRequestManager::NotifySmsSendFailed(PRInt32 aRequestId, SmsRequest::ErrorType aError)
{
  NotifyError(aRequestId, aError);
}

void
SmsRequestManager::NotifyGotSms(PRInt32 aRequestId, nsIDOMMozSmsMessage* aMessage)
{
  NotifySuccess<nsIDOMMozSmsMessage*>(aRequestId, aMessage);
}

void
SmsRequestManager::NotifyGetSmsFailed(PRInt32 aRequestId,
                                      SmsRequest::ErrorType aError)
{
  NotifyError(aRequestId, aError);
}

void
SmsRequestManager::NotifySmsDeleted(PRInt32 aRequestId, bool aDeleted)
{
  NotifySuccess<bool>(aRequestId, aDeleted);
}

void
SmsRequestManager::NotifySmsDeleteFailed(PRInt32 aRequestId, SmsRequest::ErrorType aError)
{
  NotifyError(aRequestId, aError);
}

void
SmsRequestManager::NotifyNoMessageInList(PRInt32 aRequestId)
{
  SmsRequest* request = GetRequest(aRequestId);

  nsCOMPtr<nsIDOMMozSmsCursor> cursor = request->GetCursor();
  if (!cursor) {
    cursor = new SmsCursor();
  } else {
    static_cast<SmsCursor*>(cursor.get())->Disconnect();
  }

  NotifySuccess<nsIDOMMozSmsCursor*>(aRequestId, cursor);
}

void
SmsRequestManager::NotifyCreateMessageList(PRInt32 aRequestId, PRInt32 aListId,
                                           nsIDOMMozSmsMessage* aMessage)
{
  SmsRequest* request = GetRequest(aRequestId);

  nsCOMPtr<SmsCursor> cursor = new SmsCursor(aListId, request);
  cursor->SetMessage(aMessage);

  NotifySuccess<nsIDOMMozSmsCursor*>(aRequestId, cursor);
}

void
SmsRequestManager::NotifyGotNextMessage(PRInt32 aRequestId, nsIDOMMozSmsMessage* aMessage)
{
  SmsRequest* request = GetRequest(aRequestId);

  nsCOMPtr<SmsCursor> cursor = static_cast<SmsCursor*>(request->GetCursor());
  NS_ASSERTION(cursor, "Request should have an cursor in that case!");
  cursor->SetMessage(aMessage);

  NotifySuccess<nsIDOMMozSmsCursor*>(aRequestId, cursor);
}

void
SmsRequestManager::NotifyReadMessageListFailed(PRInt32 aRequestId,
                                               SmsRequest::ErrorType aError)
{
  SmsRequest* request = GetRequest(aRequestId);

  nsCOMPtr<nsIDOMMozSmsCursor> cursor = request->GetCursor();
  if (cursor) {
    static_cast<SmsCursor*>(cursor.get())->Disconnect();
  }

  NotifyError(aRequestId, aError);
}

} // namespace sms
} // namespace dom
} // namespace mozilla
