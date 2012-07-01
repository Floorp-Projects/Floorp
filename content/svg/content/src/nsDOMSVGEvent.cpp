/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "nsDOMSVGEvent.h"

//----------------------------------------------------------------------
// Implementation

nsDOMSVGEvent::nsDOMSVGEvent(nsPresContext* aPresContext,
                             nsEvent* aEvent)
  : nsDOMEvent(aPresContext,
               aEvent ? aEvent : new nsEvent(false, 0))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->eventStructType = NS_SVG_EVENT;
    mEvent->time = PR_Now();
  }

  mEvent->flags |= NS_EVENT_FLAG_CANT_CANCEL;
  if (mEvent->message == NS_SVG_LOAD || mEvent->message == NS_SVG_UNLOAD) {
    mEvent->flags |= NS_EVENT_FLAG_CANT_BUBBLE;
  }
}

//----------------------------------------------------------------------
// nsISupports methods:

NS_IMPL_ADDREF_INHERITED(nsDOMSVGEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMSVGEvent, nsDOMEvent)

DOMCI_DATA(SVGEvent, nsDOMSVGEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMSVGEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSVGEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(SVGEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)


////////////////////////////////////////////////////////////////////////
// Exported creation functions:

nsresult
NS_NewDOMSVGEvent(nsIDOMEvent** aInstancePtrResult,
                  nsPresContext* aPresContext,
                  nsEvent *aEvent)
{
  nsDOMSVGEvent* it = new nsDOMSVGEvent(aPresContext, aEvent);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  return CallQueryInterface(it, aInstancePtrResult);
}
