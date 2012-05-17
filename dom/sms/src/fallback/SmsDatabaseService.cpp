/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsDatabaseService.h"

namespace mozilla {
namespace dom {
namespace sms {

NS_IMPL_ISUPPORTS1(SmsDatabaseService, nsISmsDatabaseService)

NS_IMETHODIMP
SmsDatabaseService::SaveReceivedMessage(const nsAString& aSender,
                                        const nsAString& aBody,
                                        PRUint64 aDate, PRInt32* aId)
{
  *aId = -1;
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::SaveSentMessage(const nsAString& aReceiver,
                                    const nsAString& aBody,
                                    PRUint64 aDate, PRInt32* aId)
{
  *aId = -1;
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::GetMessageMoz(PRInt32 aMessageId, PRInt32 aRequestId,
                                  PRUint64 aProcessId)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::DeleteMessage(PRInt32 aMessageId, PRInt32 aRequestId,
                                  PRUint64 aProcessId)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::CreateMessageList(nsIDOMMozSmsFilter* aFilter,
                                      bool aReverse, PRInt32 aRequestId,
                                      PRUint64 aProcessId)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::GetNextMessageInList(PRInt32 aListId, PRInt32 aRequestId,
                                         PRUint64 aProcessId)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::ClearMessageList(PRInt32 aListId)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
SmsDatabaseService::MarkMessageRead(PRInt32 aMessageId, bool aValue,
                                  PRInt32 aRequestId, PRUint64 aProcessId)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

} // namespace sms
} // namespace dom
} // namespace mozilla
