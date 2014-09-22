/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsMessage.h"
#include "MmsMessage.h"
#include "MobileMessageThread.h"
#include "MobileMessageService.h"
#include "DeletedMessageInfo.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS(MobileMessageService, nsIMobileMessageService)

NS_IMETHODIMP
MobileMessageService::CreateSmsMessage(int32_t aId,
                                       uint64_t aThreadId,
                                       const nsAString& aIccId,
                                       const nsAString& aDelivery,
                                       const nsAString& aDeliveryStatus,
                                       const nsAString& aSender,
                                       const nsAString& aReceiver,
                                       const nsAString& aBody,
                                       const nsAString& aMessageClass,
                                       uint64_t aTimestamp,
                                       uint64_t aSentTimestamp,
                                       uint64_t aDeliveryTimestamp,
                                       bool aRead,
                                       JSContext* aCx,
                                       nsIDOMMozSmsMessage** aMessage)
{
  return SmsMessage::Create(aId,
                            aThreadId,
                            aIccId,
                            aDelivery,
                            aDeliveryStatus,
                            aSender,
                            aReceiver,
                            aBody,
                            aMessageClass,
                            aTimestamp,
                            aSentTimestamp,
                            aDeliveryTimestamp,
                            aRead,
                            aCx,
                            aMessage);
}

NS_IMETHODIMP
MobileMessageService::CreateMmsMessage(int32_t aId,
                                       uint64_t aThreadId,
                                       const nsAString& aIccId,
                                       const nsAString& aDelivery,
                                       JS::Handle<JS::Value> aDeliveryInfo,
                                       const nsAString& aSender,
                                       JS::Handle<JS::Value> aReceivers,
                                       uint64_t aTimestamp,
                                       uint64_t aSentTimestamp,
                                       bool aRead,
                                       const nsAString& aSubject,
                                       const nsAString& aSmil,
                                       JS::Handle<JS::Value> aAttachments,
                                       uint64_t aExpiryDate,
                                       bool aReadReportRequested,
                                       JSContext* aCx,
                                       nsIDOMMozMmsMessage** aMessage)
{
  return MmsMessage::Create(aId,
                            aThreadId,
                            aIccId,
                            aDelivery,
                            aDeliveryInfo,
                            aSender,
                            aReceivers,
                            aTimestamp,
                            aSentTimestamp,
                            aRead,
                            aSubject,
                            aSmil,
                            aAttachments,
                            aExpiryDate,
                            aReadReportRequested,
                            aCx,
                            aMessage);
}

NS_IMETHODIMP
MobileMessageService::CreateThread(uint64_t aId,
                                   JS::Handle<JS::Value> aParticipants,
                                   uint64_t aTimestamp,
                                   const nsAString& aLastMessageSubject,
                                   const nsAString& aBody,
                                   uint64_t aUnreadCount,
                                   const nsAString& aLastMessageType,
                                   JSContext* aCx,
                                   nsIDOMMozMobileMessageThread** aThread)
{
  return MobileMessageThread::Create(aId,
                                     aParticipants,
                                     aTimestamp,
                                     aLastMessageSubject,
                                     aBody,
                                     aUnreadCount,
                                     aLastMessageType,
                                     aCx,
                                     aThread);
}

NS_IMETHODIMP
MobileMessageService::CreateDeletedMessageInfo(int32_t* aMessageIds,
                                               uint32_t aMsgCount,
                                               uint64_t* aThreadIds,
                                               uint32_t  aThreadCount,
                                               nsIDeletedMessageInfo** aDeletedInfo)
{
  return DeletedMessageInfo::Create(aMessageIds,
                                    aMsgCount,
                                    aThreadIds,
                                    aThreadCount,
                                    aDeletedInfo);
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
