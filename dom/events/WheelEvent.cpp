/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MouseEventBinding.h"
#include "mozilla/dom/WheelEvent.h"
#include "mozilla/MouseEvents.h"
#include "prtime.h"

namespace mozilla::dom {

WheelEvent::WheelEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                       WidgetWheelEvent* aWheelEvent)
    : MouseEvent(aOwner, aPresContext,
                 aWheelEvent
                     ? aWheelEvent
                     : new WidgetWheelEvent(false, eVoidEvent, nullptr)),
      mAppUnitsPerDevPixel(0) {
  if (aWheelEvent) {
    mEventIsInternal = false;
    // If the delta mode is pixel, the WidgetWheelEvent's delta values are in
    // device pixels.  However, JS contents need the delta values in CSS pixels.
    // We should store the value of mAppUnitsPerDevPixel here because
    // it might be changed by changing zoom or something.
    if (aWheelEvent->mDeltaMode == WheelEvent_Binding::DOM_DELTA_PIXEL) {
      mAppUnitsPerDevPixel = aPresContext->AppUnitsPerDevPixel();
    }
  } else {
    mEventIsInternal = true;
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    mEvent->AsWheelEvent()->mInputSource =
        MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
  }
}

void WheelEvent::InitWheelEvent(
    const nsAString& aType, bool aCanBubble, bool aCancelable,
    nsGlobalWindowInner* aView, int32_t aDetail, int32_t aScreenX,
    int32_t aScreenY, int32_t aClientX, int32_t aClientY, uint16_t aButton,
    EventTarget* aRelatedTarget, const nsAString& aModifiersList,
    double aDeltaX, double aDeltaY, double aDeltaZ, uint32_t aDeltaMode) {
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                             aScreenX, aScreenY, aClientX, aClientY, aButton,
                             aRelatedTarget, aModifiersList);

  WidgetWheelEvent* wheelEvent = mEvent->AsWheelEvent();
  // When specified by the caller (for JS-created events), don't mess with the
  // delta mode.
  wheelEvent->mDeltaModeCheckingState =
      WidgetWheelEvent::DeltaModeCheckingState::Checked;
  wheelEvent->mDeltaX = aDeltaX;
  wheelEvent->mDeltaY = aDeltaY;
  wheelEvent->mDeltaZ = aDeltaZ;
  wheelEvent->mDeltaMode = aDeltaMode;
  wheelEvent->mAllowToOverrideSystemScrollSpeed = false;
}

int32_t WheelEvent::WheelDeltaX(CallerType aCallerType) {
  WidgetWheelEvent* ev = mEvent->AsWheelEvent();
  if (ev->mWheelTicksX != 0.0) {
    return int32_t(-ev->mWheelTicksX * kNativeTicksToWheelDelta);
  }
  if (IsTrusted()) {
    // We always return pixels regardless of the checking-state.
    double pixelDelta =
        ev->mDeltaMode == WheelEvent_Binding::DOM_DELTA_PIXEL
            ? CSSCoord(DevToCssPixels(ev->OverriddenDeltaX()))
            : ev->OverriddenDeltaX() *
                  CSSPixel::FromAppUnits(ev->mScrollAmount.width).Rounded();
    return int32_t(-std::round(pixelDelta * kTrustedDeltaToWheelDelta));
  }
  return int32_t(-std::round(DeltaX(aCallerType)));  // This matches Safari.
}

int32_t WheelEvent::WheelDeltaY(CallerType aCallerType) {
  WidgetWheelEvent* ev = mEvent->AsWheelEvent();
  if (ev->mWheelTicksY != 0.0) {
    return int32_t(-ev->mWheelTicksY * kNativeTicksToWheelDelta);
  }

  if (IsTrusted()) {
    double pixelDelta =
        ev->mDeltaMode == WheelEvent_Binding::DOM_DELTA_PIXEL
            ? CSSCoord(DevToCssPixels(ev->OverriddenDeltaY()))
            : ev->OverriddenDeltaY() *
                  CSSPixel::FromAppUnits(ev->mScrollAmount.height).Rounded();
    return int32_t(-std::round(pixelDelta * kTrustedDeltaToWheelDelta));
  }
  return int32_t(-std::round(DeltaY(aCallerType)));  // This matches Safari.
}

