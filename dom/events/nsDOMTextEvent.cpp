/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMTextEvent.h"
#include "nsPrivateTextRange.h"
#include "prtime.h"
#include "mozilla/TextEvents.h"

using namespace mozilla;
using namespace mozilla::dom;

nsDOMTextEvent::nsDOMTextEvent(mozilla::dom::EventTarget* aOwner,
                               nsPresContext* aPresContext,
                               WidgetTextEvent* aEvent)
  : UIEvent(aOwner, aPresContext,
            aEvent ? aEvent : new WidgetTextEvent(false, 0, nullptr))
{
  NS_ASSERTION(mEvent->eventStructType == NS_TEXT_EVENT, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMTextEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMTextEvent, UIEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMTextEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

nsresult NS_NewDOMTextEvent(nsIDOMEvent** aInstancePtrResult,
                            mozilla::dom::EventTarget* aOwner,
                            nsPresContext* aPresContext,
                            WidgetTextEvent* aEvent)
{
  nsDOMTextEvent* it = new nsDOMTextEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
