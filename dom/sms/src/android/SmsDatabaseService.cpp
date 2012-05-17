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
                                        PRUint64 aDate, PRInt32* aId)
{
  // The Android stock SMS app does this already.
  *aId = -1;
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::SaveSentMessage(const nsAString& aReceiver,
                                    const nsAString& aBody,
                                    PRUint64 aDate, PRInt32* aId)
{
  *aId = -1;

  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  *aId = AndroidBridge::Bridge()->SaveSentMessage(aReceiver, aBody, aDate);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::GetMessageMoz(PRInt32 aMessageId, PRInt32 aRequestId,
                                  PRUint64 aProcessId)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->GetMessage(aMessageId, aRequestId, aProcessId);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::DeleteMessage(PRInt32 aMessageId, PRInt32 aRequestId,
                                  PRUint64 aProcessId)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->DeleteMessage(aMessageId, aRequestId, aProcessId);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::CreateMessageList(nsIDOMMozSmsFilter* aFilter,
                                      bool aReverse, PRInt32 aRequestId,
                                      PRUint64 aProcessId)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->CreateMessageList(
    static_cast<SmsFilter*>(aFilter)->GetData(), aReverse, aRequestId, aProcessId
  );
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::GetNextMessageInList(PRInt32 aListId, PRInt32 aRequestId,
                                         PRUint64 aProcessId)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->GetNextMessageInList(aListId, aRequestId, aProcessId);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::ClearMessageList(PRInt32 aListId)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->ClearMessageList(aListId);
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::MarkMessageRead(PRInt32 aMessageId, bool aValue,
                                    PRInt32 aRequestId, PRUint64 aProcessId)
{
  // TODO: This would need to be implemented as part of Bug 748391
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
