/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMProgressEvent_h__
#define nsDOMProgressEvent_h__

#include "nsIDOMProgressEvent.h"
#include "nsDOMEvent.h"

/**
 * Implements the ProgressEvent event, used for progress events from the media
 * elements.

 * See http://www.whatwg.org/specs/web-apps/current-work/#progress0 for
 * further details.
 */
class nsDOMProgressEvent : public nsDOMEvent,
                           public nsIDOMProgressEvent
{
public:
  nsDOMProgressEvent(nsPresContext* aPresContext, nsEvent* aEvent)
    : nsDOMEvent(aPresContext, aEvent)
  {
  }
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIDOMPROGRESSEVENT
    
  // Forward to base class
  NS_FORWARD_TO_NSDOMEVENT

private:
  bool    mLengthComputable;
  PRUint64 mLoaded;
  PRUint64 mTotal;
};

#endif // nsDOMProgressEvent_h__
