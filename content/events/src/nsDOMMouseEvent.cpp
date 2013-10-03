/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMouseEvent.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "prtime.h"
#include "mozilla/MouseEvents.h"

using namespace mozilla;

nsDOMMouseEvent::nsDOMMouseEvent(mozilla::dom::EventTarget* aOwner,
                                 nsPresContext* aPresContext,
                                 WidgetInputEvent* aEvent)
  : nsDOMUIEvent(aOwner, aPresContext, aEvent ? aEvent :
                 new WidgetMouseEvent(false, 0, nullptr,
                                      WidgetMouseEvent::eReal))
{
  // There's no way to make this class' ctor allocate an WidgetMouseScrollEvent.
  // It's not that important, though, since a scroll event is not a real
  // DOM event.
  
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<WidgetMouseEvent*>(mEvent)->inputSource =
      nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }

  switch (mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
      NS_ASSERTION(static_cast<WidgetMouseEvent*>(mEvent)->reason
                     != WidgetMouseEvent::eSynthesized,
                   "Don't dispatch DOM events from synthesized mouse events");
      mDetail = static_cast<WidgetMouseEvent*>(mEvent)->clickCount;
      break;
    default:
      break;
  }
}

nsDOMMouseEvent::~nsDOMMouseEvent()
{
  if (mEventIsInternal && mEvent) {
    switch (mEvent->eventStructType)
    {
      case NS_MOUSE_EVENT:
        delete static_cast<WidgetMouseEvent*>(mEvent);
        break;
      default:
        delete mEvent;
        break;
    }
    mEvent = nullptr;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMMouseEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMouseEvent, nsDOMUIEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMouseEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMMouseEvent::InitMouseEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                                nsIDOMWindow* aView, int32_t aDetail, int32_t aScreenX, 
                                int32_t aScreenY, int32_t aClientX, int32_t aClientY, 
                                bool aCtrlKey, bool aAltKey, bool aShiftKey, 
                                bool aMetaKey, uint16_t aButton, nsIDOMEventTarget *aRelatedTarget)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
    {
       static_cast<WidgetMouseEventBase*>(mEvent)->relatedTarget =
         aRelatedTarget;
       static_cast<WidgetMouseEventBase*>(mEvent)->button = aButton;
       WidgetInputEvent* inputEvent = static_cast<WidgetInputEvent*>(mEvent);
       inputEvent->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey, aMetaKey);
       mClientPoint.x = aClientX;
       mClientPoint.y = aClientY;
       inputEvent->refPoint.x = aScreenX;
       inputEvent->refPoint.y = aScreenY;

       if (mEvent->eventStructType == NS_MOUSE_EVENT) {
         WidgetMouseEvent* mouseEvent = static_cast<WidgetMouseEvent*>(mEvent);
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
nsDOMMouseEvent::InitMouseEvent(const nsAString& aType,
                                bool aCanBubble,
                                bool aCancelable,
                                nsIDOMWindow* aView,
                                int32_t aDetail,
                                int32_t aScreenX,
                                int32_t aScreenY,
                                int32_t aClientX,
                                int32_t aClientY,
                                uint16_t aButton,
                                nsIDOMEventTarget *aRelatedTarget,
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
    case NS_SIMPLE_GESTURE_EVENT:
      static_cast<WidgetInputEvent*>(mEvent)->modifiers = modifiers;
      return NS_OK;
    default:
      MOZ_CRASH("There is no space to store the modifiers");
  }
}

already_AddRefed<nsDOMMouseEvent>
nsDOMMouseEvent::Constructor(const mozilla::dom::GlobalObject& aGlobal,
                             const nsAString& aType,
                             const mozilla::dom::MouseEventInit& aParam,
                             mozilla::ErrorResult& aRv)
{
  nsCOMPtr<mozilla::dom::EventTarget> t = do_QueryInterface(aGlobal.GetAsSupports());
  nsRefPtr<nsDOMMouseEvent> e = new nsDOMMouseEvent(t, nullptr, nullptr);
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
    case NS_SIMPLE_GESTURE_EVENT:
      static_cast<WidgetMouseEventBase*>(e->mEvent)->buttons = aParam.mButtons;
      break;
    default:
      break;
  }

  return e.forget();
}

