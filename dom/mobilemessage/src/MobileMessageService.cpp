/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsMessage.h"
#include "MmsMessage.h"
#include "MobileMessageThread.h"
#include "MobileMessageService.h"
#include "SmsSegmentInfo.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS1(MobileMessageService, nsIMobileMessageService)

/* static */ StaticRefPtr<MobileMessageService> MobileMessageService::sSingleton;

/* static */ already_AddRefed<MobileMessageService>
MobileMessageService::GetInstance()
{
  if (!sSingleton) {
    sSingleton = new MobileMessageService();
    ClearOnShutdown(&sSingleton);
  }

  nsRefPtr<MobileMessageService> service = sSingleton.get();
  return service.forget();
}

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
                                       JS::Handle<JS::Value> aTimestamp,
                                       JS::Handle<JS::Value> aSentTimestamp,
                                       JS::Handle<JS::Value> aDeliveryTimestamp,
                                       const bool aRead,
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
MobileMessageService::CreateMmsMessage(int32_t               aId,
                                       uint64_t              aThreadId,
                                       const nsAString&      aIccId,
                                       const nsAString&      aDelivery,
                                       JS::Handle<JS::Value> aDeliveryInfo,
                                       const nsAString&      aSender,
                                       JS::Handle<JS::Value> aReceivers,
                                       JS::Handle<JS::Value> aTimestamp,
                                       JS::Handle<JS::Value> aSentTimestamp,
                                       bool                  aRead,
                                       const nsAString&      aSubject,
                                       const nsAString&      aSmil,
                                       JS::Handle<JS::Value> aAttachments,
                                       JS::Handle<JS::Value> aExpiryDate,
                                       bool                  aReadReportRequested,
                                       JSContext*            aCx,
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
MobileMessageService::CreateSmsSegmentInfo(int32_t aSegments,
                                           int32_t aCharsPerSegment,
                                           int32_t aCharsAvailableInLastSegment,
                                           nsIDOMMozSmsSegmentInfo** aSegmentInfo)
{
  nsCOMPtr<nsIDOMMozSmsSegmentInfo> info =
      new SmsSegmentInfo(aSegments, aCharsPerSegment, aCharsAvailableInLastSegment);
  info.forget(aSegmentInfo);
  return NS_OK;
}

NS_IMETHODIMP
MobileMessageService::CreateThread(uint64_t aId,
                                   JS::Handle<JS::Value> aParticipants,
                                   JS::Handle<JS::Value> aTimestamp,
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

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
