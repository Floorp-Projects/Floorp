/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Steve Clark (buster@netscape.com)
 * Ilya Konstantinov (mozilla-code@future.shiny.co.il)
 *
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
#include "nsIEventStateManager.h"
#include "nsContentUtils.h"

nsDOMMouseEvent::nsDOMMouseEvent(nsPresContext* aPresContext, nsInputEvent* aEvent)
: nsDOMUIEvent(aPresContext, aEvent ? aEvent : new nsMouseEvent()), mButton(-1)
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
    mEvent->refPoint.x = mEvent->refPoint.y = mEvent->point.x = mEvent->point.y = 0;
  }

  switch (mEvent->eventStructType)
  {
    case NS_MOUSE_EVENT:
      mDetail = NS_STATIC_CAST(nsMouseEvent*, mEvent)->clickCount;
      break;
    case NS_MOUSE_SCROLL_EVENT:
      mDetail = NS_STATIC_CAST(nsMouseScrollEvent*, mEvent)->delta;
      break;
    default:
      break;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMMouseEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMouseEvent, nsDOMUIEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMouseEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(MouseEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMMouseEvent::InitMouseEvent(const nsAString & aType, PRBool aCanBubble, PRBool aCancelable,
                                nsIDOMAbstractView *aView, PRInt32 aDetail, PRInt32 aScreenX, 
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
    {
       nsInputEvent* inputEvent = NS_STATIC_CAST(nsInputEvent*, mEvent);
       inputEvent->isControl = aCtrlKey;
       inputEvent->isAlt = aAltKey;
       inputEvent->isShift = aShiftKey;
       inputEvent->isMeta = aMetaKey;
       inputEvent->point.x = aClientX;
       inputEvent->point.y = aClientY;
       inputEvent->refPoint.x = aScreenX;
       inputEvent->refPoint.y = aScreenY;
       mButton = aButton;
       // Now fix up mEvent->message, since nsDOMUIEvent::InitUIEvent
       // doesn't have enough information to set it right.
       // XXXbz AARGH.  No useful constants for the buttons!
       if (mEvent->message == NS_MOUSE_LEFT_CLICK) {
         if (mButton == 1) {  // Middle button
           mEvent->message = NS_MOUSE_MIDDLE_CLICK;
         }
         else if (mButton == 2) {  // Right button
           mEvent->message = NS_MOUSE_RIGHT_CLICK;
         }
       }
       if (mEvent->message == NS_MOUSE_LEFT_DOUBLECLICK) {
         if (mButton == 1) {  // Middle button
           mEvent->message = NS_MOUSE_MIDDLE_DOUBLECLICK;
         }
         else if (mButton == 2) {  // Right button
           mEvent->message = NS_MOUSE_RIGHT_DOUBLECLICK;
         }
       }
       if (mEvent->eventStructType == NS_MOUSE_SCROLL_EVENT) {
         nsMouseScrollEvent* scrollEvent = NS_STATIC_CAST(nsMouseScrollEvent*, mEvent);
         scrollEvent->delta = aDetail;
       } else {
         nsMouseEvent* mouseEvent = NS_STATIC_CAST(nsMouseEvent*, mEvent);
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
nsDOMMouseEvent::GetButton(PRUint16* aButton)
{
  NS_ENSURE_ARG_POINTER(aButton);
  if (!mEvent || mEvent->eventStructType != NS_MOUSE_EVENT) {
    NS_WARNING("Tried to get mouse button for null or non-mouse event!");
    *aButton = (PRUint16)-1;
    return NS_OK;
  }

  // If button has been set then use that instead.
  if (mButton >= 0) {
    *aButton = (PRUint16)mButton;
  }
  else {
    switch (mEvent->message) {
    case NS_MOUSE_LEFT_BUTTON_UP:
    case NS_MOUSE_LEFT_BUTTON_DOWN:
    case NS_MOUSE_LEFT_CLICK:
    case NS_MOUSE_LEFT_DOUBLECLICK:
      *aButton = 0;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    case NS_MOUSE_MIDDLE_CLICK:
    case NS_MOUSE_MIDDLE_DOUBLECLICK:
      *aButton = 1;
      break;
    case NS_MOUSE_RIGHT_BUTTON_UP:
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_CLICK:
    case NS_MOUSE_RIGHT_DOUBLECLICK:
    case NS_CONTEXTMENU:
      *aButton = 2;
      break;
    default:
      // This event doesn't have a mouse button associated with it
      *aButton = (PRUint16)-1;
      break;
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMouseEvent::GetRelatedTarget(nsIDOMEventTarget** aRelatedTarget)
{
  NS_ENSURE_ARG_POINTER(aRelatedTarget);
  *aRelatedTarget = nsnull;

  if (!mPresContext) {
    return NS_OK;
  }

  nsCOMPtr<nsIContent> relatedContent;
  mPresContext->EventStateManager()->
    GetEventRelatedContent(getter_AddRefs(relatedContent));
  if (!relatedContent) {
    return NS_OK;
  }

  return CallQueryInterface(relatedContent, aRelatedTarget);
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

NS_IMETHODIMP
nsDOMMouseEvent::GetWhich(PRUint32* aWhich)
{
  NS_ENSURE_ARG_POINTER(aWhich);
  PRUint16 button;
  (void) GetButton(&button);
  *aWhich = button + 1;
  return NS_OK;
}

nsresult NS_NewDOMMouseEvent(nsIDOMEvent** aInstancePtrResult,
                             nsPresContext* aPresContext,
                             nsInputEvent *aEvent) 
{
  nsDOMMouseEvent* it = new nsDOMMouseEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
