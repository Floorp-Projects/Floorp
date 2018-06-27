/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Portions Copyright 2013 Microsoft Open Technologies, Inc. */

#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/PointerEvent.h"
#include "mozilla/dom/PointerEventBinding.h"
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
    mEvent->mTime = PR_Now();
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    mouseEvent->inputSource = MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
  }
  // 5.2 Pointer Event types, for all pointer events, |detail| attribute SHOULD
  // be 0.
  mDetail = 0;
}

JSObject*
PointerEvent::WrapObjectInternal(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto)
{
  return PointerEvent_Binding::Wrap(aCx, this, aGivenProto);
}

static uint16_t
ConvertStringToPointerType(const nsAString& aPointerTypeArg)
{
  if (aPointerTypeArg.EqualsLiteral("mouse")) {
    return MouseEvent_Binding::MOZ_SOURCE_MOUSE;
  }
  if (aPointerTypeArg.EqualsLiteral("pen")) {
    return MouseEvent_Binding::MOZ_SOURCE_PEN;
  }
  if (aPointerTypeArg.EqualsLiteral("touch")) {
    return MouseEvent_Binding::MOZ_SOURCE_TOUCH;
  }

  return MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
}

void
ConvertPointerTypeToString(uint16_t aPointerTypeSrc, nsAString& aPointerTypeDest)
{
  switch (aPointerTypeSrc) {
    case MouseEvent_Binding::MOZ_SOURCE_MOUSE:
      aPointerTypeDest.AssignLiteral("mouse");
      break;
    case MouseEvent_Binding::MOZ_SOURCE_PEN:
      aPointerTypeDest.AssignLiteral("pen");
      break;
    case MouseEvent_Binding::MOZ_SOURCE_TOUCH:
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
  RefPtr<PointerEvent> e = new PointerEvent(aOwner, nullptr, nullptr);
  bool trusted = e->Init(aOwner);

  e->InitMouseEvent(aType, aParam.mBubbles, aParam.mCancelable,
                    aParam.mView, aParam.mDetail, aParam.mScreenX,
                    aParam.mScreenY, aParam.mClientX, aParam.mClientY,
                    false, false, false, false, aParam.mButton,
                    aParam.mRelatedTarget);
  e->InitializeExtraMouseEventDictionaryMembers(aParam);

  WidgetPointerEvent* widgetEvent = e->mEvent->AsPointerEvent();
  widgetEvent->pointerId = aParam.mPointerId;
  widgetEvent->mWidth = aParam.mWidth;
  widgetEvent->mHeight = aParam.mHeight;
  widgetEvent->pressure = aParam.mPressure;
  widgetEvent->tangentialPressure = aParam.mTangentialPressure;
  widgetEvent->tiltX = aParam.mTiltX;
  widgetEvent->tiltY = aParam.mTiltY;
  widgetEvent->twist = aParam.mTwist;
  widgetEvent->inputSource = ConvertStringToPointerType(aParam.mPointerType);
  widgetEvent->mIsPrimary = aParam.mIsPrimary;
  widgetEvent->buttons = aParam.mButtons;

  if (!aParam.mCoalescedEvents.IsEmpty()) {
    e->mCoalescedEvents.AppendElements(aParam.mCoalescedEvents);
  }
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
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

NS_IMPL_CYCLE_COLLECTION_CLASS(PointerEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PointerEvent, MouseEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCoalescedEvents)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PointerEvent, MouseEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCoalescedEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PointerEvent)
NS_INTERFACE_MAP_END_INHERITING(MouseEvent)

NS_IMPL_ADDREF_INHERITED(PointerEvent, MouseEvent)
NS_IMPL_RELEASE_INHERITED(PointerEvent, MouseEvent)

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
  return mEvent->AsPointerEvent()->mWidth;
}

int32_t
PointerEvent::Height()
{
  return mEvent->AsPointerEvent()->mHeight;
}

float
PointerEvent::Pressure()
{
  return mEvent->AsPointerEvent()->pressure;
}

float
PointerEvent::TangentialPressure()
{
  return mEvent->AsPointerEvent()->tangentialPressure;
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

int32_t
PointerEvent::Twist()
{
  return mEvent->AsPointerEvent()->twist;
}

bool
PointerEvent::IsPrimary()
{
  return mEvent->AsPointerEvent()->mIsPrimary;
}

void
PointerEvent::GetCoalescedEvents(nsTArray<RefPtr<PointerEvent>>& aPointerEvents)
{
  WidgetPointerEvent* widgetEvent = mEvent->AsPointerEvent();
  if (mCoalescedEvents.IsEmpty() && widgetEvent &&
      widgetEvent->mCoalescedWidgetEvents &&
      !widgetEvent->mCoalescedWidgetEvents->mEvents.IsEmpty()) {
    for (WidgetPointerEvent& event :
         widgetEvent->mCoalescedWidgetEvents->mEvents) {
      RefPtr<PointerEvent> domEvent =
        NS_NewDOMPointerEvent(nullptr, nullptr, &event);

      // The dom event is derived from an OS generated widget event. Setup
      // mWidget and mPresContext since they are necessary to calculate
      // offsetX / offsetY.
      domEvent->mEvent->AsGUIEvent()->mWidget = widgetEvent->mWidget;
      domEvent->mPresContext = mPresContext;

      // The coalesced widget mouse events shouldn't have been dispatched.
      MOZ_ASSERT(!domEvent->mEvent->mTarget);
      // The event target should be the same as the dispatched event's target.
      domEvent->mEvent->mTarget = mEvent->mTarget;

      // JS could hold reference to dom events. We have to ask dom event to
      // duplicate its private data to avoid the widget event is destroyed.
      domEvent->DuplicatePrivateData();

      // Setup mPresContext again after DuplicatePrivateData since it clears
      // mPresContext.
      domEvent->mPresContext = mPresContext;
      mCoalescedEvents.AppendElement(domEvent);
    }
  }
  if (mEvent->mTarget) {
    for (RefPtr<PointerEvent>& pointerEvent : mCoalescedEvents) {
      // Only set event target when it's null.
      if (!pointerEvent->mEvent->mTarget) {
        pointerEvent->mEvent->mTarget = mEvent->mTarget;
      }
    }
  }
  aPointerEvents.AppendElements(mCoalescedEvents);
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
  RefPtr<PointerEvent> it = new PointerEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
