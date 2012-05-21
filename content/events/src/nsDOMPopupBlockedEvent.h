/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMPopupBlockedEvent_h__
#define nsDOMPopupBlockedEvent_h__

#include "nsIDOMPopupBlockedEvent.h"
#include "nsDOMEvent.h"
#include "nsIURI.h"

class nsDOMPopupBlockedEvent : public nsDOMEvent,
                               public nsIDOMPopupBlockedEvent
{
public:

  nsDOMPopupBlockedEvent(nsPresContext* aPresContext, nsEvent* aEvent)
  : nsDOMEvent(aPresContext, aEvent) {}

  NS_DECL_ISUPPORTS_INHERITED

  // Forward to nsDOMEvent
  NS_FORWARD_TO_NSDOMEVENT

  // nsIDOMPopupBlockedEvent Interface
  NS_DECL_NSIDOMPOPUPBLOCKEDEVENT
protected:
  nsWeakPtr        mRequestingWindow;
  nsCOMPtr<nsIURI> mPopupWindowURI;
  nsString         mPopupWindowFeatures;
  nsString         mPopupWindowName;
};

#endif // nsDOMPopupBlockedEvent_h__
