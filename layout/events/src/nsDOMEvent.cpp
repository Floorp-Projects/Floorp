/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDOMEvent.h"
#include "nsIDOMNode.h"

static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDOMEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kINSEventIID, NS_INSEVENT_IID);

nsDOMEvent::nsDOMEvent() {
}

nsDOMEvent::~nsDOMEvent() {
}

NS_IMPL_ADDREF(nsDOMEvent)
NS_IMPL_RELEASE(nsDOMEvent)

nsresult nsDOMEvent::QueryInterface(const nsIID& aIID,
                                       void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIDOMEventIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMEvent*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kINSEventIID)) {
    *aInstancePtrResult = (void*) ((nsINSEvent*)this);
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

// nsIDOMEventInterface
NS_METHOD nsDOMEvent::GetType(nsString& aType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetTarget(nsIDOMNode** aTarget)
{
  return kTarget->QueryInterface(kIDOMNodeIID, (void**)aTarget);
}

NS_METHOD nsDOMEvent::GetScreenX(PRInt32& aX)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetScreenY(PRInt32& aY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetClientX(PRInt32& aX)
{
  //XXX these are not client coords yet
  aX = kEvent->point.x;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetClientY(PRInt32& aY)
{
  //XXX these are not client coords yet
  aY = kEvent->point.y;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetAltKey(PRBool& aIsDown)
{
  aIsDown = ((nsInputEvent*)kEvent)->isAlt;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetCtrlKey(PRBool& aIsDown)
{
  aIsDown = ((nsInputEvent*)kEvent)->isControl;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetShiftKey(PRBool& aIsDown)
{
  aIsDown = ((nsInputEvent*)kEvent)->isShift;
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetMetaKey(PRBool& aIsDown)
{
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetCharCode(PRUint32& aCharCode)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetKeyCode(PRUint32& aKeyCode)
{
  switch (kEvent->message) {
  case NS_KEY_UP:
  case NS_KEY_DOWN:
    aKeyCode = ((nsKeyEvent*)kEvent)->keyCode;
    break;
  default:
    return NS_ERROR_FAILURE;
    break;
  }
  return NS_OK;
}

NS_METHOD nsDOMEvent::GetButton(PRUint32& aButton)
{
  switch (kEvent->message) {
  case NS_MOUSE_LEFT_BUTTON_UP:
  case NS_MOUSE_LEFT_BUTTON_DOWN:
  case NS_MOUSE_LEFT_DOUBLECLICK:
    aButton = 1;
    break;
  case NS_MOUSE_MIDDLE_BUTTON_UP:
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    aButton = 2;
    break;
  case NS_MOUSE_RIGHT_BUTTON_UP:
  case NS_MOUSE_RIGHT_BUTTON_DOWN:
  case NS_MOUSE_RIGHT_DOUBLECLICK:
    aButton = 3;
    break;
  default:
    return NS_ERROR_FAILURE;
    break;
  }
  return NS_OK;
}

// nsINSEventInterface
NS_METHOD nsDOMEvent::GetLayerX(PRInt32& aX)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::GetLayerY(PRInt32& aY)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD nsDOMEvent::SetGUIEvent(nsGUIEvent *aEvent)
{
  kEvent = aEvent;
  return NS_OK;
}

NS_METHOD nsDOMEvent::SetEventTarget(nsISupports *aTarget)
{
  kTarget = aTarget;
  return NS_OK;
}