NS_IMETHODIMP
nsDOMMouseEvent::InitNSMouseEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                                  nsIDOMWindow *aView, int32_t aDetail, int32_t aScreenX,
                                  int32_t aScreenY, int32_t aClientX, int32_t aClientY,
                                  bool aCtrlKey, bool aAltKey, bool aShiftKey,
                                  bool aMetaKey, uint16_t aButton, nsIDOMEventTarget *aRelatedTarget,
                                  float aPressure, uint16_t aInputSource)
{
  nsresult rv = nsDOMMouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                                                aView, aDetail, aScreenX, aScreenY,
                                                aClientX, aClientY, aCtrlKey, aAltKey, aShiftKey,
                                                aMetaKey, aButton, aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  static_cast<WidgetMouseEventBase*>(mEvent)->pressure = aPressure;
  static_cast<WidgetMouseEventBase*>(mEvent)->inputSource = aInputSource;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetButton(uint16_t* aButton)
{
  NS_ENSURE_ARG_POINTER(aButton);
  *aButton = Button();
  return NS_OK;
}

uint16_t
nsDOMMouseEvent::Button()
{
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
      return static_cast<WidgetMouseEventBase*>(mEvent)->button;
    default:
      NS_WARNING("Tried to get mouse button for non-mouse event!");
      return WidgetMouseEvent::eLeftButton;
  }
}

NS_IMETHODIMP
nsDOMMouseEvent::GetButtons(uint16_t* aButtons)
{
  NS_ENSURE_ARG_POINTER(aButtons);
  *aButtons = Buttons();
  return NS_OK;
}

uint16_t
nsDOMMouseEvent::Buttons()
{
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
      return static_cast<WidgetMouseEventBase*>(mEvent)->buttons;
    default:
      MOZ_CRASH("Tried to get mouse buttons for non-mouse event!");
  }
}

NS_IMETHODIMP
nsDOMMouseEvent::GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget)
{
  NS_ENSURE_ARG_POINTER(aRelatedTarget);
  *aRelatedTarget = GetRelatedTarget().get();
  return NS_OK;
}

already_AddRefed<mozilla::dom::EventTarget>
nsDOMMouseEvent::GetRelatedTarget()
{
  nsCOMPtr<mozilla::dom::EventTarget> relatedTarget;
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
      relatedTarget = do_QueryInterface(
        static_cast<WidgetMouseEventBase*>(mEvent)->relatedTarget);
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
nsDOMMouseEvent::GetMozMovementX(int32_t* aMovementX)
{
  NS_ENSURE_ARG_POINTER(aMovementX);
  *aMovementX = MozMovementX();

  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMozMovementY(int32_t* aMovementY)
{
  NS_ENSURE_ARG_POINTER(aMovementY);
  *aMovementY = MozMovementY();

  return NS_OK;
}

NS_METHOD nsDOMMouseEvent::GetScreenX(int32_t* aScreenX)
{
  NS_ENSURE_ARG_POINTER(aScreenX);
  *aScreenX = ScreenX();
  return NS_OK;
}

int32_t
nsDOMMouseEvent::ScreenX()
{
  return nsDOMEvent::GetScreenCoords(mPresContext,
                                     mEvent,
                                     mEvent->refPoint).x;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetScreenY(int32_t* aScreenY)
{
  NS_ENSURE_ARG_POINTER(aScreenY);
  *aScreenY = ScreenY();
  return NS_OK;
}

int32_t
nsDOMMouseEvent::ScreenY()
{
  return nsDOMEvent::GetScreenCoords(mPresContext,
                                     mEvent,
                                     mEvent->refPoint).y;
}


NS_METHOD nsDOMMouseEvent::GetClientX(int32_t* aClientX)
{
  NS_ENSURE_ARG_POINTER(aClientX);
  *aClientX = ClientX();
  return NS_OK;
}

int32_t
nsDOMMouseEvent::ClientX()
{
  return nsDOMEvent::GetClientCoords(mPresContext,
                                     mEvent,
                                     mEvent->refPoint,
                                     mClientPoint).x;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetClientY(int32_t* aClientY)
{
  NS_ENSURE_ARG_POINTER(aClientY);
  *aClientY = ClientY();
  return NS_OK;
}

int32_t
nsDOMMouseEvent::ClientY()
{
  return nsDOMEvent::GetClientCoords(mPresContext,
                                     mEvent,
                                     mEvent->refPoint,
                                     mClientPoint).y;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetAltKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = AltKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetCtrlKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = CtrlKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetShiftKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ShiftKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMetaKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = MetaKey();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetModifierState(const nsAString& aKey,
                                  bool* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  *aState = GetModifierState(aKey);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMozPressure(float* aPressure)
{
  NS_ENSURE_ARG_POINTER(aPressure);
  *aPressure = MozPressure();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMozInputSource(uint16_t* aInputSource)
{
  NS_ENSURE_ARG_POINTER(aInputSource);
  *aInputSource = MozInputSource();
  return NS_OK;
}

nsresult NS_NewDOMMouseEvent(nsIDOMEvent** aInstancePtrResult,
                             mozilla::dom::EventTarget* aOwner,
                             nsPresContext* aPresContext,
                             WidgetInputEvent* aEvent)
{
  nsDOMMouseEvent* it = new nsDOMMouseEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
