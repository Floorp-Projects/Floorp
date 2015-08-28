/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MouseScrollEvent.h"
#include "mozilla/MouseEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

MouseScrollEvent::MouseScrollEvent(EventTarget* aOwner,
                                   nsPresContext* aPresContext,
                                   WidgetMouseScrollEvent* aEvent)
  : MouseEvent(aOwner, aPresContext,
               aEvent ? aEvent :
                        new WidgetMouseScrollEvent(false, eVoidEvent, nullptr))
{
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<WidgetMouseEventBase*>(mEvent)->inputSource =
      nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }

  mDetail = mEvent->AsMouseScrollEvent()->delta;
}

NS_IMPL_ADDREF_INHERITED(MouseScrollEvent, MouseEvent)
NS_IMPL_RELEASE_INHERITED(MouseScrollEvent, MouseEvent)

NS_INTERFACE_MAP_BEGIN(MouseScrollEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseScrollEvent)
NS_INTERFACE_MAP_END_INHERITING(MouseEvent)

NS_IMETHODIMP
MouseScrollEvent::InitMouseScrollEvent(const nsAString& aType,
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
                                       int32_t aAxis)
{
  nsresult rv =
    MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                               aScreenX, aScreenY, aClientX, aClientY,
                               aCtrlKey, aAltKey, aShiftKey, aMetaKey, aButton,
                               aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);
  mEvent->AsMouseScrollEvent()->isHorizontal = (aAxis == HORIZONTAL_AXIS);
  return NS_OK;
}


NS_IMETHODIMP
MouseScrollEvent::GetAxis(int32_t* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = Axis();
  return NS_OK;
}

int32_t
MouseScrollEvent::Axis()
{
  return mEvent->AsMouseScrollEvent()->isHorizontal ?
           static_cast<int32_t>(HORIZONTAL_AXIS) :
           static_cast<int32_t>(VERTICAL_AXIS);
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace dom;

already_AddRefed<MouseScrollEvent>
NS_NewDOMMouseScrollEvent(EventTarget* aOwner,
                          nsPresContext* aPresContext,
                          WidgetMouseScrollEvent* aEvent)
{
  nsRefPtr<MouseScrollEvent> it =
    new MouseScrollEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
