/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_DOMTIMEEVENT_H_
#define NS_DOMTIMEEVENT_H_

#include "nsIDOMTimeEvent.h"
#include "nsDOMEvent.h"

class nsDOMTimeEvent : public nsDOMEvent,
                       public nsIDOMTimeEvent
{
public:
  nsDOMTimeEvent(nsPresContext* aPresContext, nsEvent* aEvent);
                     
  // nsISupports interface:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMTimeEvent, nsDOMEvent)

  // nsIDOMTimeEvent interface:
  NS_DECL_NSIDOMTIMEEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

private:
  nsCOMPtr<nsIDOMWindow> mView;
  PRInt32 mDetail;
};

#endif // NS_DOMTIMEEVENT_H_
