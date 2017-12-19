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

MouseEvent::MouseEvent(EventTarget* aOwner,
                       nsPresContext* aPresContext,
                       WidgetMouseEventBase* aEvent)
  : UIEvent(aOwner, aPresContext,
            aEvent ? aEvent :
                     new WidgetMouseEvent(false, eVoidEvent, nullptr,
                                          WidgetMouseEvent::eReal))
{
  // There's no way to make this class' ctor allocate an WidgetMouseScrollEvent.
  // It's not that important, though, since a scroll event is not a real
  // DOM event.

  WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent();
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->mTime = PR_Now();
    mEvent->mRefPoint = LayoutDeviceIntPoint(0, 0);
    mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }

  if (mouseEvent) {
    MOZ_ASSERT(mouseEvent->mReason != WidgetMouseEvent::eSynthesized,
               "Don't dispatch DOM events from synthesized mouse events");
    mDetail = mouseEvent->mClickCount;
  }
}

NS_IMPL_ADDREF_INHERITED(MouseEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(MouseEvent, UIEvent)

NS_INTERFACE_MAP_BEGIN(MouseEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

void
MouseEvent::InitMouseEvent(const nsAString& aType,
                           bool aCanBubble,
                           bool aCancelable,
                           nsGlobalWindowInner* aView,
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
                           EventTarget* aRelatedTarget)
{
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);

  switch(mEvent->mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case ePointerEventClass:
    case eSimpleGestureEventClass: {
      WidgetMouseEventBase* mouseEventBase = mEvent->AsMouseEventBase();
      mouseEventBase->mRelatedTarget = aRelatedTarget;
      mouseEventBase->button = aButton;
      mouseEventBase->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey, aMetaKey);
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

NS_IMETHODIMP
MouseEvent::InitMouseEvent(const nsAString& aType,
                           bool aCanBubble,
                           bool aCancelable,
                           mozIDOMWindow* aView,
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
                           nsIDOMEventTarget* aRelatedTarget)
{
  MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                             nsGlobalWindowInner::Cast(aView), aDetail,
                             aScreenX, aScreenY,
                             aClientX, aClientY,
                             aCtrlKey, aAltKey, aShiftKey,
                             aMetaKey, aButton,
                             static_cast<EventTarget *>(aRelatedTarget));

  return NS_OK;
}

void
MouseEvent::InitMouseEvent(const nsAString& aType,
                           bool aCanBubble,
                           bool aCancelable,
                           nsGlobalWindowInner* aView,
                           int32_t aDetail,
                           int32_t aScreenX,
                           int32_t aScreenY,
                           int32_t aClientX,
                           int32_t aClientY,
                           int16_t aButton,
                           EventTarget* aRelatedTarget,
                           const nsAString& aModifiersList)
{
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  Modifiers modifiers = ComputeModifierState(aModifiersList);

  InitMouseEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                 aScreenX, aScreenY, aClientX, aClientY,
                 (modifiers & MODIFIER_CONTROL) != 0,
                 (modifiers & MODIFIER_ALT) != 0,
                 (modifiers & MODIFIER_SHIFT) != 0,
                 (modifiers & MODIFIER_META) != 0,
                 aButton, aRelatedTarget);

  switch(mEvent->mClass) {
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

void
MouseEvent::InitializeExtraMouseEventDictionaryMembers(const MouseEventInit& aParam)
{
  InitModifiers(aParam);
  mEvent->AsMouseEventBase()->buttons = aParam.mButtons;
  mMovementPoint.x = aParam.mMovementX;
  mMovementPoint.y = aParam.mMovementY;
}

already_AddRefed<MouseEvent>
MouseEvent::Constructor(const GlobalObject& aGlobal,
                        const nsAString& aType,
                        const MouseEventInit& aParam,
                        ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  RefPtr<MouseEvent> e = new MouseEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitMouseEvent(aType, aParam.mBubbles, aParam.mCancelable,
                    aParam.mView, aParam.mDetail, aParam.mScreenX,
                    aParam.mScreenY, aParam.mClientX, aParam.mClientY,
                    aParam.mCtrlKey, aParam.mAltKey, aParam.mShiftKey,
                    aParam.mMetaKey, aParam.mButton, aParam.mRelatedTarget);
  e->InitializeExtraMouseEventDictionaryMembers(aParam);
  e->SetTrusted(trusted);
  e->SetComposed(aParam.mComposed);
  return e.forget();
}

void
MouseEvent::InitNSMouseEvent(const nsAString& aType,
                             bool aCanBubble,
                             bool aCancelable,
                             nsGlobalWindowInner* aView,
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
                             float aPressure,
                             uint16_t aInputSource)
{
  NS_ENSURE_TRUE_VOID(!mEvent->mFlags.mIsBeingDispatched);

  MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                             aView, aDetail, aScreenX, aScreenY,
                             aClientX, aClientY,
                             aCtrlKey, aAltKey, aShiftKey,
                             aMetaKey, aButton, aRelatedTarget);

  WidgetMouseEventBase* mouseEventBase = mEvent->AsMouseEventBase();
  mouseEventBase->pressure = aPressure;
  mouseEventBase->inputSource = aInputSource;
}

NS_IMETHODIMP
MouseEvent::GetButton(int16_t* aButton)
{
  NS_ENSURE_ARG_POINTER(aButton);
  *aButton = Button();
  return NS_OK;
}

int16_t
MouseEvent::Button()
{
  switch(mEvent->mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case ePointerEventClass:
    case eSimpleGestureEventClass:
      return mEvent->AsMouseEventBase()->button;
    default:
      NS_WARNING("Tried to get mouse button for non-mouse event!");
      return WidgetMouseEvent::eLeftButton;
  }
}

NS_IMETHODIMP
MouseEvent::GetButtons(uint16_t* aButtons)
{
  NS_ENSURE_ARG_POINTER(aButtons);
  *aButtons = Buttons();
  return NS_OK;
}

uint16_t
MouseEvent::Buttons()
{
  switch(mEvent->mClass) {
    case eMouseEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eDragEventClass:
    case ePointerEventClass:
    case eSimpleGestureEventClass:
      return mEvent->AsMouseEventBase()->buttons;
    default:
      MOZ_CRASH("Tried to get mouse buttons for non-mouse event!");
  }
}

NS_IMETHODIMP
MouseEvent::GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget)
{
  NS_ENSURE_ARG_POINTER(aRelatedTarget);
  *aRelatedTarget = GetRelatedTarget().take();
  return NS_OK;
}

already_AddRefed<EventTarget>
MouseEvent::GetRelatedTarget()
{
  nsCOMPtr<EventTarget> relatedTarget;
  switch(mEvent->mClass) {
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

void
MouseEvent::GetRegion(nsAString& aRegion)
{
  SetDOMStringToNull(aRegion);
  WidgetMouseEventBase* mouseEventBase = mEvent->AsMouseEventBase();
  if (mouseEventBase) {
    aRegion = mouseEventBase->region;
  }
}

NS_IMETHODIMP
MouseEvent::GetMozMovementX(int32_t* aMovementX)
{
  NS_ENSURE_ARG_POINTER(aMovementX);
  *aMovementX = MovementX();

  return NS_OK;
}

NS_IMETHODIMP
MouseEvent::GetMozMovementY(int32_t* aMovementY)
{
  NS_ENSURE_ARG_POINTER(aMovementY);
  *aMovementY = MovementY();

  return NS_OK;
}

NS_IMETHODIMP
MouseEvent::GetScreenX(int32_t* aScreenX)
{
  NS_ENSURE_ARG_POINTER(aScreenX);
  *aScreenX = ScreenX(CallerType::System);
  return NS_OK;
}

int32_t
MouseEvent::ScreenX(CallerType aCallerType)
{
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    // Sanitize to something sort of like client cooords, but not quite
    // (defaulting to (0,0) instead of our pre-specified client coords).
    return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                  CSSIntPoint(0, 0)).x;
  }

  return Event::GetScreenCoords(mPresContext, mEvent, mEvent->mRefPoint).x;
}

NS_IMETHODIMP
MouseEvent::GetScreenY(int32_t* aScreenY)
{
  NS_ENSURE_ARG_POINTER(aScreenY);
  *aScreenY = ScreenY(CallerType::System);
  return NS_OK;
}

int32_t
MouseEvent::ScreenY(CallerType aCallerType)
{
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    // Sanitize to something sort of like client cooords, but not quite
    // (defaulting to (0,0) instead of our pre-specified client coords).
    return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                  CSSIntPoint(0, 0)).y;
  }

  return Event::GetScreenCoords(mPresContext, mEvent, mEvent->mRefPoint).y;
}


