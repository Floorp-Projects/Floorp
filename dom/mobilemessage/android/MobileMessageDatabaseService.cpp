/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageDatabaseService.h"

#include "AndroidBridge.h"
#include "SmsManager.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS(MobileMessageDatabaseService, nsIMobileMessageDatabaseService)

MobileMessageDatabaseService::MobileMessageDatabaseService()
{
  SmsManager::Init();
}

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
                                                  bool aHasThreadId,
                                                  uint64_t aThreadId,
                                                  bool aReverse,
                                                  nsIMobileMessageCursorCallback* aCallback,
                                                  nsICursorContinueCallback** aCursor)
{
  if (!AndroidBridge::Bridge()) {
    *aCursor = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsICursorContinueCallback> cursor =
    AndroidBridge::Bridge()->CreateMessageCursor(aHasStartDate,
                                                 aStartDate,
                                                 aHasEndDate,
                                                 aEndDate,
                                                 aNumbers,
                                                 aNumbersCount,
                                                 aDelivery,
                                                 aHasRead,
                                                 aRead,
                                                 aHasThreadId,
                                                 aThreadId,
                                                 aReverse,
                                                 aCallback);
  cursor.forget(aCursor);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::MarkMessageRead(int32_t aMessageId,
                                              bool aValue,
                                              bool aSendReadReport,
                                              nsIMobileMessageCallback* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->MarkMessageRead(aMessageId,
                                           aValue,
                                           aSendReadReport,
                                           aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::CreateThreadCursor(nsIMobileMessageCursorCallback* aRequest,
                                                 nsICursorContinueCallback** aCursor)
{
  if (!AndroidBridge::Bridge()) {
    *aCursor = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsICursorContinueCallback> cursor =
    AndroidBridge::Bridge()->CreateThreadCursor(aRequest);
  cursor.forget(aCursor);
  return NS_OK;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
