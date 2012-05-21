/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMCustomEvent_h__
#define nsDOMCustomEvent_h__

#include "nsIDOMCustomEvent.h"
#include "nsDOMEvent.h"
#include "nsCycleCollectionParticipant.h"

class nsDOMCustomEvent : public nsDOMEvent,
                         public nsIDOMCustomEvent
{
public:
  nsDOMCustomEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent)
  {
  }
                     
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMCustomEvent, nsDOMEvent)

  NS_DECL_NSIDOMCUSTOMEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);
private:
  nsCOMPtr<nsIVariant> mDetail;
};

#endif // nsDOMCustomEvent_h__
