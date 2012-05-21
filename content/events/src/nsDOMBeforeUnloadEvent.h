/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMBeforeUnloadEvent_h__
#define nsDOMBeforeUnloadEvent_h__

#include "nsIDOMBeforeUnloadEvent.h"
#include "nsDOMEvent.h"

class nsDOMBeforeUnloadEvent : public nsDOMEvent,
                               public nsIDOMBeforeUnloadEvent
{
public:
  nsDOMBeforeUnloadEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent) {}

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMBeforeUnloadEvent Interface
  NS_DECL_NSIDOMBEFOREUNLOADEVENT
protected:
  nsString mText;
};

#endif // nsDOMBeforeUnloadEvent_h__
