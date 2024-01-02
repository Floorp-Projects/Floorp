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
#include "mozilla/dom/PointerEventHandler.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/StaticPrefs_dom.h"
#include "nsContentUtils.h"
#include "prtime.h"
#include "jsfriendapi.h"

namespace mozilla::dom {

PointerEvent::PointerEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                           WidgetPointerEvent* aEvent)
    : MouseEvent(aOwner, aPresContext,
                 aEvent ? aEvent
                        : new WidgetPointerEvent(false, eVoidEvent, nullptr)) {
  NS_ASSERTION(mEvent->mClass == ePointerEventClass,
               "event type mismatch ePointerEventClass");

  WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent();
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    mouseEvent->mInputSource = MouseEvent_Binding::MOZ_SOURCE_UNKNOWN;
  }
  // 5.2 Pointer Event types, for all pointer events, |detail| attribute SHOULD
  // be 0.
  mDetail = 0;
}

JSObject* PointerEvent::WrapObjectInternal(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return PointerEvent_Binding::Wrap(aCx, this, aGivenProto);
}

static uint16_t ConvertStringToPointerType(const nsAString& aPointerTypeArg) {
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

void ConvertPointerTypeToString(uint16_t aPointerTypeSrc,
                                nsAString& aPointerTypeDest) {
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
already_AddRefed<PointerEvent> PointerEvent::Constructor(
    EventTarget* aOwner, const nsAString& aType,
    const PointerEventInit& aParam) {
  RefPtr<PointerEvent> e = new PointerEvent(aOwner, nullptr, nullptr);
  bool trusted = e->Init(aOwner);

  e->InitMouseEvent(aType, aParam.mBubbles, aParam.mCancelable, aParam.mView,
                    aParam.mDetail, aParam.mScreenX, aParam.mScreenY,
                    aParam.mClientX, aParam.mClientY, false, false, false,
                    false, aParam.mButton, aParam.mRelatedTarget);
  e->InitializeExtraMouseEventDictionaryMembers(aParam);
  e->mPointerType = Some(aParam.mPointerType);

  WidgetPointerEvent* widgetEvent = e->mEvent->AsPointerEvent();
  widgetEvent->pointerId = aParam.mPointerId;
  widgetEvent->mWidth = aParam.mWidth;
  widgetEvent->mHeight = aParam.mHeight;
  widgetEvent->mPressure = aParam.mPressure;
  widgetEvent->tangentialPressure = aParam.mTangentialPressure;
  widgetEvent->tiltX = aParam.mTiltX;
  widgetEvent->tiltY = aParam.mTiltY;
  widgetEvent->twist = aParam.mTwist;
  widgetEvent->mInputSource = ConvertStringToPointerType(aParam.mPointerType);
  widgetEvent->mIsPrimary = aParam.mIsPrimary;
  widgetEvent->mButtons = aParam.mButtons;

  if (!aParam.mCoalescedEvents.IsEmpty()) {
    e->mCoalescedEvents.AppendElements(aParam.mCoalescedEvents);
  }
  if (!aParam.mPredictedEvents.IsEmpty()) {
    e->mPredictedEvents.AppendElements(aParam.mPredictedEvents);
  }
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

// static
already_AddRefed<PointerEvent> PointerEvent::Constructor(
    const GlobalObject& aGlobal, const nsAString& aType,
    const PointerEventInit& aParam) {
  nsCOMPtr<EventTarget> owner = do_QueryInterface(aGlobal.GetAsSupports());
  return Constructor(owner, aType, aParam);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PointerEvent)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(PointerEvent, MouseEvent)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCoalescedEvents)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPredictedEvents)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(PointerEvent, MouseEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCoalescedEvents)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPredictedEvents)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PointerEvent)
NS_INTERFACE_MAP_END_INHERITING(MouseEvent)

NS_IMPL_ADDREF_INHERITED(PointerEvent, MouseEvent)
NS_IMPL_RELEASE_INHERITED(PointerEvent, MouseEvent)

void PointerEvent::GetPointerType(nsAString& aPointerType) {
  if (mPointerType.isSome()) {
    aPointerType = mPointerType.value();
    return;
  }

  if (ShouldResistFingerprinting()) {
    aPointerType.AssignLiteral("mouse");
    return;
  }

  ConvertPointerTypeToString(mEvent->AsPointerEvent()->mInputSource,
                             aPointerType);
}

int32_t PointerEvent::PointerId() {
  return ShouldResistFingerprinting()
             ? PointerEventHandler::GetSpoofedPointerIdForRFP()
             : mEvent->AsPointerEvent()->pointerId;
}

int32_t PointerEvent::Width() {
  return ShouldResistFingerprinting() ? 1 : mEvent->AsPointerEvent()->mWidth;
}

