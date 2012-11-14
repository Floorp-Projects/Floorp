/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsFilter.h"
#include "SmsDatabaseService.h"
#include "AndroidBridge.h"

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_ISUPPORTS1(SmsDatabaseService, nsISmsDatabaseService)

NS_IMETHODIMP
SmsDatabaseService::SaveReceivedMessage(const nsAString& aSender,
                                        const nsAString& aBody,
                                        const nsAString& aMessageClass,
                                        uint64_t aDate, int32_t* aId)
{
  // The Android stock SMS app does this already.
  *aId = -1;
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::SaveSentMessage(const nsAString& aReceiver,
                                    const nsAString& aBody,
                                    uint64_t aDate, int32_t* aId)
{
  *aId = -1;

  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  *aId = AndroidBridge::Bridge()->SaveSentMessage(aReceiver, aBody, aDate);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::SetMessageDeliveryStatus(int32_t aMessageId,
                                             const nsAString& aDeliveryStatus)
{
  // TODO: Bug 803828: update delivery status for sent messages in Android.
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::GetMessageMoz(int32_t aMessageId, nsISmsRequest* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->GetMessage(aMessageId, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::DeleteMessage(int32_t aMessageId, nsISmsRequest* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->DeleteMessage(aMessageId, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::CreateMessageList(nsIDOMMozSmsFilter* aFilter,
                                      bool aReverse, nsISmsRequest* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->CreateMessageList(
    static_cast<SmsFilter*>(aFilter)->GetData(), aReverse, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::GetNextMessageInList(int32_t aListId, nsISmsRequest* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->GetNextMessageInList(aListId, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::ClearMessageList(int32_t aListId)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->ClearMessageList(aListId);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::MarkMessageRead(int32_t aMessageId, bool aValue,
                                    nsISmsRequest* aRequest)
{
  // TODO: This would need to be implemented as part of Bug 748391
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::GetThreadList(nsISmsRequest* aRequest)
{
  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
