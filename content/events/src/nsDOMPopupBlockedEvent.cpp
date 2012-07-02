/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "nsDOMPopupBlockedEvent.h"
#include "nsIURI.h"

NS_IMPL_ADDREF_INHERITED(nsDOMPopupBlockedEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMPopupBlockedEvent, nsDOMEvent)

DOMCI_DATA(PopupBlockedEvent, nsDOMPopupBlockedEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMPopupBlockedEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMPopupBlockedEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(PopupBlockedEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMPopupBlockedEvent::InitPopupBlockedEvent(const nsAString & aTypeArg,
                            bool aCanBubbleArg, bool aCancelableArg,
                            nsIDOMWindow *aRequestingWindow,
                            nsIURI *aPopupWindowURI,
                            const nsAString & aPopupWindowName,
                            const nsAString & aPopupWindowFeatures)
{
  nsresult rv = nsDOMEvent::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mRequestingWindow = do_GetWeakReference(aRequestingWindow);
  mPopupWindowURI = aPopupWindowURI;
  mPopupWindowFeatures = aPopupWindowFeatures;
  mPopupWindowName = aPopupWindowName;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMPopupBlockedEvent::GetRequestingWindow(nsIDOMWindow **aRequestingWindow)
{
  *aRequestingWindow = nsnull;
  if (mRequestingWindow)
    CallQueryReferent(mRequestingWindow.get(), aRequestingWindow);

  return NS_OK;  // Don't throw an exception
}

NS_IMETHODIMP
nsDOMPopupBlockedEvent::GetPopupWindowURI(nsIURI **aPopupWindowURI)
{
  NS_ENSURE_ARG_POINTER(aPopupWindowURI);

  *aPopupWindowURI = mPopupWindowURI;
  NS_IF_ADDREF(*aPopupWindowURI);
  return NS_OK;  // Don't throw an exception
}

NS_IMETHODIMP
nsDOMPopupBlockedEvent::GetPopupWindowFeatures(nsAString &aPopupWindowFeatures)
{

  aPopupWindowFeatures = mPopupWindowFeatures;
  return NS_OK;  // Don't throw an exception
}

NS_IMETHODIMP
nsDOMPopupBlockedEvent::GetPopupWindowName(nsAString &aPopupWindowName)
{
  aPopupWindowName = mPopupWindowName;
  return NS_OK;  // Don't throw an exception
}

nsresult NS_NewDOMPopupBlockedEvent(nsIDOMEvent** aInstancePtrResult,
                                    nsPresContext* aPresContext,
                                    nsEvent *aEvent) 
{
  nsDOMPopupBlockedEvent* it = new nsDOMPopupBlockedEvent(aPresContext, aEvent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}
