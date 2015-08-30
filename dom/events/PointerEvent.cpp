/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#include "mozilla/dom/PointerEvent.h"
#include "mozilla/MouseEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

PointerEvent::PointerEvent(EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           WidgetPointerEvent* aEvent)
  : MouseEvent(aOwner, aPresContext,
               aEvent ? aEvent :
                        new WidgetPointerEvent(false, eVoidEvent, nullptr))
{
  NS_ASSERTION(mEvent->mClass == ePointerEventClass,
               "event type mismatch ePointerEventClass");

  WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent();
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }
}

static uint16_t
ConvertStringToPointerType(const nsAString& aPointerTypeArg)
{
  if (aPointerTypeArg.EqualsLiteral("mouse")) {
    return nsIDOMMouseEvent::MOZ_SOURCE_MOUSE;
  }
  if (aPointerTypeArg.EqualsLiteral("pen")) {
    return nsIDOMMouseEvent::MOZ_SOURCE_PEN;
  }
  if (aPointerTypeArg.EqualsLiteral("touch")) {
    return nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
  }

  return nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
}

void
ConvertPointerTypeToString(uint16_t aPointerTypeSrc, nsAString& aPointerTypeDest)
{
  switch (aPointerTypeSrc) {
    case nsIDOMMouseEvent::MOZ_SOURCE_MOUSE:
      aPointerTypeDest.AssignLiteral("mouse");
      break;
    case nsIDOMMouseEvent::MOZ_SOURCE_PEN:
      aPointerTypeDest.AssignLiteral("pen");
      break;
    case nsIDOMMouseEvent::MOZ_SOURCE_TOUCH:
      aPointerTypeDest.AssignLiteral("touch");
      break;
    default:
      aPointerTypeDest.Truncate();
      break;
  }
}

// static
already_AddRefed<PointerEvent>
PointerEvent::Constructor(EventTarget* aOwner,
                          const nsAString& aType,
                          const PointerEventInit& aParam)
{
  nsRefPtr<PointerEvent> e = new PointerEvent(aOwner, nullptr, nullptr);
  bool trusted = e->Init(aOwner);

  e->InitMouseEvent(aType, aParam.mBubbles, aParam.mCancelable,
                    aParam.mView, aParam.mDetail, aParam.mScreenX,
                    aParam.mScreenY, aParam.mClientX, aParam.mClientY,
                    false, false, false, false, aParam.mButton,
                    aParam.mRelatedTarget);
  e->InitializeExtraMouseEventDictionaryMembers(aParam);

  WidgetPointerEvent* widgetEvent = e->mEvent->AsPointerEvent();
  widgetEvent->pointerId = aParam.mPointerId;
  widgetEvent->width = aParam.mWidth;
  widgetEvent->height = aParam.mHeight;
  widgetEvent->pressure = aParam.mPressure;
  widgetEvent->tiltX = aParam.mTiltX;
  widgetEvent->tiltY = aParam.mTiltY;
  widgetEvent->inputSource = ConvertStringToPointerType(aParam.mPointerType);
  widgetEvent->isPrimary = aParam.mIsPrimary;
  widgetEvent->buttons = aParam.mButtons;

  e->SetTrusted(trusted);
  return e.forget();
}

// static
already_AddRefed<PointerEvent>
PointerEvent::Constructor(const GlobalObject& aGlobal,
                          const nsAString& aType,
                          const PointerEventInit& aParam,
                          ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aParam);
}

void
PointerEvent::GetPointerType(nsAString& aPointerType)
{
  ConvertPointerTypeToString(mEvent->AsPointerEvent()->inputSource, aPointerType);
}

int32_t
PointerEvent::PointerId()
{
  return mEvent->AsPointerEvent()->pointerId;
}

int32_t
PointerEvent::Width()
{
  return mEvent->AsPointerEvent()->width;
}

int32_t
PointerEvent::Height()
{
  return mEvent->AsPointerEvent()->height;
}

float
PointerEvent::Pressure()
{
  return mEvent->AsPointerEvent()->pressure;
}

int32_t
PointerEvent::TiltX()
{
  return mEvent->AsPointerEvent()->tiltX;
}

int32_t
PointerEvent::TiltY()
{
  return mEvent->AsPointerEvent()->tiltY;
}

bool
PointerEvent::IsPrimary()
{
  return mEvent->AsPointerEvent()->isPrimary;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<PointerEvent>
NS_NewDOMPointerEvent(EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      WidgetPointerEvent *aEvent)
{
  nsRefPtr<PointerEvent> it = new PointerEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
