/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_SVGEVENT_H__
#define __NS_SVGEVENT_H__

#include "nsDOMEvent.h"
#include "nsIDOMSVGEvent.h"

class nsEvent;
class nsPresContext;

class nsDOMSVGEvent : public nsDOMEvent,
                      public nsIDOMSVGEvent
{
public:
  nsDOMSVGEvent(nsPresContext* aPresContext, nsEvent* aEvent);

  // nsISupports interface:
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMSVGEvent interface:
  NS_DECL_NSIDOMSVGEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT
};

#endif // __NS_SVGEVENT_H__
