/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMMouseEvent.h"
#include "nsGUIEvent.h"
#include "nsIContent.h"
#include "nsContentUtils.h"
#include "DictionaryHelpers.h"
#include "nsDOMClassInfoID.h"

using namespace mozilla;

nsDOMMouseEvent::nsDOMMouseEvent(nsPresContext* aPresContext,
                                 nsInputEvent* aEvent)
  : nsDOMUIEvent(aPresContext, aEvent ? aEvent :
                 new nsMouseEvent(false, 0, nsnull,
                                  nsMouseEvent::eReal))
{
  // There's no way to make this class' ctor allocate an nsMouseScrollEvent.
  // It's not that important, though, since a scroll event is not a real
  // DOM event.
  
  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<nsMouseEvent*>(mEvent)->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }

  switch (mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
      NS_ASSERTION(static_cast<nsMouseEvent*>(mEvent)->reason
                   != nsMouseEvent::eSynthesized,
                   "Don't dispatch DOM events from synthesized mouse events");
      mDetail = static_cast<nsMouseEvent*>(mEvent)->clickCount;
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
        delete static_cast<nsMouseEvent*>(mEvent);
        break;
      default:
        delete mEvent;
        break;
    }
    mEvent = nsnull;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMMouseEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMouseEvent, nsDOMUIEvent)

