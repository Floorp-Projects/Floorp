/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MmsMessage_h
#define mozilla_dom_mobilemessage_MmsMessage_h

#include "nsIDOMMozMmsMessage.h"
#include "nsString.h"
#include "mozilla/dom/mobilemessage/Types.h"
#include "mozilla/Attributes.h"
#include "DictionaryHelpers.h"

namespace mozilla {
namespace dom {

namespace mobilemessage {
class MmsMessageData;
} // namespace mobilemessage

class ContentParent;

class MmsMessage MOZ_FINAL : public nsIDOMMozMmsMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZMMSMESSAGE

  MmsMessage(int32_t                               aId,
             uint64_t                              aThreadId,
             const nsAString&                      aIccId,
             mobilemessage::DeliveryState          aDelivery,
             const nsTArray<idl::MmsDeliveryInfo>& aDeliveryInfo,
             const nsAString&                      aSender,
             const nsTArray<nsString>&             aReceivers,
             uint64_t                              aTimestamp,
             bool                                  aRead,
             const nsAString&                      aSubject,
             const nsAString&                      aSmil,
             const nsTArray<idl::MmsAttachment>&   aAttachments,
             uint64_t                              aExpiryDate,
             bool                                  aReadReportRequested);

  MmsMessage(const mobilemessage::MmsMessageData& aData);

  static nsresult Create(int32_t               aId,
                         uint64_t              aThreadId,
                         const nsAString&      aIccId,
                         const nsAString&      aDelivery,
                         const JS::Value&      aDeliveryInfo,
                         const nsAString&      aSender,
                         const JS::Value&      aReceivers,
                         const JS::Value&      aTimestamp,
                         bool                  aRead,
                         const nsAString&      aSubject,
                         const nsAString&      aSmil,
                         const JS::Value&      aAttachments,
                         const JS::Value&      aExpiryDate,
                         bool                  aReadReportRequested,
                         JSContext*            aCx,
                         nsIDOMMozMmsMessage** aMessage);

  bool GetData(ContentParent* aParent,
               mobilemessage::MmsMessageData& aData);

private:

  int32_t                        mId;
  uint64_t                       mThreadId;
  nsString                       mIccId;
  mobilemessage::DeliveryState   mDelivery;
  nsTArray<idl::MmsDeliveryInfo> mDeliveryInfo;
  nsString                       mSender;
  nsTArray<nsString>             mReceivers;
  uint64_t                       mTimestamp;
  bool                           mRead;
  nsString                       mSubject;
  nsString                       mSmil;
  nsTArray<idl::MmsAttachment>   mAttachments;
  uint64_t                       mExpiryDate;
  bool                           mReadReportRequested;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MmsMessage_h