NS_IMETHODIMP
MouseEvent::GetClientX(int32_t* aClientX)
{
  NS_ENSURE_ARG_POINTER(aClientX);
  *aClientX = ClientX();
  return NS_OK;
}

int32_t
MouseEvent::ClientX()
{
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mClientPoint).x;
}

NS_IMETHODIMP
MouseEvent::GetClientY(int32_t* aClientY)
{
  NS_ENSURE_ARG_POINTER(aClientY);
  *aClientY = ClientY();
  return NS_OK;
}

int32_t
MouseEvent::ClientY()
{
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }

  return Event::GetClientCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mClientPoint).y;
}

int32_t
MouseEvent::OffsetX()
{
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }
  return Event::GetOffsetCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mClientPoint).x;
}

int32_t
MouseEvent::OffsetY()
{
  if (mEvent->mFlags.mIsPositionless) {
    return 0;
  }
  return Event::GetOffsetCoords(mPresContext, mEvent, mEvent->mRefPoint,
                                mClientPoint).y;
}

bool
MouseEvent::AltKey()
{
  return mEvent->AsInputEvent()->IsAlt();
}

NS_IMETHODIMP
MouseEvent::GetAltKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = AltKey();
  return NS_OK;
}

bool
MouseEvent::CtrlKey()
{
  return mEvent->AsInputEvent()->IsControl();
}

