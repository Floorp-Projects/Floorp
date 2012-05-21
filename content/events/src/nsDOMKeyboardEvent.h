/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMKeyboardEvent_h__
#define nsDOMKeyboardEvent_h__

#include "nsIDOMKeyEvent.h"
#include "nsDOMUIEvent.h"

class nsDOMKeyboardEvent : public nsDOMUIEvent,
                           public nsIDOMKeyEvent
{
public:
  nsDOMKeyboardEvent(nsPresContext* aPresContext, nsKeyEvent* aEvent);
  virtual ~nsDOMKeyboardEvent();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMKeyEvent Interface
  NS_DECL_NSIDOMKEYEVENT

  // Forward to base class
  NS_FORWARD_TO_NSDOMUIEVENT

protected:
  // Specific implementation for a keyboard event.
  virtual nsresult Which(PRUint32* aWhich);
};


#endif // nsDOMKeyboardEvent_h__
