/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMouseScrollEvent_h__
#define nsDOMMouseScrollEvent_h__

#include "nsIDOMMouseScrollEvent.h"
#include "nsDOMMouseEvent.h"

class nsDOMMouseScrollEvent : public nsDOMMouseEvent,
                              public nsIDOMMouseScrollEvent
{
public:
  nsDOMMouseScrollEvent(nsPresContext* aPresContext, nsInputEvent* aEvent);
  virtual ~nsDOMMouseScrollEvent();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMMouseScrollEvent Interface
  NS_DECL_NSIDOMMOUSESCROLLEVENT
  
  // Forward to base class
  NS_FORWARD_TO_NSDOMMOUSEEVENT
};

#endif // nsDOMMouseScrollEvent_h__
