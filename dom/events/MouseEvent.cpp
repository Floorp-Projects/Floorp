/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MouseEvent.h"
#include "mozilla/MouseEvents.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

MouseEvent::MouseEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                       WidgetMouseEventBase* aEvent)
    : UIEvent(aOwner, aPresContext,
              aEvent ? aEvent
                     : new WidgetMouseEvent(false, eVoidEvent, nullptr,
                                            WidgetMouseEvent::eReal)) {
  // There's no way to make this class' ctor allocate an WidgetMouseScrollEvent.
  // It's not that important, though, since a scroll event is not a real
  // DOM event.

  WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent();
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    mouseEvent->mInputSource = MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
  }

  if (mouseEvent) {
    MOZ_ASSERT(mouseEvent->mReason != WidgetMouseEvent::eSynthesized,
               "Don't dispatch DOM events from synthesized mouse events");
    mDetail = mouseEvent->mClickCount;
  }
}

void MouseEvent::InitMouseEvent(const nsAString& aType, bool aCanBubble,
                                bool aCancelable, nsGlobalWindowInner* aView,
                                int32_t aDetail, int32_t aScreenX,
                                int32_t aScreenY, int32_t aClientX,
                                int32_t aClientY, bool aCtrlKey, bool aAltKey,
                                bool aShiftKey, bool aMetaKey, uint16_t aButton,
                                EventTarget* aRelatedTarget) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);

  switch (mEvent->mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case ePointerEventClass:
    case eSimpleGestureEventClass: {
      WidgetMouseEventBase* mouseEventBase = mEvent->AsMouseEventBase();
      mouseEventBase->mRelatedTarget = aRelatedTarget;
      mouseEventBase->mButton = aButton;
      mouseEventBase->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey,
                                         aMetaKey);
      mClientPoint.x = aClientX;
      mClientPoint.y = aClientY;
      mouseEventBase->mRefPoint.x = aScreenX;
      mouseEventBase->mRefPoint.y = aScreenY;

      WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent();
      if (mouseEvent) {
        mouseEvent->mClickCount = aDetail;
      }
      break;
    }
    default:
      break;
  }
}

void MouseEvent::InitMouseEvent(const nsAString& aType, bool aCanBubble,
                                bool aCancelable, nsGlobalWindowInner* aView,
                                int32_t aDetail, int32_t aScreenX,
                                int32_t aScreenY, int32_t aClientX,
                                int32_t aClientY, int16_t aButton,
                                EventTarget* aRelatedTarget,
                                const nsAString& aModifiersList) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  Modifiers modifiers = ComputeModifierState(aModifiersList);

  InitMouseEvent(
      aType, aCanBubble, aCancelable, aView, aDetail, aScreenX, aScreenY,
      aClientX, aClientY, (modifiers & MODIFIER_CONTROL) != 0,
      (modifiers & MODIFIER_ALT) != 0, (modifiers & MODIFIER_SHIFT) != 0,
      (modifiers & MODIFIER_META) != 0, aButton, aRelatedTarget);

  switch (mEvent->mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case ePointerEventClass:
    case eSimpleGestureEventClass:
      mEvent->AsInputEvent()->mModifiers = modifiers;
      return;
    default:
      MOZ_CRASH("There is no space to store the modifiers");
  }
}

void MouseEvent::InitializeExtraMouseEventDictionaryMembers(
    const MouseEventInit& aParam) {
  InitModifiers(aParam);
  mEvent->AsMouseEventBase()->mButtons = aParam.mButtons;
  mMovementPoint.x = aParam.mMovementX;
  mMovementPoint.y = aParam.mMovementY;
}

already_AddRefed<MouseEvent> MouseEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const MouseEventInit& aParam) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<MouseEvent> e = new MouseEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitMouseEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                    aParam.mDetail, aParam.mScreenX, aParam.mScreenY,
                    aParam.mClientX, aParam.mClientY, aParam.mCtrlKey,
                    aParam.mAltKey, aParam.mShiftKey, aParam.mMetaKey,
                    aParam.mButton, aParam.mRelatedTarget);
  e->InitializeExtraMouseEventDictionaryMembers(aParam);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

