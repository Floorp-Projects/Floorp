/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/BeforeUnloadEvent.h"

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF_INHERITED(BeforeUnloadEvent, Event)
NS_IMPL_RELEASE_INHERITED(BeforeUnloadEvent, Event)

NS_INTERFACE_MAP_BEGIN(BeforeUnloadEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMBeforeUnloadEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMETHODIMP
BeforeUnloadEvent::SetReturnValue(const nsAString& aReturnValue)
{
  mText = aReturnValue;
  return NS_OK;  // Don't throw an exception
}

NS_IMETHODIMP
BeforeUnloadEvent::GetReturnValue(nsAString& aReturnValue)
{
  aReturnValue = mText;
  return NS_OK;  // Don't throw an exception
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMBeforeUnloadEvent(nsIDOMEvent** aInstancePtrResult,
                           EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           WidgetEvent* aEvent) 
{
  BeforeUnloadEvent* it = new BeforeUnloadEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
