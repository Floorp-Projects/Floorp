/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/sms/SmsMessage.h"
#include "SmsService.h"
#include "jsapi.h"
#include "SmsSegmentInfo.h"

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_ISUPPORTS1(SmsService, nsISmsService)

SmsService::SmsService()
{
  mRIL = do_GetService("@mozilla.org/ril;1");
  NS_WARN_IF_FALSE(mRIL, "This shouldn't fail!");
}

NS_IMETHODIMP
SmsService::HasSupport(bool* aHasSupport)
{
  *aHasSupport = true;
  return NS_OK;
}

NS_IMETHODIMP
SmsService::GetSegmentInfoForText(const nsAString & aText,
                                  nsIDOMMozSmsSegmentInfo** aResult)
{
  NS_ENSURE_TRUE(mRIL, NS_ERROR_FAILURE);

  return mRIL->GetSegmentInfoForText(aText, aResult);
}

NS_IMETHODIMP
SmsService::Send(const nsAString& aNumber,
                 const nsAString& aMessage,
                 nsISmsRequest* aRequest)
{
  if (!mRIL) {
    return NS_OK;
  }
  mRIL->SendSMS(aNumber, aMessage, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsService::CreateSmsMessage(int32_t aId,
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
SmsService::CreateSmsSegmentInfo(int32_t aSegments,
                                 int32_t aCharsPerSegment,
                                 int32_t aCharsAvailableInLastSegment,
                                 nsIDOMMozSmsSegmentInfo** aSegmentInfo)
{
  nsCOMPtr<nsIDOMMozSmsSegmentInfo> info =
      new SmsSegmentInfo(aSegments, aCharsPerSegment, aCharsAvailableInLastSegment);
  info.forget(aSegmentInfo);
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
