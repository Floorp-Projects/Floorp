/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsMessage_h
#define mozilla_dom_sms_SmsMessage_h

#include "mozilla/dom/sms/PSms.h"
#include "nsIDOMSmsMessage.h"
#include "nsString.h"
#include "jspubtd.h"
#include "Types.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace sms {

class SmsMessage MOZ_FINAL : public nsIDOMMozSmsMessage
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMMOZSMSMESSAGE

  SmsMessage(PRInt32 aId, DeliveryState aDelivery, const nsString& aSender,
             const nsString& aReceiver, const nsString& aBody,
             PRUint64 aTimestamp, bool aRead);
  SmsMessage(const SmsMessageData& aData);

  static nsresult Create(PRInt32 aId,
                         const nsAString& aDelivery,
                         const nsAString& aSender,
                         const nsAString& aReceiver,
                         const nsAString& aBody,
                         const JS::Value& aTimestamp,
                         const bool aRead,
                         JSContext* aCx,
                         nsIDOMMozSmsMessage** aMessage);
  const SmsMessageData& GetData() const;

private:
  // Don't try to use the default constructor.
  SmsMessage();

  SmsMessageData mData;
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsMessage_h
