/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMUserProximityEvent_h__
#define nsDOMUserProximityEvent_h__

#include "nsIDOMUserProximityEvent.h"
#include "nsDOMEvent.h"

class nsDOMUserProximityEvent
 : public nsDOMEvent
 , public nsIDOMUserProximityEvent
{
public:

  nsDOMUserProximityEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent),
    mNear(false) {}

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMUserProximityEvent Interface
  NS_DECL_NSIDOMUSERPROXIMITYEVENT

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx,
                                jsval* aVal);
protected:
  bool mNear;
};

#endif
