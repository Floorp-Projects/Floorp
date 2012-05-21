/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDragEvent_h__
#define nsDOMDragEvent_h__

#include "nsIDOMDragEvent.h"
#include "nsDOMMouseEvent.h"
#include "nsIDOMDataTransfer.h"

class nsIContent;
class nsEvent;

class nsDOMDragEvent : public nsDOMMouseEvent,
                       public nsIDOMDragEvent
{
public:
  nsDOMDragEvent(nsPresContext* aPresContext, nsInputEvent* aEvent);
  virtual ~nsDOMDragEvent();

  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_NSIDOMDRAGEVENT
  
  NS_FORWARD_TO_NSDOMMOUSEEVENT
};

nsresult NS_NewDOMDragEvent(nsIDOMEvent** aInstancePtrResult,
                            nsPresContext* aPresContext,
                            nsDragEvent* aEvent);

#endif // nsDOMDragEvent_h__
