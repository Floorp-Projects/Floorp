/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsDOMBeforeUnloadEvent.h"

using namespace mozilla;

NS_IMPL_ADDREF_INHERITED(nsDOMBeforeUnloadEvent, nsDOMEvent)
NS_IMPL_RELEASE_INHERITED(nsDOMBeforeUnloadEvent, nsDOMEvent)

NS_INTERFACE_MAP_BEGIN(nsDOMBeforeUnloadEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBeforeUnloadEvent)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEvent)

NS_IMETHODIMP
nsDOMBeforeUnloadEvent::SetReturnValue(const nsAString& aReturnValue)
{
  mText = aReturnValue;
  return NS_OK;  // Don't throw an exception
}

NS_IMETHODIMP
nsDOMBeforeUnloadEvent::GetReturnValue(nsAString& aReturnValue)
{
  aReturnValue = mText;
  return NS_OK;  // Don't throw an exception
}

nsresult NS_NewDOMBeforeUnloadEvent(nsIDOMEvent** aInstancePtrResult,
                                    mozilla::dom::EventTarget* aOwner,
                                    nsPresContext* aPresContext,
                                    WidgetEvent* aEvent) 
{
  nsDOMBeforeUnloadEvent* it =
    new nsDOMBeforeUnloadEvent(aOwner, aPresContext, aEvent);

  return CallQueryInterface(it, aInstancePtrResult);
}
