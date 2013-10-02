/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMDragEvent.h"
#include "nsContentUtils.h"
#include "nsIDOMDataTransfer.h"
#include "prtime.h"
#include "mozilla/MouseEvents.h"

using namespace mozilla;

nsDOMDragEvent::nsDOMDragEvent(mozilla::dom::EventTarget* aOwner,
                               nsPresContext* aPresContext,
                               WidgetInputEvent* aEvent)
  : nsDOMMouseEvent(aOwner, aPresContext, aEvent ? aEvent :
                    new WidgetDragEvent(false, 0, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<WidgetMouseEvent*>(mEvent)->inputSource =
      nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }
}

nsDOMDragEvent::~nsDOMDragEvent()
{
  if (mEventIsInternal) {
    if (mEvent->eventStructType == NS_DRAG_EVENT)
      delete static_cast<WidgetDragEvent*>(mEvent);
    mEvent = nullptr;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMDragEvent, nsDOMMouseEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMDragEvent, nsDOMMouseEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMDragEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDragEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

NS_IMETHODIMP
nsDOMDragEvent::InitDragEvent(const nsAString & aType,
                              bool aCanBubble, bool aCancelable,
                              nsIDOMWindow* aView, int32_t aDetail,
                              int32_t aScreenX, int32_t aScreenY,
                              int32_t aClientX, int32_t aClientY, 
                              bool aCtrlKey, bool aAltKey, bool aShiftKey,
                              bool aMetaKey, uint16_t aButton,
                              nsIDOMEventTarget *aRelatedTarget,
                              nsIDOMDataTransfer* aDataTransfer)
{
  nsresult rv = nsDOMMouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                  aView, aDetail, aScreenX, aScreenY, aClientX, aClientY,
                  aCtrlKey, aAltKey, aShiftKey, aMetaKey, aButton,
                  aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mEventIsInternal && mEvent) {
    WidgetDragEvent* dragEvent = static_cast<WidgetDragEvent*>(mEvent);
    dragEvent->dataTransfer = aDataTransfer;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDragEvent::GetDataTransfer(nsIDOMDataTransfer** aDataTransfer)
{
  NS_IF_ADDREF(*aDataTransfer = GetDataTransfer());
  return NS_OK;
}

nsIDOMDataTransfer*
nsDOMDragEvent::GetDataTransfer()
{
  // the dataTransfer field of the event caches the DataTransfer associated
  // with the drag. It is initialized when an attempt is made to retrieve it
  // rather that when the event is created to avoid duplicating the data when
  // no listener ever uses it.
  if (!mEvent || mEvent->eventStructType != NS_DRAG_EVENT) {
    NS_WARNING("Tried to get dataTransfer from non-drag event!");
    return nullptr;
  }

  WidgetDragEvent* dragEvent = static_cast<WidgetDragEvent*>(mEvent);
  // for synthetic events, just use the supplied data transfer object even if null
  if (!mEventIsInternal) {
    nsresult rv = nsContentUtils::SetDataTransferInEvent(dragEvent);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  return dragEvent->dataTransfer;
}

nsresult NS_NewDOMDragEvent(nsIDOMEvent** aInstancePtrResult,
                            mozilla::dom::EventTarget* aOwner,
                            nsPresContext* aPresContext,
                            WidgetDragEvent* aEvent) 
{
  nsDOMDragEvent* event = new nsDOMDragEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(event, aInstancePtrResult);
}
