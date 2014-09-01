/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageDatabaseService.h"
#include "AndroidBridge.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS(MobileMessageDatabaseService, nsIMobileMessageDatabaseService)

NS_IMETHODIMP
MobileMessageDatabaseService::GetMessageMoz(int32_t aMessageId,
                                            nsIMobileMessageCallback* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->GetMessage(aMessageId, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::DeleteMessage(int32_t *aMessageIds,
                                            uint32_t aLength,
                                            nsIMobileMessageCallback* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  if (!aMessageIds) {
    return NS_OK;
  }

  if (aLength != 1) {
    return NS_ERROR_FAILURE;
  }

  AndroidBridge::Bridge()->DeleteMessage(aMessageIds[0], aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::CreateMessageCursor(bool aHasStartDate,
                                                  uint64_t aStartDate,
                                                  bool aHasEndDate,
                                                  uint64_t aEndDate,
                                                  const char16_t** aNumbers,
                                                  uint32_t aNumbersCount,
                                                  const nsAString& aDelivery,
                                                  bool aHasRead,
                                                  bool aRead,
                                                  uint64_t aThreadId,
                                                  bool aReverse,
                                                  nsIMobileMessageCursorCallback* aCallback,
                                                  nsICursorContinueCallback** aResult)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MobileMessageDatabaseService::MarkMessageRead(int32_t aMessageId,
                                              bool aValue,
                                              bool aSendReadReport,
                                              nsIMobileMessageCallback* aRequest)
{
  // TODO: This would need to be implemented as part of Bug 748391
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::CreateThreadCursor(nsIMobileMessageCursorCallback* aCallback,
                                                 nsICursorContinueCallback** aResult)
{
  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
