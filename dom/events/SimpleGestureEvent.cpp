/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/SimpleGestureEvent.h"
#include "mozilla/TouchEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

SimpleGestureEvent::SimpleGestureEvent(EventTarget* aOwner,
                                       nsPresContext* aPresContext,
                                       WidgetSimpleGestureEvent* aEvent)
  : MouseEvent(aOwner, aPresContext,
               aEvent ? aEvent :
                        new WidgetSimpleGestureEvent(false, eVoidEvent,
                                                     nullptr))
{
  NS_ASSERTION(mEvent->mClass == eSimpleGestureEventClass,
               "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    static_cast<WidgetMouseEventBase*>(mEvent)->inputSource =
      nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }
}

NS_IMPL_ADDREF_INHERITED(SimpleGestureEvent, MouseEvent)
NS_IMPL_RELEASE_INHERITED(SimpleGestureEvent, MouseEvent)

NS_INTERFACE_MAP_BEGIN(SimpleGestureEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMSimpleGestureEvent)
NS_INTERFACE_MAP_END_INHERITING(MouseEvent)

uint32_t
SimpleGestureEvent::AllowedDirections()
{
  return mEvent->AsSimpleGestureEvent()->mAllowedDirections;
}

NS_IMETHODIMP
SimpleGestureEvent::GetAllowedDirections(uint32_t* aAllowedDirections)
{
  NS_ENSURE_ARG_POINTER(aAllowedDirections);
  *aAllowedDirections = AllowedDirections();
  return NS_OK;
}

NS_IMETHODIMP
SimpleGestureEvent::SetAllowedDirections(uint32_t aAllowedDirections)
{
  mEvent->AsSimpleGestureEvent()->mAllowedDirections = aAllowedDirections;
  return NS_OK;
}

uint32_t
SimpleGestureEvent::Direction()
{
  return mEvent->AsSimpleGestureEvent()->mDirection;
}

NS_IMETHODIMP
SimpleGestureEvent::GetDirection(uint32_t* aDirection)
{
  NS_ENSURE_ARG_POINTER(aDirection);
  *aDirection = Direction();
  return NS_OK;
}

double
SimpleGestureEvent::Delta()
{
  return mEvent->AsSimpleGestureEvent()->delta;
}

NS_IMETHODIMP
SimpleGestureEvent::GetDelta(double* aDelta)
{
  NS_ENSURE_ARG_POINTER(aDelta);
  *aDelta = Delta();
  return NS_OK;
}

uint32_t
SimpleGestureEvent::ClickCount()
{
  return mEvent->AsSimpleGestureEvent()->clickCount;
}

NS_IMETHODIMP
SimpleGestureEvent::GetClickCount(uint32_t* aClickCount)
{
  NS_ENSURE_ARG_POINTER(aClickCount);
  *aClickCount = ClickCount();
  return NS_OK;
}

void
SimpleGestureEvent::InitSimpleGestureEvent(const nsAString& aTypeArg,
                                           bool aCanBubbleArg,
                                           bool aCancelableArg,
                                           nsGlobalWindow* aViewArg,
                                           int32_t aDetailArg,
                                           int32_t aScreenX, 
                                           int32_t aScreenY,
                                           int32_t aClientX,
                                           int32_t aClientY,
                                           bool aCtrlKeyArg,
                                           bool aAltKeyArg,
                                           bool aShiftKeyArg,
                                           bool aMetaKeyArg,
                                           uint16_t aButton,
                                           EventTarget* aRelatedTarget,
                                           uint32_t aAllowedDirectionsArg,
                                           uint32_t aDirectionArg,
                                           double aDeltaArg,
                                           uint32_t aClickCountArg)
{
  MouseEvent::InitMouseEvent(aTypeArg, aCanBubbleArg, aCancelableArg,
                             aViewArg, aDetailArg,
                             aScreenX, aScreenY, aClientX, aClientY,
                             aCtrlKeyArg, aAltKeyArg, aShiftKeyArg,
                             aMetaKeyArg, aButton, aRelatedTarget);

  WidgetSimpleGestureEvent* simpleGestureEvent = mEvent->AsSimpleGestureEvent();
  simpleGestureEvent->mAllowedDirections = aAllowedDirectionsArg;
  simpleGestureEvent->mDirection = aDirectionArg;
  simpleGestureEvent->delta = aDeltaArg;
  simpleGestureEvent->clickCount = aClickCountArg;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<SimpleGestureEvent>
NS_NewDOMSimpleGestureEvent(EventTarget* aOwner,
                            nsPresContext* aPresContext,
                            WidgetSimpleGestureEvent* aEvent)
{
  RefPtr<SimpleGestureEvent> it =
    new SimpleGestureEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
