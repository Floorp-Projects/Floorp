/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMContactChangeEvent_h__
#define nsDOMContactChangeEvent_h__

#include "nsIDOMContactManager.h"
#include "nsDOMEvent.h"

class nsDOMMozContactChangeEvent : public nsDOMEvent,
                                   public nsIDOMMozContactChangeEvent
{
public:
  nsDOMMozContactChangeEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent) {}
                     
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMMozContactChangeEvent, nsDOMEvent)
  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  NS_DECL_NSIDOMMOZCONTACTCHANGEEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);
private:
  nsString mContactID;
  nsString mReason;
};

#endif // nsDOMContactChangeEvent_h__