DOMCI_DATA(MouseEvent, nsDOMMouseEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMouseEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MouseEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMMouseEvent::InitMouseEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                                nsIDOMWindow* aView, PRInt32 aDetail, PRInt32 aScreenX, 
                                PRInt32 aScreenY, PRInt32 aClientX, PRInt32 aClientY, 
                                bool aCtrlKey, bool aAltKey, bool aShiftKey, 
                                bool aMetaKey, PRUint16 aButton, nsIDOMEventTarget *aRelatedTarget)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
    case NS_MOZTOUCH_EVENT:
    {
       static_cast<nsMouseEvent_base*>(mEvent)->relatedTarget = aRelatedTarget;
       static_cast<nsMouseEvent_base*>(mEvent)->button = aButton;
       nsInputEvent* inputEvent = static_cast<nsInputEvent*>(mEvent);
       inputEvent->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey, aMetaKey);
       mClientPoint.x = aClientX;
       mClientPoint.y = aClientY;
       inputEvent->refPoint.x = aScreenX;
       inputEvent->refPoint.y = aScreenY;

       if (mEvent->eventStructType == NS_MOUSE_EVENT) {
         nsMouseEvent* mouseEvent = static_cast<nsMouseEvent*>(mEvent);
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
                                PRInt32 aDetail,
                                PRInt32 aScreenX,
                                PRInt32 aScreenY,
                                PRInt32 aClientX,
                                PRInt32 aClientY,
                                PRUint16 aButton,
                                nsIDOMEventTarget *aRelatedTarget,
                                const nsAString& aModifiersList)
{
  Modifiers modifiers = ComputeModifierState(aModifiersList);

  nsresult rv = InitMouseEvent(aType, aCanBubble, aCancelable, aView,
                               aDetail, aScreenX, aScreenY, aClientX, aClientY,
                               (modifiers & widget::MODIFIER_CONTROL) != 0,
                               (modifiers & widget::MODIFIER_ALT) != 0,
                               (modifiers & widget::MODIFIER_SHIFT) != 0,
                               (modifiers & widget::MODIFIER_META) != 0,
                               aButton, aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(mEvent->eventStructType) {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
    case NS_MOZTOUCH_EVENT:
      static_cast<nsInputEvent*>(mEvent)->modifiers = modifiers;
      return NS_OK;
    default:
      MOZ_NOT_REACHED("There is no space to store the modifiers");
      return NS_ERROR_FAILURE;
  }
}

nsresult
nsDOMMouseEvent::InitFromCtor(const nsAString& aType,
                              JSContext* aCx, jsval* aVal)
{
  mozilla::dom::MouseEventInit d;
  nsresult rv = d.Init(aCx, aVal);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = InitMouseEvent(aType, d.bubbles, d.cancelable,
                      d.view, d.detail, d.screenX, d.screenY,
                      d.clientX, d.clientY, 
                      d.ctrlKey, d.altKey, d.shiftKey, d.metaKey,
                      d.button, d.relatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  switch(mEvent->eventStructType) {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
    case NS_MOZTOUCH_EVENT:
      static_cast<nsMouseEvent_base*>(mEvent)->buttons = d.buttons;
      break;
    default:
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::InitNSMouseEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                                  nsIDOMWindow *aView, PRInt32 aDetail, PRInt32 aScreenX,
                                  PRInt32 aScreenY, PRInt32 aClientX, PRInt32 aClientY,
                                  bool aCtrlKey, bool aAltKey, bool aShiftKey,
                                  bool aMetaKey, PRUint16 aButton, nsIDOMEventTarget *aRelatedTarget,
                                  float aPressure, PRUint16 aInputSource)
{
  nsresult rv = nsDOMMouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable,
                                                aView, aDetail, aScreenX, aScreenY,
                                                aClientX, aClientY, aCtrlKey, aAltKey, aShiftKey,
                                                aMetaKey, aButton, aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  static_cast<nsMouseEvent_base*>(mEvent)->pressure = aPressure;
  static_cast<nsMouseEvent_base*>(mEvent)->inputSource = aInputSource;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetButton(PRUint16* aButton)
{
  NS_ENSURE_ARG_POINTER(aButton);
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
    case NS_MOZTOUCH_EVENT:
      *aButton = static_cast<nsMouseEvent_base*>(mEvent)->button;
      break;
    default:
      NS_WARNING("Tried to get mouse button for non-mouse event!");
      *aButton = nsMouseEvent::eLeftButton;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetButtons(PRUint16* aButtons)
{
  NS_ENSURE_ARG_POINTER(aButtons);
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
    case NS_MOZTOUCH_EVENT:
      *aButtons = static_cast<nsMouseEvent_base*>(mEvent)->buttons;
      break;
    default:
      MOZ_NOT_REACHED("Tried to get mouse buttons for non-mouse event!");
      *aButtons = 0;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget)
{
  NS_ENSURE_ARG_POINTER(aRelatedTarget);
  *aRelatedTarget = nsnull;
  nsISupports* relatedTarget = nsnull;
  switch(mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
    case NS_MOZTOUCH_EVENT:
      relatedTarget = static_cast<nsMouseEvent_base*>(mEvent)->relatedTarget;
      break;
    default:
      break;
  }

  if (relatedTarget) {
    nsCOMPtr<nsIContent> content = do_QueryInterface(relatedTarget);
    if (content && content->IsInNativeAnonymousSubtree() &&
        !nsContentUtils::CanAccessNativeAnon()) {
      relatedTarget = content->FindFirstNonNativeAnonymous();
      if (!relatedTarget) {
        return NS_OK;
      }
    }

    CallQueryInterface(relatedTarget, aRelatedTarget);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMozMovementX(PRInt32* aMovementX)
{
  NS_ENSURE_ARG_POINTER(aMovementX);
  *aMovementX = GetMovementPoint().x;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMozMovementY(PRInt32* aMovementY)
{
  NS_ENSURE_ARG_POINTER(aMovementY);
  *aMovementY = GetMovementPoint().y;

  return NS_OK;
}

NS_METHOD nsDOMMouseEvent::GetScreenX(PRInt32* aScreenX)
{
  NS_ENSURE_ARG_POINTER(aScreenX);
  *aScreenX = nsDOMEvent::GetScreenCoords(mPresContext,
                                          mEvent,
                                          mEvent->refPoint).x;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetScreenY(PRInt32* aScreenY)
{
  NS_ENSURE_ARG_POINTER(aScreenY);
  *aScreenY = nsDOMEvent::GetScreenCoords(mPresContext,
                                          mEvent,
                                          mEvent->refPoint).y;
  return NS_OK;
}


NS_METHOD nsDOMMouseEvent::GetClientX(PRInt32* aClientX)
{
  NS_ENSURE_ARG_POINTER(aClientX);
  *aClientX = nsDOMEvent::GetClientCoords(mPresContext,
                                          mEvent,
                                          mEvent->refPoint,
                                          mClientPoint).x;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetClientY(PRInt32* aClientY)
{
  NS_ENSURE_ARG_POINTER(aClientY);
  *aClientY = nsDOMEvent::GetClientCoords(mPresContext,
                                          mEvent,
                                          mEvent->refPoint,
                                          mClientPoint).y;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetAltKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = static_cast<nsInputEvent*>(mEvent)->IsAlt();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetCtrlKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = static_cast<nsInputEvent*>(mEvent)->IsControl();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetShiftKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = static_cast<nsInputEvent*>(mEvent)->IsShift();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMetaKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = static_cast<nsInputEvent*>(mEvent)->IsMeta();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetModifierState(const nsAString& aKey,
                                  bool* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  *aState = GetModifierStateInternal(aKey);
  return NS_OK;
}

/* virtual */
nsresult
nsDOMMouseEvent::Which(PRUint32* aWhich)
{
  NS_ENSURE_ARG_POINTER(aWhich);
  PRUint16 button;
  (void) GetButton(&button);
  *aWhich = button + 1;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMozPressure(float* aPressure)
{
  NS_ENSURE_ARG_POINTER(aPressure);
  *aPressure = static_cast<nsMouseEvent_base*>(mEvent)->pressure;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMozInputSource(PRUint16* aInputSource)
{
  NS_ENSURE_ARG_POINTER(aInputSource);
  *aInputSource = static_cast<nsMouseEvent_base*>(mEvent)->inputSource;
  return NS_OK;
}

nsresult NS_NewDOMMouseEvent(nsIDOMEvent** aInstancePtrResult,
                             nsPresContext* aPresContext,
                             nsInputEvent *aEvent) 
{
  nsDOMMouseEvent* it = new nsDOMMouseEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
