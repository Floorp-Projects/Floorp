/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMHashChangeEvent_h__
#define nsDOMHashChangeEvent_h__

class nsEvent;

#include "nsIDOMHashChangeEvent.h"
#include "nsDOMEvent.h"
#include "nsIVariant.h"
#include "nsCycleCollectionParticipant.h"

class nsDOMHashChangeEvent : public nsDOMEvent,
                             public nsIDOMHashChangeEvent
{
public:
  nsDOMHashChangeEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent)
  {
  }

  virtual ~nsDOMHashChangeEvent();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMHASHCHANGEEVENT

  NS_FORWARD_TO_NSDOMEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);
protected:
  nsString mOldURL;
  nsString mNewURL;
};

#endif // nsDOMHashChangeEvent_h__
