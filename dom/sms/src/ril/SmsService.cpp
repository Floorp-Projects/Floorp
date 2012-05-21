/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/sms/SmsMessage.h"
#include "SmsService.h"
#include "SystemWorkerManager.h"
#include "jsapi.h"
#include "nsIInterfaceRequestorUtils.h"

using mozilla::dom::gonk::SystemWorkerManager;

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_ISUPPORTS1(SmsService, nsISmsService)

SmsService::SmsService()
{
  nsIInterfaceRequestor* ireq = SystemWorkerManager::GetInterfaceRequestor();
  NS_WARN_IF_FALSE(ireq, "The SystemWorkerManager has not been created!");

  if (ireq) {
    mRIL = do_GetInterface(ireq);
    NS_WARN_IF_FALSE(mRIL, "This shouldn't fail!");
  }
}

NS_IMETHODIMP
SmsService::HasSupport(bool* aHasSupport)
{
  *aHasSupport = true;
  return NS_OK;
}

NS_IMETHODIMP
SmsService::GetNumberOfMessagesForText(const nsAString& aText, PRUint16* aResult)
{
  if (!mRIL) {
    *aResult = 0;
    return NS_OK;
  }

  mRIL->GetNumberOfMessagesForText(aText, aResult);
  return NS_OK;
}

NS_IMETHODIMP
SmsService::Send(const nsAString& aNumber,
                 const nsAString& aMessage,
                 PRInt32 aRequestId,
                 PRUint64 aProcessId)
{
  if (!mRIL) {
    return NS_OK;
  }

  mRIL->SendSMS(aNumber, aMessage, aRequestId, aProcessId);
  return NS_OK;
}

NS_IMETHODIMP
SmsService::CreateSmsMessage(PRInt32 aId,
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

} // namespace sms
} // namespace dom
} // namespace mozilla
