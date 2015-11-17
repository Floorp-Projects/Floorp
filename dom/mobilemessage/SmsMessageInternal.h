/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_SmsMessageInternal_h
#define mozilla_dom_mobilemessage_SmsMessageInternal_h

#include "mozilla/dom/mobilemessage/SmsTypes.h"
#include "nsISmsMessage.h"
#include "nsString.h"
#include "mozilla/dom/mobilemessage/Types.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

class SmsMessageData;

class SmsMessageInternal final : public nsISmsMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISMSMESSAGE

  SmsMessageInternal(int32_t aId,
                     uint64_t aThreadId,
                     const nsString& aIccId,
                     mobilemessage::DeliveryState aDelivery,
                     mobilemessage::DeliveryStatus aDeliveryStatus,
                     const nsString& aSender,
                     const nsString& aReceiver,
                     const nsString& aBody,
                     mobilemessage::MessageClass aMessageClass,
                     uint64_t aTimestamp,
                     uint64_t aSentTimestamp,
                     uint64_t aDeliveryTimestamp,
                     bool aRead);

  explicit SmsMessageInternal(const SmsMessageData& aData);

  static nsresult Create(int32_t aId,
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
                         nsISmsMessage** aMessage);
  const SmsMessageData& GetData() const;

private:
  ~SmsMessageInternal() {}

  // Don't try to use the default constructor.
  SmsMessageInternal();

  SmsMessageData mData;
};

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_mobilemessage_SmsMessageInternal_h
