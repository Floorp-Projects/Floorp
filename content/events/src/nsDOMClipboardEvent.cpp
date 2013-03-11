/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClipboardEvent.h"
#include "nsContentUtils.h"
#include "nsClientRect.h"
#include "nsDOMDataTransfer.h"
#include "nsDOMClassInfoID.h"

nsDOMClipboardEvent::nsDOMClipboardEvent(mozilla::dom::EventTarget* aOwner,
                                         nsPresContext* aPresContext,
                                         nsClipboardEvent* aEvent)
  : nsDOMEvent(aOwner, aPresContext, aEvent ? aEvent :
               new nsClipboardEvent(false, 0))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

nsDOMClipboardEvent::~nsDOMClipboardEvent()
{
  if (mEventIsInternal && mEvent->eventStructType == NS_CLIPBOARD_EVENT) {
    delete static_cast<nsClipboardEvent*>(mEvent);
    mEvent = nullptr;
  }
}

DOMCI_DATA(ClipboardEvent, nsDOMClipboardEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMClipboardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMClipboardEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ClipboardEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMClipboardEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMClipboardEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMClipboardEvent::GetClipboardData(nsIDOMDataTransfer** aClipboardData)
{
  nsClipboardEvent* event = static_cast<nsClipboardEvent*>(mEvent);

  if (!event->clipboardData) {
    if (mEventIsInternal) {
      event->clipboardData = new nsDOMDataTransfer(NS_COPY, false);
    } else {
      event->clipboardData =
        new nsDOMDataTransfer(event->message, event->message == NS_PASTE);
    }
  }

  NS_IF_ADDREF(*aClipboardData = event->clipboardData);
  return NS_OK;
}

nsresult NS_NewDOMClipboardEvent(nsIDOMEvent** aInstancePtrResult,
                                 mozilla::dom::EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 nsClipboardEvent *aEvent)
{
  nsDOMClipboardEvent* it =
    new nsDOMClipboardEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