int32_t PointerEvent::Height() {
  return ShouldResistFingerprinting() ? 1 : mEvent->AsPointerEvent()->mHeight;
}

float PointerEvent::Pressure() {
  if (mEvent->mMessage == ePointerUp || !ShouldResistFingerprinting()) {
    return mEvent->AsPointerEvent()->mPressure;
  }

  // According to [1], we should use 0.5 when it is in active buttons state and
  // 0 otherwise for devices that don't support pressure. And a pointerup event
  // always reports 0, so we don't need to spoof that.
  //
  // [1] https://www.w3.org/TR/pointerevents/#dom-pointerevent-pressure
  float spoofedPressure = 0.0;
  if (mEvent->AsPointerEvent()->mButtons) {
    spoofedPressure = 0.5;
  }

  return spoofedPressure;
}

float PointerEvent::TangentialPressure() {
  return ShouldResistFingerprinting()
             ? 0
             : mEvent->AsPointerEvent()->tangentialPressure;
}

int32_t PointerEvent::TiltX() {
  return ShouldResistFingerprinting() ? 0 : mEvent->AsPointerEvent()->tiltX;
}

int32_t PointerEvent::TiltY() {
  return ShouldResistFingerprinting() ? 0 : mEvent->AsPointerEvent()->tiltY;
}

int32_t PointerEvent::Twist() {
  return ShouldResistFingerprinting() ? 0 : mEvent->AsPointerEvent()->twist;
}

bool PointerEvent::IsPrimary() { return mEvent->AsPointerEvent()->mIsPrimary; }

bool PointerEvent::EnableGetCoalescedEvents(JSContext* aCx, JSObject* aGlobal) {
  return !StaticPrefs::
             dom_w3c_pointer_events_getcoalescedevents_only_in_securecontext() ||
         nsContentUtils::IsSecureContextOrWebExtension(aCx, aGlobal);
}

void PointerEvent::GetCoalescedEvents(
    nsTArray<RefPtr<PointerEvent>>& aPointerEvents) {
  WidgetPointerEvent* widgetEvent = mEvent->AsPointerEvent();
  if (mCoalescedEvents.IsEmpty() && widgetEvent &&
      widgetEvent->mCoalescedWidgetEvents &&
      !widgetEvent->mCoalescedWidgetEvents->mEvents.IsEmpty()) {
    nsCOMPtr<EventTarget> owner = do_QueryInterface(mOwner);
    for (WidgetPointerEvent& event :
         widgetEvent->mCoalescedWidgetEvents->mEvents) {
      RefPtr<PointerEvent> domEvent =
          NS_NewDOMPointerEvent(owner, nullptr, &event);

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
  if (mEvent->IsTrusted() && mEvent->mTarget) {
    for (RefPtr<PointerEvent>& pointerEvent : mCoalescedEvents) {
      // Only set event target when it's null.
      if (!pointerEvent->mEvent->mTarget) {
        pointerEvent->mEvent->mTarget = mEvent->mTarget;
      }
    }
  }
  aPointerEvents.AppendElements(mCoalescedEvents);
}

void PointerEvent::GetPredictedEvents(
    nsTArray<RefPtr<PointerEvent>>& aPointerEvents) {
  // XXX Add support for native predicted events, bug 1550461
  if (mEvent->IsTrusted() && mEvent->mTarget) {
    for (RefPtr<PointerEvent>& pointerEvent : mPredictedEvents) {
      // Only set event target when it's null.
      if (!pointerEvent->mEvent->mTarget) {
        pointerEvent->mEvent->mTarget = mEvent->mTarget;
      }
    }
  }
  aPointerEvents.AppendElements(mPredictedEvents);
}

bool PointerEvent::ShouldResistFingerprinting() {
  // There are three simple situations we don't need to spoof this pointer
  // event.
  //   1. The pref privcy.resistFingerprinting' is false, we fast return here
  //      since we don't need to do any QI of following codes.
  //   2. This event is generated by scripts.
  //   3. This event is a mouse pointer event.
  //  We don't need to check for the system group since pointer events won't be
  //  dispatched to the system group.
  if (!nsContentUtils::ShouldResistFingerprinting("Efficiency Check",
                                                  RFPTarget::PointerEvents) ||
      !mEvent->IsTrusted() ||
      mEvent->AsPointerEvent()->mInputSource ==
          MouseEvent_Binding::MOZ_SOURCE_MOUSE) {
    return false;
  }

  // Pref is checked above, so use true as fallback.
  nsCOMPtr<Document> doc = GetDocument();
  return doc ? doc->ShouldResistFingerprinting(RFPTarget::PointerEvents) : true;
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<PointerEvent> NS_NewDOMPointerEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    WidgetPointerEvent* aEvent) {
  RefPtr<PointerEvent> it = new PointerEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
