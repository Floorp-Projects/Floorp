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

#include "nsDOMPopupBlockedEvent.h"
#include "nsIURI.h"
#include "nsContentUtils.h"

nsDOMPopupBlockedEvent::nsDOMPopupBlockedEvent(nsPresContext* aPresContext, nsPopupBlockedEvent* aEvent)
: nsDOMEvent(aPresContext, aEvent ? aEvent : new nsPopupBlockedEvent())
{
  NS_ASSERTION(mEvent->eventStructType == NS_POPUPBLOCKED_EVENT, "event type mismatch");

  if (aEvent) {
    mEventIsInternal = PR_FALSE;
    mEventIsTrusted = PR_TRUE;
  }
  else {
    mEventIsInternal = PR_TRUE;
    mEventIsTrusted = PR_FALSE;
    mEvent->time = PR_Now();
  }
}

nsDOMPopupBlockedEvent::~nsDOMPopupBlockedEvent() 
{
  if (mEventIsInternal) {
    if (mEvent->eventStructType == NS_POPUPBLOCKED_EVENT) {
      nsPopupBlockedEvent* event = NS_STATIC_CAST(nsPopupBlockedEvent*, mEvent);
      NS_IF_RELEASE(event->mRequestingWindowURI);
      NS_IF_RELEASE(event->mPopupWindowURI);
    }
  }
}

NS_IMPL_ADDREF_INHERITED(nsDOMPopupBlockedEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMPopupBlockedEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMPopupBlockedEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPopupBlockedEvent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(PopupBlockedEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMPopupBlockedEvent::InitPopupBlockedEvent(const nsAString & aTypeArg,
                            PRBool aCanBubbleArg, PRBool aCancelableArg,
                            nsIURI *aRequestingWindowURI,
                            nsIURI *aPopupWindowURI,
                            const nsAString & aPopupWindowFeatures)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (mEvent->eventStructType)
  {
    case NS_POPUPBLOCKED_EVENT:
    {
       nsPopupBlockedEvent* event = NS_STATIC_CAST(nsPopupBlockedEvent*, mEvent);
       event->mRequestingWindowURI = aRequestingWindowURI;
       event->mPopupWindowURI = aPopupWindowURI;
       NS_IF_ADDREF(event->mRequestingWindowURI);
       NS_IF_ADDREF(event->mPopupWindowURI);
       event->mPopupWindowFeatures = aPopupWindowFeatures;
       break;
    }
    default:
       break;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsDOMPopupBlockedEvent::GetRequestingWindowURI(nsIURI **aRequestingWindowURI)
{
  NS_ENSURE_ARG_POINTER(aRequestingWindowURI);
  if (mEvent->eventStructType == NS_POPUPBLOCKED_EVENT) {
    nsPopupBlockedEvent* event = NS_STATIC_CAST(nsPopupBlockedEvent*, mEvent);
    *aRequestingWindowURI = event->mRequestingWindowURI;
    NS_IF_ADDREF(*aRequestingWindowURI);
    return NS_OK;
  }
  *aRequestingWindowURI = 0;
  return NS_OK;  // Don't throw an exception
}

NS_IMETHODIMP
nsDOMPopupBlockedEvent::GetPopupWindowURI(nsIURI **aPopupWindowURI)
{
  NS_ENSURE_ARG_POINTER(aPopupWindowURI);
  if (mEvent->eventStructType == NS_POPUPBLOCKED_EVENT) {
    nsPopupBlockedEvent* event = NS_STATIC_CAST(nsPopupBlockedEvent*, mEvent);
    *aPopupWindowURI = event->mPopupWindowURI;
    NS_IF_ADDREF(*aPopupWindowURI);
    return NS_OK;
  }
  *aPopupWindowURI = 0;
  return NS_OK;  // Don't throw an exception
}

NS_IMETHODIMP
nsDOMPopupBlockedEvent::GetPopupWindowFeatures(nsAString &aPopupWindowFeatures)
{
  if (mEvent->eventStructType == NS_POPUPBLOCKED_EVENT) {
    nsPopupBlockedEvent* event = NS_STATIC_CAST(nsPopupBlockedEvent*, mEvent);
    aPopupWindowFeatures = event->mPopupWindowFeatures;
    return NS_OK;
  }
  aPopupWindowFeatures.Truncate();
  return NS_OK;  // Don't throw an exception
}

nsresult NS_NewDOMPopupBlockedEvent(nsIDOMEvent** aInstancePtrResult,
                                    nsPresContext* aPresContext,
                                    nsPopupBlockedEvent *aEvent) 
{
  nsDOMPopupBlockedEvent* it = new nsDOMPopupBlockedEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
