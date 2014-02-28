/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/DragEvent.h"
#include "mozilla/MouseEvents.h"
#include "nsContentUtils.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

DragEvent::DragEvent(EventTarget* aOwner,
                     nsPresContext* aPresContext,
                     WidgetDragEvent* aEvent)
  : MouseEvent(aOwner, aPresContext,
               aEvent ? aEvent : new WidgetDragEvent(false, 0, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    mEvent->AsMouseEvent()->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }
}

NS_IMPL_ADDREF_INHERITED(DragEvent, MouseEvent)
NS_IMPL_RELEASE_INHERITED(DragEvent, MouseEvent)

NS_INTERFACE_MAP_BEGIN(DragEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDragEvent)
NS_INTERFACE_MAP_END_INHERITING(MouseEvent)

void
DragEvent::InitDragEvent(const nsAString& aType,
                         bool aCanBubble,
                         bool aCancelable,
                         nsIDOMWindow* aView,
                         int32_t aDetail,
                         int32_t aScreenX,
                         int32_t aScreenY,
                         int32_t aClientX,
                         int32_t aClientY,
                         bool aCtrlKey,
                         bool aAltKey,
                         bool aShiftKey,
                         bool aMetaKey,
                         uint16_t aButton,
                         EventTarget* aRelatedTarget,
                         DataTransfer* aDataTransfer,
                         ErrorResult& aError)
{
  aError =
    MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                               aView, aDetail, aScreenX, aScreenY,
                               aClientX, aClientY, aCtrlKey, aAltKey,
                               aShiftKey, aMetaKey, aButton, aRelatedTarget);
  if (aError.Failed()) {
    return;
  }

  if (mEventIsInternal && mEvent) {
    mEvent->AsDragEvent()->dataTransfer = aDataTransfer;
  }
}

NS_IMETHODIMP
DragEvent::InitDragEvent(const nsAString& aType,
                         bool aCanBubble,
                         bool aCancelable,
                         nsIDOMWindow* aView,
                         int32_t aDetail,
                         int32_t aScreenX,
                         int32_t aScreenY,
                         int32_t aClientX,
                         int32_t aClientY,
                         bool aCtrlKey,
                         bool aAltKey,
                         bool aShiftKey,
                         bool aMetaKey,
                         uint16_t aButton,
                         nsIDOMEventTarget* aRelatedTarget,
                         nsIDOMDataTransfer* aDataTransfer)
{
  nsCOMPtr<DataTransfer> dataTransfer = do_QueryInterface(aDataTransfer);

  nsresult rv =
    MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                               aScreenX, aScreenY, aClientX, aClientY,
                               aCtrlKey, aAltKey, aShiftKey, aMetaKey, aButton,
                               aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mEventIsInternal && mEvent) {
    mEvent->AsDragEvent()->dataTransfer = dataTransfer;
  }

  return NS_OK;
}

NS_IMETHODIMP
DragEvent::GetDataTransfer(nsIDOMDataTransfer** aDataTransfer)
{
  NS_IF_ADDREF(*aDataTransfer = GetDataTransfer());
  return NS_OK;
}

DataTransfer*
DragEvent::GetDataTransfer()
{
  // the dataTransfer field of the event caches the DataTransfer associated
  // with the drag. It is initialized when an attempt is made to retrieve it
  // rather that when the event is created to avoid duplicating the data when
  // no listener ever uses it.
  if (!mEvent || mEvent->eventStructType != NS_DRAG_EVENT) {
    NS_WARNING("Tried to get dataTransfer from non-drag event!");
    return nullptr;
  }

  WidgetDragEvent* dragEvent = mEvent->AsDragEvent();
  // for synthetic events, just use the supplied data transfer object even if null
  if (!mEventIsInternal) {
    nsresult rv = nsContentUtils::SetDataTransferInEvent(dragEvent);
    NS_ENSURE_SUCCESS(rv, nullptr);
  }

  return dragEvent->dataTransfer;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMDragEvent(nsIDOMEvent** aInstancePtrResult,
                   EventTarget* aOwner,
                   nsPresContext* aPresContext,
                   WidgetDragEvent* aEvent) 
{
  DragEvent* event = new DragEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(event, aInstancePtrResult);
}