void MouseEvent::InitNSMouseEvent(const nsAString& aType, bool aCanBubble,
                                  bool aCancelable, nsGlobalWindowInner* aView,
                                  int32_t aDetail, int32_t aScreenX,
                                  int32_t aScreenY, int32_t aClientX,
                                  int32_t aClientY, bool aCtrlKey, bool aAltKey,
                                  bool aShiftKey, bool aMetaKey,
                                  uint16_t aButton, EventTarget* aRelatedTarget,
                                  float aPressure, uint16_t aInputSource) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                             aScreenX, aScreenY, aClientX, aClientY, aCtrlKey,
                             aAltKey, aShiftKey, aMetaKey, aButton,
                             aRelatedTarget);

  WidgetMouseEventBase* mouseEventBase = mEvent->AsMouseEventBase();
  mouseEventBase->mPressure = aPressure;
  mouseEventBase->mInputSource = aInputSource;
}

int16_t MouseEvent::Button() {
  switch (mEvent->mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case ePointerEventClass:
    case eSimpleGestureEventClass:
      return mEvent->AsMouseEventBase()->mButton;
    default:
      NS_WARNING("Tried to get mouse mButton for non-mouse event!");
      return MouseButton::eLeft;
  }
}

uint16_t MouseEvent::Buttons() {
  switch (mEvent->mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case ePointerEventClass:
    case eSimpleGestureEventClass:
      return mEvent->AsMouseEventBase()->mButtons;
    default:
      MOZ_CRASH("Tried to get mouse buttons for non-mouse event!");
  }
}

already_AddRefed<EventTarget> MouseEvent::GetRelatedTarget() {
  nsCOMPtr<EventTarget> relatedTarget;
  switch (mEvent->mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case ePointerEventClass:
    case eSimpleGestureEventClass:
      relatedTarget = mEvent->AsMouseEventBase()->mRelatedTarget;
      break;
    default:
      break;
  }

  return EnsureWebAccessibleRelatedTarget(relatedTarget);
}

void MouseEvent::GetRegion(nsAString& aRegion) {
  SetDOMStringToNull(aRegion);
  WidgetMouseEventBase* mouseEventBase = mEvent->AsMouseEventBase();
  if (mouseEventBase) {
    aRegion = mouseEventBase->mRegion;
  }
}

int32_t MouseEvent::ScreenX(CallerType aCallerType) {
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    // Sanitize to something sort of like client cooords, but not quite
    // (defaulting to (0,0) instead of our pre-specified client coords).
    return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                  CSSIntPoint(0, 0))
        .x;
  }

  return Event::GetScreenCoords(mPresContext, mEvent, mEvent->mRefPoint).x;
}

int32_t MouseEvent::ScreenY(CallerType aCallerType) {
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    // Sanitize to something sort of like client cooords, but not quite
    // (defaulting to (0,0) instead of our pre-specified client coords).
    return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                  CSSIntPoint(0, 0))
        .y;
  }

  return Event::GetScreenCoords(mPresContext, mEvent, mEvent->mRefPoint).y;
}

int32_t MouseEvent::PageX() const {
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  if (mPrivateDataDuplicated) {
    return mPagePoint.x;
  }

  return Event::GetPageCoords(mPresContext, mEvent, mEvent->mRefPoint,
                              mClientPoint)
      .x;
}

int32_t MouseEvent::PageY() const {
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  if (mPrivateDataDuplicated) {
    return mPagePoint.y;
  }

  return Event::GetPageCoords(mPresContext, mEvent, mEvent->mRefPoint,
                              mClientPoint)
      .y;
}

int32_t MouseEvent::ClientX() {
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mClientPoint)
      .x;
}

int32_t MouseEvent::ClientY() {
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mClientPoint)
      .y;
}

int32_t MouseEvent::OffsetX() {
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }
  return Event::GetOffsetCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mClientPoint)
      .x;
}

int32_t MouseEvent::OffsetY() {
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }
  return Event::GetOffsetCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mClientPoint)
      .y;
}

bool MouseEvent::AltKey() { return mEvent->AsInputEvent()->IsAlt(); }

bool MouseEvent::CtrlKey() { return mEvent->AsInputEvent()->IsControl(); }

bool MouseEvent::ShiftKey() { return mEvent->AsInputEvent()->IsShift(); }

bool MouseEvent::MetaKey() { return mEvent->AsInputEvent()->IsMeta(); }

float MouseEvent::MozPressure() const {
  return mEvent->AsMouseEventBase()->mPressure;
}

bool MouseEvent::HitCluster() const {
  return mEvent->AsMouseEventBase()->mHitCluster;
}

uint16_t MouseEvent::MozInputSource() const {
  return mEvent->AsMouseEventBase()->mInputSource;
}

}  // namespace dom
}  // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<MouseEvent> NS_NewDOMMouseEvent(EventTarget* aOwner,
                                                 nsPresContext* aPresContext,
                                                 WidgetMouseEvent* aEvent) {
  RefPtr<MouseEvent> it = new MouseEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
