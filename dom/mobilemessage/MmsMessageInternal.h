/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MmsMessageInternal_h
#define mozilla_dom_mobilemessage_MmsMessageInternal_h

#include "nsIMmsMessage.h"
#include "nsString.h"
#include "mozilla/dom/mobilemessage/Types.h"
#include "mozilla/dom/MmsMessageBinding.h"
#include "mozilla/dom/MozMobileMessageManagerBinding.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

class ContentParent;
class Blob;
struct MmsAttachment;
class MmsMessage;

namespace mobilemessage {

class MmsMessageData;

class MmsMessageInternal final : public nsIMmsMessage
{
  // This allows the MmsMessage class to access jsval data members
  // like |deliveryInfo|, |receivers|, and |attachments| without JS API.
  friend class mozilla::dom::MmsMessage;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(MmsMessageInternal)
  NS_DECL_NSIMMSMESSAGE

  MmsMessageInternal(int32_t aId,
                     uint64_t aThreadId,
                     const nsAString& aIccId,
                     mobilemessage::DeliveryState aDelivery,
                     const nsTArray<MmsDeliveryInfo>& aDeliveryInfo,
                     const nsAString& aSender,
                     const nsTArray<nsString>& aReceivers,
                     uint64_t aTimestamp,
                     uint64_t aSentTimestamp,
                     bool aRead,
                     const nsAString& aSubject,
                     const nsAString& aSmil,
                     const nsTArray<MmsAttachment>& aAttachments,
                     uint64_t aExpiryDate,
                     bool aReadReportRequested);

  explicit MmsMessageInternal(const MmsMessageData& aData);

  static nsresult Create(int32_t aId,
                         uint64_t aThreadId,
                         const nsAString& aIccId,
                         const nsAString& aDelivery,
                         const JS::Value& aDeliveryInfo,
                         const nsAString& aSender,
                         const JS::Value& aReceivers,
                         uint64_t aTimestamp,
                         uint64_t aSentTimestamp,
                         bool aRead,
                         const nsAString& aSubject,
                         const nsAString& aSmil,
                         const JS::Value& aAttachments,
                         uint64_t aExpiryDate,
                         bool aReadReportRequested,
                         JSContext* aCx,
                         nsIMmsMessage** aMessage);

  bool GetData(ContentParent* aParent,
               MmsMessageData& aData);

private:

  ~MmsMessageInternal() {}

  int32_t mId;
  uint64_t mThreadId;
  nsString mIccId;
  mobilemessage::DeliveryState mDelivery;
  nsTArray<MmsDeliveryInfo> mDeliveryInfo;
  nsString mSender;
  nsTArray<nsString> mReceivers;
  uint64_t mTimestamp;
  uint64_t mSentTimestamp;
  bool mRead;
  nsString mSubject;
  nsString mSmil;
  nsTArray<MmsAttachment> mAttachments;
  uint64_t mExpiryDate;
  bool mReadReportRequested;
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MmsMessageInternal_h
