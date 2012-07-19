/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_VoicemailEvent_h__
#define mozilla_dom_telephony_VoicemailEvent_h__

#include "TelephonyCommon.h"

#include "nsIDOMVoicemailEvent.h"
#include "nsDOMEvent.h"

class nsIDOMMozVoicemailStatus;

BEGIN_TELEPHONY_NAMESPACE

class VoicemailEvent : public nsIDOMMozVoicemailEvent,
                       public nsDOMEvent
{
public:
  VoicemailEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent) { }

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMMOZVOICEMAILEVENT

  NS_FORWARD_TO_NSDOMEVENT

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(VoicemailEvent, nsDOMEvent)

  nsresult InitVoicemailEvent(const nsAString& aEventTypeArg,
                              bool aCanBubbleArg, bool aCancelableArg,
                              nsIDOMMozVoicemailStatus* aStatus);

private:
  nsCOMPtr<nsIDOMMozVoicemailStatus> mStatus;
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_VoicemailEvent_h__
