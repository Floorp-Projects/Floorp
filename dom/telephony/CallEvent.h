/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_callevent_h__
#define mozilla_dom_telephony_callevent_h__

#include "TelephonyCommon.h"

#include "nsIDOMCallEvent.h"
#include "nsIDOMEventTarget.h"

#include "nsDOMEvent.h"

BEGIN_TELEPHONY_NAMESPACE

class CallEvent : public nsDOMEvent,
                  public nsIDOMCallEvent
{
  nsRefPtr<TelephonyCall> mCall;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_TO_NSDOMEVENT
  NS_DECL_NSIDOMCALLEVENT
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(CallEvent, nsDOMEvent)

  static already_AddRefed<CallEvent>
  Create(TelephonyCall* aCall);

  nsresult
  Dispatch(nsIDOMEventTarget* aTarget, const nsAString& aEventType)
  {
    NS_ASSERTION(aTarget, "Null pointer!");
    NS_ASSERTION(!aEventType.IsEmpty(), "Empty event type!");

    nsresult rv = InitEvent(aEventType, false, false);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = SetTrusted(true);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIDOMEvent* thisEvent =
      static_cast<nsDOMEvent*>(const_cast<CallEvent*>(this));

    bool dummy;
    rv = aTarget->DispatchEvent(thisEvent, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

private:
  CallEvent()
  : nsDOMEvent(nullptr, nullptr)
  { }

  ~CallEvent()
  { }
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_callevent_h__
