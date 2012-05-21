/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMPopStateEvent_h__
#define nsDOMPopStateEvent_h__

class nsEvent;

#include "nsIDOMPopStateEvent.h"
#include "nsDOMEvent.h"
#include "nsIVariant.h"
#include "nsCycleCollectionParticipant.h"

class nsDOMPopStateEvent : public nsDOMEvent,
                           public nsIDOMPopStateEvent
{
public:
  nsDOMPopStateEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent)  // state
  {
  }

  virtual ~nsDOMPopStateEvent();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMPopStateEvent, nsDOMEvent)

  NS_DECL_NSIDOMPOPSTATEEVENT

  NS_FORWARD_TO_NSDOMEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);
protected:
  nsCOMPtr<nsIVariant> mState;
};

#endif // nsDOMPopStateEvent_h__
