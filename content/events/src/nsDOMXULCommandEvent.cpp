/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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
 * The Original Code is Gecko.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
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

#include "nsDOMXULCommandEvent.h"
#include "nsContentUtils.h"

nsDOMXULCommandEvent::nsDOMXULCommandEvent(nsPresContext* aPresContext,
                                           nsXULCommandEvent* aEvent)
  : nsDOMUIEvent(aPresContext,
                 aEvent ? aEvent : new nsXULCommandEvent(PR_FALSE, 0, nsnull))
{
  if (aEvent) {
    mEventIsInternal = PR_FALSE;
  }
  else {
    mEventIsInternal = PR_TRUE;
    mEvent->time = PR_Now();
  }
}

nsDOMXULCommandEvent::~nsDOMXULCommandEvent()
{
}

NS_IMPL_ADDREF_INHERITED(nsDOMXULCommandEvent, nsDOMUIEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMXULCommandEvent, nsDOMUIEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMXULCommandEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMXULCommandEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(XULCommandEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMUIEvent)

NS_IMETHODIMP
nsDOMXULCommandEvent::GetAltKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = Event()->isAlt;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::GetCtrlKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = Event()->isControl;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::GetShiftKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = Event()->isShift;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::GetMetaKey(PRBool* aIsDown)
{
  NS_ENSURE_ARG_POINTER(aIsDown);
  *aIsDown = Event()->isMeta;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::GetSourceEvent(nsIDOMEvent** aSourceEvent)
{
  NS_ENSURE_ARG_POINTER(aSourceEvent);
  NS_IF_ADDREF(*aSourceEvent = Event()->sourceEvent);
  return NS_OK;
}

NS_IMETHODIMP
nsDOMXULCommandEvent::InitCommandEvent(const nsAString& aType,
                                       PRBool aCanBubble, PRBool aCancelable,
                                       nsIDOMAbstractView *aView,
                                       PRInt32 aDetail,
                                       PRBool aCtrlKey, PRBool aAltKey,
                                       PRBool aShiftKey, PRBool aMetaKey,
                                       nsIDOMEvent* aSourceEvent)
{
  nsresult rv = nsDOMUIEvent::InitUIEvent(aType, aCanBubble, aCancelable,
                                          aView, aDetail);
  NS_ENSURE_SUCCESS(rv, rv);

  nsXULCommandEvent *event = Event();
  event->isControl = aCtrlKey;
  event->isAlt = aAltKey;
  event->isShift = aShiftKey;
  event->isMeta = aMetaKey;
  event->sourceEvent = aSourceEvent;

  return NS_OK;
}


nsresult NS_NewDOMXULCommandEvent(nsIDOMEvent** aInstancePtrResult,
                                  nsPresContext* aPresContext,
                                  nsXULCommandEvent *aEvent) 
{
  nsDOMXULCommandEvent* it = new nsDOMXULCommandEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
