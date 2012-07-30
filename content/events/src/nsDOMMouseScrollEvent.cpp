/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMouseScrollEvent.h"
#include "nsDOMClassInfoID.h"

nsDOMMouseScrollEvent::nsDOMMouseScrollEvent(nsPresContext* aPresContext,
                                             nsInputEvent* aEvent)
  : nsDOMMouseEvent(aPresContext, aEvent ? aEvent :
                                  new nsMouseScrollEvent(false, 0, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<nsMouseEvent*>(mEvent)->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }

  if(mEvent->eventStructType == NS_MOUSE_SCROLL_EVENT) {
    nsMouseScrollEvent* mouseEvent = static_cast<nsMouseScrollEvent*>(mEvent);
    mDetail = mouseEvent->delta;
  }
}

nsDOMMouseScrollEvent::~nsDOMMouseScrollEvent()
{
  if (mEventIsInternal && mEvent) {
    switch (mEvent->eventStructType)
    {
      case NS_MOUSE_SCROLL_EVENT:
        delete static_cast<nsMouseScrollEvent*>(mEvent);
        break;
      default:
        delete mEvent;
        break;
    }
    mEvent = nullptr;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMMouseScrollEvent, nsDOMMouseEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMouseScrollEvent, nsDOMMouseEvent)

DOMCI_DATA(MouseScrollEvent, nsDOMMouseScrollEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMouseScrollEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseScrollEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MouseScrollEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

NS_IMETHODIMP
nsDOMMouseScrollEvent::InitMouseScrollEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                                nsIDOMWindow *aView, PRInt32 aDetail, PRInt32 aScreenX, 
                                PRInt32 aScreenY, PRInt32 aClientX, PRInt32 aClientY, 
                                bool aCtrlKey, bool aAltKey, bool aShiftKey, 
                                bool aMetaKey, PRUint16 aButton, nsIDOMEventTarget *aRelatedTarget,
                                PRInt32 aAxis)
{
  nsresult rv = nsDOMMouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                                                aScreenX, aScreenY, aClientX, aClientY, aCtrlKey,
                                                aAltKey, aShiftKey, aMetaKey, aButton, aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (mEvent->eventStructType == NS_MOUSE_SCROLL_EVENT) {
    static_cast<nsMouseScrollEvent*>(mEvent)->scrollFlags =
        (aAxis == HORIZONTAL_AXIS) ? nsMouseScrollEvent::kIsHorizontal
                                   : nsMouseScrollEvent::kIsVertical;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsDOMMouseScrollEvent::GetAxis(PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (mEvent->eventStructType == NS_MOUSE_SCROLL_EVENT) {
    PRUint32 flags = static_cast<nsMouseScrollEvent*>(mEvent)->scrollFlags;
    *aResult = (flags & nsMouseScrollEvent::kIsHorizontal)
         ? PRInt32(HORIZONTAL_AXIS) : PRInt32(VERTICAL_AXIS);
  } else {
    *aResult = 0;
  }
  return NS_OK;
}

nsresult NS_NewDOMMouseScrollEvent(nsIDOMEvent** aInstancePtrResult,
                                   nsPresContext* aPresContext,
                                   nsInputEvent *aEvent) 
{
  nsDOMMouseScrollEvent* it = new nsDOMMouseScrollEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
