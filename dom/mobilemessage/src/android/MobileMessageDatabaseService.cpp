/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsFilter.h"
#include "MobileMessageDatabaseService.h"
#include "AndroidBridge.h"

using namespace mozilla::dom::sms;

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS1(MobileMessageDatabaseService, nsIMobileMessageDatabaseService)

NS_IMETHODIMP
MobileMessageDatabaseService::GetMessageMoz(int32_t aMessageId, nsISmsRequest* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->GetMessage(aMessageId, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::DeleteMessage(int32_t aMessageId, nsISmsRequest* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->DeleteMessage(aMessageId, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::CreateMessageList(nsIDOMMozSmsFilter* aFilter,
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
MobileMessageDatabaseService::GetNextMessageInList(int32_t aListId, nsISmsRequest* aRequest)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->GetNextMessageInList(aListId, aRequest);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::ClearMessageList(int32_t aListId)
{
  if (!AndroidBridge::Bridge()) {
    return NS_OK;
  }

  AndroidBridge::Bridge()->ClearMessageList(aListId);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::MarkMessageRead(int32_t aMessageId, bool aValue,
                                              nsISmsRequest* aRequest)
{
  // TODO: This would need to be implemented as part of Bug 748391
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageDatabaseService::GetThreadList(nsISmsRequest* aRequest)
{
  NS_NOTYETIMPLEMENTED("Implement me!");
  return NS_ERROR_NOT_IMPLEMENTED;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
