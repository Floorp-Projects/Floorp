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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Felipe Gomes <felipc@gmail.com>
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

#include "nsDOMMozTouchEvent.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"

nsDOMMozTouchEvent::nsDOMMozTouchEvent(nsPresContext* aPresContext, nsMozTouchEvent* aEvent)
  : nsDOMMouseEvent(aPresContext, aEvent ? aEvent : new nsMozTouchEvent(PR_FALSE, 0, nsnull, 0))
{
  NS_ASSERTION(mEvent->eventStructType == NS_MOZTOUCH_EVENT, "event type mismatch NS_MOZTOUCH_EVENT");

  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  } else {
    mEventIsInternal = PR_TRUE;
    mEvent->time = PR_Now();
    mEvent->refPoint.x = mEvent->refPoint.y = 0;
  }
}

nsDOMMozTouchEvent::~nsDOMMozTouchEvent()
{
  if (mEventIsInternal) {
    delete static_cast<nsMozTouchEvent*>(mEvent);
    mEvent = nsnull;
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMMozTouchEvent, nsDOMMouseEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMMozTouchEvent, nsDOMMouseEvent)

DOMCI_DATA(MozTouchEvent, nsDOMMozTouchEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMMozTouchEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozTouchEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozTouchEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMMouseEvent)

/* readonly attribute unsigned long steramId; */
NS_IMETHODIMP
nsDOMMozTouchEvent::GetStreamId(PRUint32 *aStreamId)
{
  NS_ENSURE_ARG_POINTER(aStreamId);
  *aStreamId = static_cast<nsMozTouchEvent*>(mEvent)->streamId;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMMozTouchEvent::InitMozTouchEvent(const nsAString& aTypeArg,
                                      bool aCanBubbleArg,
                                      bool aCancelableArg,
                                      nsIDOMWindow* aViewArg,
                                      PRInt32 aDetailArg,
                                      PRInt32 aScreenX,
                                      PRInt32 aScreenY,
                                      PRInt32 aClientX,
                                      PRInt32 aClientY,
                                      bool aCtrlKeyArg,
                                      bool aAltKeyArg,
                                      bool aShiftKeyArg,
                                      bool aMetaKeyArg,
                                      PRUint16 aButton,
                                      nsIDOMEventTarget* aRelatedTarget,
                                      PRUint32 aStreamId)
{
  nsresult rv = nsDOMMouseEvent::InitMouseEvent(aTypeArg,
                                                aCanBubbleArg,
                                                aCancelableArg,
                                                aViewArg,
                                                aDetailArg,
                                                aScreenX,
                                                aScreenY,
                                                aClientX,
                                                aClientY,
                                                aCtrlKeyArg,
                                                aAltKeyArg,
                                                aShiftKeyArg,
                                                aMetaKeyArg,
                                                aButton,
                                                aRelatedTarget);
  NS_ENSURE_SUCCESS(rv, rv);

  nsMozTouchEvent* mozTouchEvent = static_cast<nsMozTouchEvent*>(mEvent);
  mozTouchEvent->streamId = aStreamId;

  return NS_OK;
}

nsresult NS_NewDOMMozTouchEvent(nsIDOMEvent** aInstancePtrResult,
                                     nsPresContext* aPresContext,
                                     nsMozTouchEvent *aEvent)
{
  nsDOMMozTouchEvent *it = new nsDOMMozTouchEvent(aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
