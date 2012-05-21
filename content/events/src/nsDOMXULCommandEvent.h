/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This class implements a XUL "command" event.  See nsIDOMXULCommandEvent.idl

#ifndef nsDOMXULCommandEvent_h_
#define nsDOMXULCommandEvent_h_

#include "nsDOMUIEvent.h"
#include "nsIDOMXULCommandEvent.h"

class nsDOMXULCommandEvent : public nsDOMUIEvent,
                             public nsIDOMXULCommandEvent
{
public:
  nsDOMXULCommandEvent(nsPresContext* aPresContext, nsInputEvent* aEvent);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMXULCommandEvent, nsDOMUIEvent)
  NS_DECL_NSIDOMXULCOMMANDEVENT

  // Forward our inherited virtual methods to the base class
  NS_FORWARD_TO_NSDOMUIEVENT

protected:
  // Convenience accessor for the event
  nsInputEvent* Event() {
    return static_cast<nsInputEvent*>(mEvent);
  }

  nsCOMPtr<nsIDOMEvent> mSourceEvent;
};

#endif  // nsDOMXULCommandEvent_h_
