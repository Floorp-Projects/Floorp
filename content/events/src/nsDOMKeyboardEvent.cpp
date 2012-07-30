/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMKeyboardEvent.h"
#include "nsDOMClassInfoID.h"

nsDOMKeyboardEvent::nsDOMKeyboardEvent(nsPresContext* aPresContext,
                                       nsKeyEvent* aEvent)
  : nsDOMUIEvent(aPresContext, aEvent ? aEvent :
                 new nsKeyEvent(false, 0, nullptr))
{
  NS_ASSERTION(mEvent->eventStructType == NS_KEY_EVENT, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  }
  else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();
  }
}

nsDOMKeyboardEvent::~nsDOMKeyboardEvent()
{
  if (mEventIsInternal) {
    delete static_cast<nsKeyEvent*>(mEvent);
    mEvent = nullptr;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMKeyboardEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMKeyboardEvent, nsDOMUIEvent)

DOMCI_DATA(KeyboardEvent, nsDOMKeyboardEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMKeyboardEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(KeyboardEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMKeyboardEvent::GetAltKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = static_cast<nsInputEvent*>(mEvent)->IsAlt();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetCtrlKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = static_cast<nsInputEvent*>(mEvent)->IsControl();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetShiftKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = static_cast<nsInputEvent*>(mEvent)->IsShift();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetMetaKey(bool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = static_cast<nsInputEvent*>(mEvent)->IsMeta();
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetModifierState(const nsAString& aKey,
                                     bool* aState)
{
  NS_ENSURE_ARG_POINTER(aState);

  *aState = GetModifierStateInternal(aKey);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetCharCode(PRUint32* aCharCode)
{
  NS_ENSURE_ARG_POINTER(aCharCode);

  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
    *aCharCode = 0;
    break;
  case NS_KEY_PRESS:
    *aCharCode = ((nsKeyEvent*)mEvent)->charCode;
    break;
  default:
    *aCharCode = 0;
    break;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetKeyCode(PRUint32* aKeyCode)
{
  NS_ENSURE_ARG_POINTER(aKeyCode);

  switch (mEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_PRESS:
  case NS_KEY_DOWN:
    *aKeyCode = ((nsKeyEvent*)mEvent)->keyCode;
    break;
  default:
    *aKeyCode = 0;
    break;
  }

  return NS_OK;
}

/* virtual */
nsresult
nsDOMKeyboardEvent::Which(PRUint32* aWhich)
{
  NS_ENSURE_ARG_POINTER(aWhich);

  switch (mEvent->message) {
    case NS_KEY_UP:
    case NS_KEY_DOWN:
      return GetKeyCode(aWhich);
    case NS_KEY_PRESS:
      //Special case for 4xp bug 62878.  Try to make value of which
      //more closely mirror the values that 4.x gave for RETURN and BACKSPACE
      {
        PRUint32 keyCode = ((nsKeyEvent*)mEvent)->keyCode;
        if (keyCode == NS_VK_RETURN || keyCode == NS_VK_BACK) {
          *aWhich = keyCode;
          return NS_OK;
        }
        return GetCharCode(aWhich);
      }
      break;
    default:
      *aWhich = 0;
      break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::GetLocation(PRUint32* aLocation)
{
  NS_ENSURE_ARG_POINTER(aLocation);

  *aLocation = static_cast<nsKeyEvent*>(mEvent)->location;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMKeyboardEvent::InitKeyEvent(const nsAString& aType, bool aCanBubble, bool aCancelable,
                                 nsIDOMWindow* aView, bool aCtrlKey, bool aAltKey,
                                 bool aShiftKey, bool aMetaKey,
                                 PRUint32 aKeyCode, PRUint32 aCharCode)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  nsKeyEvent* keyEvent = static_cast<nsKeyEvent*>(mEvent);
  keyEvent->InitBasicModifiers(aCtrlKey, aAltKey, aShiftKey, aMetaKey);
  keyEvent->keyCode = aKeyCode;
  keyEvent->charCode = aCharCode;

  return NS_OK;
}

nsresult NS_NewDOMKeyboardEvent(nsIDOMEvent** aInstancePtrResult,
                                nsPresContext* aPresContext,
                                nsKeyEvent *aEvent)
{
  nsDOMKeyboardEvent* it = new nsDOMKeyboardEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
