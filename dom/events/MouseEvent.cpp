/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
            aEvent ? aEvent : new WidgetMouseEvent(false, 0, nullptr,
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
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    mouseEvent->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }

  if (mouseEvent) {
    MOZ_ASSERT(mouseEvent->reason != WidgetMouseEvent::eSynthesized,
               "Don't dispatch DOM events from synthesized mouse events");
    mDetail = mouseEvent->clickCount;
  }
}

NS_IMPL_ADDREF_INHERITED(MouseEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(MouseEvent, UIEvent)

NS_INTERFACE_MAP_BEGIN(MouseEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

NS_IMETHODIMP
MouseEvent::InitMouseEvent(const nsAString& aType,
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
                           nsIDOMEventTarget* aRelatedTarget)
{
  nsresult rv =
    UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(mEvent->eventStructType) {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_POINTER_EVENT:
    case NS_SIMPLE_GESTURE_EVENT: {
      WidgetMouseEventBase* mouseEventBase = mEvent->AsMouseEventBase();
      mouseEventBase->relatedTarget = aRelatedTarget;
      mouseEventBase->button = aButton;
      mouseEventBase->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey, aMetaKey);
      mClientPoint.x = aClientX;
      mClientPoint.y = aClientY;
      mouseEventBase->refPoint.x = aScreenX;
      mouseEventBase->refPoint.y = aScreenY;

      WidgetMouseEvent* mouseEvent = mEvent->AsMouseEvent();
      if (mouseEvent) {
        mouseEvent->clickCount = aDetail;
      }
      break;
    }
    default:
       break;
  }

  return NS_OK;
}   

nsresult
MouseEvent::InitMouseEvent(const nsAString& aType,
                           bool aCanBubble,
                           bool aCancelable,
                           nsIDOMWindow* aView,
                           int32_t aDetail,
                           int32_t aScreenX,
                           int32_t aScreenY,
                           int32_t aClientX,
                           int32_t aClientY,
                           int16_t aButton,
                           nsIDOMEventTarget* aRelatedTarget,
                           const nsAString& aModifiersList)
{
  Modifiers modifiers = ComputeModifierState(aModifiersList);

  nsresult rv = InitMouseEvent(aType, aCanBubble, aCancelable, aView,
                               aDetail, aScreenX, aScreenY, aClientX, aClientY,
                               (modifiers & MODIFIER_CONTROL) != 0,
                               (modifiers & MODIFIER_ALT) != 0,
                               (modifiers & MODIFIER_SHIFT) != 0,
                               (modifiers & MODIFIER_META) != 0,
                               aButton, aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(mEvent->eventStructType) {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_POINTER_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
      mEvent->AsInputEvent()->modifiers = modifiers;
      return NS_OK;
    default:
      MOZ_CRASH("There is no space to store the modifiers");
  }
}

already_AddRefed<MouseEvent>
MouseEvent::Constructor(const GlobalObject& aGlobal,
                        const nsAString& aType,
                        const MouseEventInit& aParam,
                        ErrorResult& aRv)
{
  nsCOMPtr<EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<MouseEvent> e = new MouseEvent(t, nullptr, nullptr);
  bool trusted = e->Init(t);
  e->InitMouseEvent(aType, aParam.mBubbles, aParam.mCancelable,
                    aParam.mView, aParam.mDetail, aParam.mScreenX,
                    aParam.mScreenY, aParam.mClientX, aParam.mClientY,
                    aParam.mCtrlKey, aParam.mAltKey, aParam.mShiftKey,
                    aParam.mMetaKey, aParam.mButton, aParam.mRelatedTarget,
                    aRv);
  e->SetTrusted(trusted);

  switch (e->mEvent->eventStructType) {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_POINTER_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
      e->mEvent->AsMouseEventBase()->buttons = aParam.mButtons;
      break;
    default:
      break;
  }

  return e.forget();
}

NS_IMETHODIMP
MouseEvent::InitNSMouseEvent(const nsAString& aType,
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
                             float aPressure,
                             uint16_t aInputSource)
{
  nsresult rv = MouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                                           aView, aDetail, aScreenX, aScreenY,
                                           aClientX, aClientY,
                                           aCtrlKey, aAltKey, aShiftKey,
                                           aMetaKey, aButton, aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  WidgetMouseEventBase* mouseEventBase = mEvent->AsMouseEventBase();
  mouseEventBase->pressure = aPressure;
  mouseEventBase->inputSource = aInputSource;
  return NS_OK;
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
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_POINTER_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
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
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_POINTER_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
      return mEvent->AsMouseEventBase()->buttons;
    default:
      MOZ_CRASH("Tried to get mouse buttons for non-mouse event!");
  }
}

NS_IMETHODIMP
MouseEvent::GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget)
{
  NS_ENSURE_ARG_POINTER(aRelatedTarget);
  *aRelatedTarget = GetRelatedTarget().get();
  return NS_OK;
}

already_AddRefed<EventTarget>
MouseEvent::GetRelatedTarget()
{
  nsCOMPtr<EventTarget> relatedTarget;
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_POINTER_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
      relatedTarget =
        do_QueryInterface(mEvent->AsMouseEventBase()->relatedTarget);
      break;
    default:
      break;
  }

  if (relatedTarget) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(relatedTarget);
    if (content && content->ChromeOnlyAccess() &&
        !nsContentUtils::CanAccessNativeAnon()) {
      relatedTarget = do_QueryInterface(content->FindFirstNonChromeOnlyAccessContent());
    }

    if (relatedTarget) {
      relatedTarget = relatedTarget->GetTargetForDOMEvent();
    }
    return relatedTarget.forget();
  }
  return nullptr;
}

NS_IMETHODIMP
MouseEvent::GetMozMovementX(int32_t* aMovementX)
{
  NS_ENSURE_ARG_POINTER(aMovementX);
  *aMovementX = MozMovementX();

  return NS_OK;
}

NS_IMETHODIMP
MouseEvent::GetMozMovementY(int32_t* aMovementY)
{
  NS_ENSURE_ARG_POINTER(aMovementY);
  *aMovementY = MozMovementY();

  return NS_OK;
}

NS_IMETHODIMP
MouseEvent::GetScreenX(int32_t* aScreenX)
{
  NS_ENSURE_ARG_POINTER(aScreenX);
  *aScreenX = ScreenX();
  return NS_OK;
}

int32_t
MouseEvent::ScreenX()
{
  return Event::GetScreenCoords(mPresContext, mEvent, mEvent->refPoint).x;
}

NS_IMETHODIMP
MouseEvent::GetScreenY(int32_t* aScreenY)
{
  NS_ENSURE_ARG_POINTER(aScreenY);
  *aScreenY = ScreenY();
  return NS_OK;
}

int32_t
MouseEvent::ScreenY()
{
  return Event::GetScreenCoords(mPresContext, mEvent, mEvent->refPoint).y;
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
  return Event::GetClientCoords(mPresContext, mEvent, mEvent->refPoint,
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
  return Event::GetClientCoords(mPresContext, mEvent, mEvent->refPoint,
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

nsresult
NS_NewDOMMouseEvent(nsIDOMEvent** aInstancePtrResult,
                    EventTarget* aOwner,
                    nsPresContext* aPresContext,
                    WidgetMouseEvent* aEvent)
{
  MouseEvent* it = new MouseEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
