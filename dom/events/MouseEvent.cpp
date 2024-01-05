/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MouseEvent.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/BasePrincipal.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIScreenManager.h"
#include "prtime.h"

namespace mozilla::dom {

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
      mDefaultClientPoint.x = aClientX;
      mDefaultClientPoint.y = aClientY;
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

void MouseEvent::PreventClickEvent() {
  if (WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent()) {
    mouseEvent->mClickEventPrevented = true;
  }
}

bool MouseEvent::ClickEventPrevented() {
  if (WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent()) {
    return mouseEvent->mClickEventPrevented;
  }
  return false;
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
      return MouseButton::ePrimary;
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

CSSIntPoint MouseEvent::ScreenPoint(CallerType aCallerType) const {
  if (mEvent->mFlags.mIsPositionless) {
    return {};
  }

  if (nsContentUtils::ShouldResistFingerprinting(
          aCallerType, GetParentObject(), RFPTarget::MouseEventScreenPoint)) {
    // Sanitize to something sort of like client cooords, but not quite
    // (defaulting to (0,0) instead of our pre-specified client coords).
    return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                  CSSIntPoint(0, 0));
  }

  return Event::GetScreenCoords(mPresContext, mEvent, mEvent->mRefPoint)
      .extract();
}

LayoutDeviceIntPoint MouseEvent::ScreenPointLayoutDevicePix() const {
  const CSSIntPoint point = ScreenPoint(CallerType::System);
  auto scale = mPresContext ? mPresContext->CSSToDevPixelScale()
                            : CSSToLayoutDeviceScale();
  return LayoutDeviceIntPoint::Round(point * scale);
}

DesktopIntPoint MouseEvent::ScreenPointDesktopPix() const {
  const CSSIntPoint point = ScreenPoint(CallerType::System);
  auto scale =
      mPresContext
          ? mPresContext->CSSToDevPixelScale() /
                mPresContext->DeviceContext()->GetDesktopToDeviceScale()
          : CSSToDesktopScale();
  return DesktopIntPoint::Round(point * scale);
}

already_AddRefed<nsIScreen> MouseEvent::GetScreen() {
  nsCOMPtr<nsIScreenManager> screenMgr =
      do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (!screenMgr) {
    return nullptr;
  }
  return screenMgr->ScreenForRect(
      DesktopIntRect(ScreenPointDesktopPix(), DesktopIntSize(1, 1)));
}

CSSIntPoint MouseEvent::PagePoint() const {
  if (mEvent->mFlags.mIsPositionless) {
    return {};
  }

  if (mPrivateDataDuplicated) {
    return mPagePoint;
  }

  return Event::GetPageCoords(mPresContext, mEvent, mEvent->mRefPoint,
                              mDefaultClientPoint);
}

CSSIntPoint MouseEvent::ClientPoint() const {
  if (mEvent->mFlags.mIsPositionless) {
    return {};
  }

  return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mDefaultClientPoint);
}

CSSIntPoint MouseEvent::OffsetPoint() const {
  if (mEvent->mFlags.mIsPositionless) {
    return {};
  }

  return Event::GetOffsetCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mDefaultClientPoint);
}

bool MouseEvent::AltKey() { return mEvent->AsInputEvent()->IsAlt(); }

bool MouseEvent::CtrlKey() { return mEvent->AsInputEvent()->IsControl(); }

bool MouseEvent::ShiftKey() { return mEvent->AsInputEvent()->IsShift(); }

bool MouseEvent::MetaKey() { return mEvent->AsInputEvent()->IsMeta(); }

float MouseEvent::MozPressure() const {
  return mEvent->AsMouseEventBase()->mPressure;
}

uint16_t MouseEvent::InputSource() const {
  return mEvent->AsMouseEventBase()->mInputSource;
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<MouseEvent> NS_NewDOMMouseEvent(EventTarget* aOwner,
                                                 nsPresContext* aPresContext,
                                                 WidgetMouseEvent* aEvent) {
  RefPtr<MouseEvent> it = new MouseEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