NS_IMETHODIMP
MouseEvent::GetCtrlKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = CtrlKey();
  return NS_OK;
}

bool
MouseEvent::ShiftKey()
{
  return mEvent->AsInputEvent()->IsShift();
}

NS_IMETHODIMP
MouseEvent::GetShiftKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ShiftKey();
  return NS_OK;
}

bool
MouseEvent::MetaKey()
{
  return mEvent->AsInputEvent()->IsMeta();
}

NS_IMETHODIMP
MouseEvent::GetMetaKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = MetaKey();
  return NS_OK;
}

NS_IMETHODIMP
MouseEvent::GetModifierState(const nsAString& aKey,
                                  bool* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  *aState = GetModifierState(aKey);
  return NS_OK;
}

float
MouseEvent::MozPressure() const
{
  return mEvent->AsMouseEventBase()->pressure;
}

NS_IMETHODIMP
MouseEvent::GetMozPressure(float* aPressure)
{
  NS_ENSURE_ARG_POINTER(aPressure);
  *aPressure = MozPressure();
  return NS_OK;
}

bool
MouseEvent::HitCluster() const
{
  return mEvent->AsMouseEventBase()->hitCluster;
}

uint16_t
MouseEvent::MozInputSource() const
{
  return mEvent->AsMouseEventBase()->inputSource;
}

NS_IMETHODIMP
MouseEvent::GetMozInputSource(uint16_t* aInputSource)
{
  NS_ENSURE_ARG_POINTER(aInputSource);
  *aInputSource = MozInputSource();
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<MouseEvent>
NS_NewDOMMouseEvent(EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    WidgetMouseEvent* aEvent)
{
  RefPtr<MouseEvent> it = new MouseEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
