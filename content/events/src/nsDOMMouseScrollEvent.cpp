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
 * Markus Stange <mstange@themasta.com>
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsDOMMouseScrollEvent.h"
#include "nsGUIEvent.h"
#include "nsIContent.h"
#include "nsContentUtils.h"

nsDOMMouseScrollEvent::nsDOMMouseScrollEvent(nsPresContext* aPresContext,
                                             nsInputEvent* aEvent)
  : nsDOMMouseEvent(aPresContext, aEvent ? aEvent :
                                  new nsMouseScrollEvent(PR_FALSE, 0, nsnull))
{
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  } else {
    mEventIsInternal = PR_TRUE;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
    static_cast<nsMouseEvent*>(mEvent)->inputSource = nsIDOMMouseEvent::MOZ_SOURCE_UNKNOWN;
  }

  if(mEvent->eventStructType == NS_MOUSE_SCROLL_EVENT) {
    nsMouseScrollEvent* mouseEvent = static_cast<nsMouseScrollEvent*>(mEvent);
    mDetail = mouseEvent->delta;
  }
}

nsDOMMouseScrollEvent::~nsDOMMouseScrollEvent()
{
  if (mEventIsInternal && mEvent) {
    switch (mEvent->eventStructType)
    {
      case NS_MOUSE_SCROLL_EVENT:
        delete static_cast<nsMouseScrollEvent*>(mEvent);
        break;
      default:
        delete mEvent;
        break;
    }
    mEvent = nsnull;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMMouseScrollEvent, nsDOMMouseEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMouseScrollEvent, nsDOMMouseEvent)

DOMCI_DATA(MouseScrollEvent, nsDOMMouseScrollEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMouseScrollEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseScrollEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MouseScrollEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

NS_IMETHODIMP
nsDOMMouseScrollEvent::InitMouseScrollEvent(const nsAString & aType, bool aCanBubble, bool aCancelable,
                                nsIDOMWindow *aView, PRInt32 aDetail, PRInt32 aScreenX, 
                                PRInt32 aScreenY, PRInt32 aClientX, PRInt32 aClientY, 
                                bool aCtrlKey, bool aAltKey, bool aShiftKey, 
                                bool aMetaKey, PRUint16 aButton, nsIDOMEventTarget *aRelatedTarget,
                                PRInt32 aAxis)
{
  nsresult rv = nsDOMMouseEvent::InitMouseEvent(aType, aCanBubble, aCancelable, aView, aDetail,
                                                aScreenX, aScreenY, aClientX, aClientY, aCtrlKey,
                                                aAltKey, aShiftKey, aMetaKey, aButton, aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);
  
  if (mEvent->eventStructType == NS_MOUSE_SCROLL_EVENT) {
    static_cast<nsMouseScrollEvent*>(mEvent)->scrollFlags =
        (aAxis == HORIZONTAL_AXIS) ? nsMouseScrollEvent::kIsHorizontal
                                   : nsMouseScrollEvent::kIsVertical;
  }

  return NS_OK;
}


NS_IMETHODIMP
nsDOMMouseScrollEvent::GetAxis(PRInt32* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  if (mEvent->eventStructType == NS_MOUSE_SCROLL_EVENT) {
    PRUint32 flags = static_cast<nsMouseScrollEvent*>(mEvent)->scrollFlags;
    *aResult = (flags & nsMouseScrollEvent::kIsHorizontal)
         ? PRInt32(HORIZONTAL_AXIS) : PRInt32(VERTICAL_AXIS);
  } else {
    *aResult = 0;
  }
  return NS_OK;
}

nsresult NS_NewDOMMouseScrollEvent(nsIDOMEvent** aInstancePtrResult,
                                   nsPresContext* aPresContext,
                                   nsInputEvent *aEvent) 
{
  nsDOMMouseScrollEvent* it = new nsDOMMouseScrollEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