double WheelEvent::ToWebExposedDelta(WidgetWheelEvent& aWidgetEvent,
                                     double aDelta, nscoord aLineOrPageAmount,
                                     CallerType aCallerType) {
  using DeltaModeCheckingState = WidgetWheelEvent::DeltaModeCheckingState;
  if (aCallerType != CallerType::System) {
    if (aWidgetEvent.mDeltaModeCheckingState ==
        DeltaModeCheckingState::Unknown) {
      aWidgetEvent.mDeltaModeCheckingState = DeltaModeCheckingState::Unchecked;
    }
    if (aWidgetEvent.mDeltaModeCheckingState ==
            DeltaModeCheckingState::Unchecked &&
        aWidgetEvent.mDeltaMode == WheelEvent_Binding::DOM_DELTA_LINE) {
      return aDelta * CSSPixel::FromAppUnits(aLineOrPageAmount).Rounded();
    }
  }
  return DevToCssPixels(aDelta);
}

double WheelEvent::DeltaX(CallerType aCallerType) {
  WidgetWheelEvent* ev = mEvent->AsWheelEvent();
  return ToWebExposedDelta(*ev, ev->OverriddenDeltaX(), ev->mScrollAmount.width,
                           aCallerType);
}

double WheelEvent::DeltaY(CallerType aCallerType) {
  WidgetWheelEvent* ev = mEvent->AsWheelEvent();
  return ToWebExposedDelta(*ev, ev->OverriddenDeltaY(),
                           ev->mScrollAmount.height, aCallerType);
}

double WheelEvent::DeltaZ(CallerType aCallerType) {
  WidgetWheelEvent* ev = mEvent->AsWheelEvent();
  // XXX Unclear what scroll amount we should use for deltaZ...
  auto amount = std::max(ev->mScrollAmount.width, ev->mScrollAmount.height);
  return ToWebExposedDelta(*ev, ev->mDeltaZ, amount, aCallerType);
}

uint32_t WheelEvent::DeltaMode(CallerType aCallerType) {
  using DeltaModeCheckingState = WidgetWheelEvent::DeltaModeCheckingState;

  WidgetWheelEvent* ev = mEvent->AsWheelEvent();
  uint32_t mode = ev->mDeltaMode;
  if (aCallerType != CallerType::System) {
    if (ev->mDeltaModeCheckingState == DeltaModeCheckingState::Unknown) {
      ev->mDeltaModeCheckingState = DeltaModeCheckingState::Checked;
    } else if (ev->mDeltaModeCheckingState ==
                   DeltaModeCheckingState::Unchecked &&
               mode == WheelEvent_Binding::DOM_DELTA_LINE) {
      return WheelEvent_Binding::DOM_DELTA_PIXEL;
    }
  }

  return mode;
}

already_AddRefed<WheelEvent> WheelEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const WheelEventInit& aParam) {
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<WheelEvent> e = new WheelEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitWheelEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                    aParam.mDetail, aParam.mScreenX, aParam.mScreenY,
                    aParam.mClientX, aParam.mClientY, aParam.mButton,
                    aParam.mRelatedTarget, u""_ns, aParam.mDeltaX,
                    aParam.mDeltaY, aParam.mDeltaZ, aParam.mDeltaMode);
  e->InitializeExtraMouseEventDictionaryMembers(aParam);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<WheelEvent> NS_NewDOMWheelEvent(EventTarget* aOwner,
                                                 nsPresContext* aPresContext,
                                                 WidgetWheelEvent* aEvent) {
  RefPtr<WheelEvent> it = new WheelEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
