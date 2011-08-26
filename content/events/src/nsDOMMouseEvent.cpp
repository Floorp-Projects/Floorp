/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Clark (buster@netscape.com)
 *   Ilya Konstantinov (mozilla-code@future.shiny.co.il)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDOMMouseEvent.h"
#include "nsGUIEvent.h"
#include "nsIContent.h"
#include "nsContentUtils.h"

nsDOMMouseEvent::nsDOMMouseEvent(nsPresContext* aPresContext,
                                 nsInputEvent* aEvent)
  : nsDOMUIEvent(aPresContext, aEvent ? aEvent :
                 new nsMouseEvent(PR_FALSE, 0, nsnull,
                                  nsMouseEvent::eReal))
{
  // There's no way to make this class' ctor allocate an nsMouseScrollEvent.
  // It's not that important, though, since a scroll event is not a real
  // DOM event.
  
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  }
  else {
    mEventIsInternal = PR_TRUE;
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
nsDOMMouseEvent::InitMouseEvent(const nsAString & aType, PRBool aCanBubble, PRBool aCancelable,
                                nsIDOMWindow* aView, PRInt32 aDetail, PRInt32 aScreenX, 
                                PRInt32 aScreenY, PRInt32 aClientX, PRInt32 aClientY, 
                                PRBool aCtrlKey, PRBool aAltKey, PRBool aShiftKey, 
                                PRBool aMetaKey, PRUint16 aButton, nsIDOMEventTarget *aRelatedTarget)
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
       inputEvent->isControl = aCtrlKey;
       inputEvent->isAlt = aAltKey;
       inputEvent->isShift = aShiftKey;
       inputEvent->isMeta = aMetaKey;
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

NS_IMETHODIMP
nsDOMMouseEvent::InitNSMouseEvent(const nsAString & aType, PRBool aCanBubble, PRBool aCancelable,
                                  nsIDOMWindow *aView, PRInt32 aDetail, PRInt32 aScreenX,
                                  PRInt32 aScreenY, PRInt32 aClientX, PRInt32 aClientY,
                                  PRBool aCtrlKey, PRBool aAltKey, PRBool aShiftKey,
                                  PRBool aMetaKey, PRUint16 aButton, nsIDOMEventTarget *aRelatedTarget,
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

NS_METHOD nsDOMMouseEvent::GetScreenX(PRInt32* aScreenX)
{
  NS_ENSURE_ARG_POINTER(aScreenX);
  *aScreenX = GetScreenPoint().x;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetScreenY(PRInt32* aScreenY)
{
  NS_ENSURE_ARG_POINTER(aScreenY);
  *aScreenY = GetScreenPoint().y;
  return NS_OK;
}


NS_METHOD nsDOMMouseEvent::GetClientX(PRInt32* aClientX)
{
  NS_ENSURE_ARG_POINTER(aClientX);
  *aClientX = GetClientPoint().x;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetClientY(PRInt32* aClientY)
{
  NS_ENSURE_ARG_POINTER(aClientY);
  *aClientY = GetClientPoint().y;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetAltKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ((nsInputEvent*)mEvent)->isAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetCtrlKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ((nsInputEvent*)mEvent)->isControl;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetShiftKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ((nsInputEvent*)mEvent)->isShift;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetMetaKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = ((nsInputEvent*)mEvent)->isMeta;
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
