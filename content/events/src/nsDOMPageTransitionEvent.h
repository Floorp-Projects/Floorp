/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMPageTransitionEvent_h__
#define nsDOMPageTransitionEvent_h__

#include "nsIDOMPageTransitionEvent.h"
#include "nsDOMEvent.h"

class nsDOMPageTransitionEvent : public nsDOMEvent,
                                 public nsIDOMPageTransitionEvent
{
public:
  nsDOMPageTransitionEvent(nsPresContext* aPresContext, nsEvent* aEvent) :
  nsDOMEvent(aPresContext, aEvent), mPersisted(false) {}

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMPAGETRANSITIONEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);
protected:
  bool mPersisted;
};

#endif // nsDOMPageTransitionEvent_h__
