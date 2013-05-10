/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageDatabaseService.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS1(MobileMessageDatabaseService, nsIMobileMessageDatabaseService)

NS_IMETHODIMP
MobileMessageDatabaseService::GetMessageMoz(int32_t aMessageId,
                                            nsIMobileMessageCallback* aRequest)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::DeleteMessage(int32_t *aMessageIds,
                                            uint32_t aLength,
                                            nsIMobileMessageCallback* aRequest)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::CreateMessageCursor(nsIDOMMozSmsFilter* aFilter,
                                                  bool aReverse,
                                                  nsIMobileMessageCursorCallback* aCallback,
                                                  nsICursorContinueCallback** aResult)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::MarkMessageRead(int32_t aMessageId,
                                              bool aValue,
                                              nsIMobileMessageCallback* aRequest)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::CreateThreadCursor(nsIMobileMessageCursorCallback* aCallback,
                                                 nsICursorContinueCallback** aResult)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
