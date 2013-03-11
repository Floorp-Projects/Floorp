/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMMutationEvent_h__
#define nsDOMMutationEvent_h__

#include "nsIDOMMutationEvent.h"
#include "nsDOMEvent.h"

class nsDOMMutationEvent : public nsDOMEvent,
                           public nsIDOMMutationEvent
{
public:
  nsDOMMutationEvent(mozilla::dom::EventTarget* aOwner,
                     nsPresContext* aPresContext, nsMutationEvent* aEvent);

  virtual ~nsDOMMutationEvent();
                     
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMMUTATIONEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT
};

#endif // nsDOMMutationEvent_h__
