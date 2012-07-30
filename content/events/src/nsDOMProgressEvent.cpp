/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMClassInfoID.h"
#include "nsDOMProgressEvent.h"

DOMCI_DATA(ProgressEvent, nsDOMProgressEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMProgressEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMProgressEvent)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ProgressEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMPL_ADDREF_INHERITED(nsDOMProgressEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMProgressEvent, nsDOMEvent)

NS_IMETHODIMP
nsDOMProgressEvent::GetLengthComputable(bool* aLengthComputable)
{
  *aLengthComputable = mLengthComputable;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMProgressEvent::GetLoaded(PRUint64* aLoaded)
{
  *aLoaded = mLoaded;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMProgressEvent::GetTotal(PRUint64* aTotal)
{
  *aTotal = mTotal;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMProgressEvent::InitProgressEvent(const nsAString& aType,
                                      bool aCanBubble,
                                      bool aCancelable,
                                      bool aLengthComputable,
                                      PRUint64 aLoaded,
                                      PRUint64 aTotal)
{
  nsresult rv = nsDOMEvent::InitEvent(aType, aCanBubble, aCancelable);
  NS_ENSURE_SUCCESS(rv, rv);

  mLoaded = aLoaded;
  mLengthComputable = aLengthComputable;
  mTotal = aTotal;

  return NS_OK;
}

nsresult
NS_NewDOMProgressEvent(nsIDOMEvent** aInstancePtrResult,
                       nsPresContext* aPresContext,
                       nsEvent* aEvent) 
{
  nsDOMProgressEvent* it = new nsDOMProgressEvent(aPresContext, aEvent);
  if (nullptr == it)
    return NS_ERROR_OUT_OF_MEMORY;

  return CallQueryInterface(it, aInstancePtrResult);
}
