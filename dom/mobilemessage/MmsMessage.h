/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MmsMessage_h
#define mozilla_dom_mobilemessage_MmsMessage_h

#include "nsIDOMMozMmsMessage.h"
#include "nsString.h"
#include "mozilla/dom/mobilemessage/Types.h"
#include "mozilla/dom/MozMmsMessageBinding.h"
#include "mozilla/dom/MozMobileMessageManagerBinding.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {

class File;

namespace mobilemessage {
class MmsMessageData;
} // namespace mobilemessage

class ContentParent;

class MmsMessage final : public nsIDOMMozMmsMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZMMSMESSAGE

  // If this is changed, change the WebIDL dictionary as well.
  struct Attachment final
  {
    nsRefPtr<File> content;
    nsString id;
    nsString location;

    explicit Attachment(const MmsAttachment& aAttachment) :
      content(aAttachment.mContent),
      id(aAttachment.mId),
      location(aAttachment.mLocation)
    {}
  };

  MmsMessage(int32_t aId,
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
             const nsTArray<Attachment>& aAttachments,
             uint64_t aExpiryDate,
             bool aReadReportRequested);

  explicit MmsMessage(const mobilemessage::MmsMessageData& aData);

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
                         nsIDOMMozMmsMessage** aMessage);

  bool GetData(ContentParent* aParent,
               mobilemessage::MmsMessageData& aData);

private:

  ~MmsMessage() {}

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
  nsTArray<Attachment> mAttachments;
  uint64_t mExpiryDate;
  bool mReadReportRequested;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_MmsMessage_h
