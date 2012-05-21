/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_sms_SmsEvent_h
#define mozilla_dom_sms_SmsEvent_h

#include "nsIDOMSmsEvent.h"
#include "nsDOMEvent.h"

class nsIDOMMozSmsMessage;

namespace mozilla {
namespace dom {
namespace sms {

class SmsEvent : public nsIDOMMozSmsEvent
               , public nsDOMEvent
{
public:
  SmsEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent)
    , mMessage(nsnull)
  {}

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMMOZSMSEVENT

  NS_FORWARD_TO_NSDOMEVENT

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SmsEvent, nsDOMEvent)

  nsresult Init(const nsAString & aEventTypeArg, bool aCanBubbleArg,
                bool aCancelableArg, nsIDOMMozSmsMessage* aMessage);

private:
  nsCOMPtr<nsIDOMMozSmsMessage> mMessage;
};

} // namespace sms
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_sms_SmsEvent_h
