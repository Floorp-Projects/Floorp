/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CompositionEvent.h"
#include "mozilla/TextEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

CompositionEvent::CompositionEvent(EventTarget* aOwner,
                                   nsPresContext* aPresContext,
                                   WidgetCompositionEvent* aEvent)
  : UIEvent(aOwner, aPresContext,
            aEvent ? aEvent : new WidgetCompositionEvent(false, 0, nullptr))
{
  NS_ASSERTION(mEvent->mClass == eCompositionEventClass,
               "event type mismatch");

  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
    mEvent->time = PR_Now();

    // XXX compositionstart is cancelable in draft of DOM3 Events.
    //     However, it doesn't make sence for us, we cannot cancel composition
    //     when we sends compositionstart event.
    mEvent->mFlags.mCancelable = false;
  }

  // XXX Do we really need to duplicate the data value?
  mData = mEvent->AsCompositionEvent()->mData;
  // TODO: Native event should have locale information.
}

NS_IMPL_ADDREF_INHERITED(CompositionEvent, UIEvent)
NS_IMPL_RELEASE_INHERITED(CompositionEvent, UIEvent)

NS_INTERFACE_MAP_BEGIN(CompositionEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCompositionEvent)
NS_INTERFACE_MAP_END_INHERITING(UIEvent)

NS_IMETHODIMP
CompositionEvent::GetData(nsAString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
CompositionEvent::GetLocale(nsAString& aLocale)
{
  aLocale = mLocale;
  return NS_OK;
}

NS_IMETHODIMP
CompositionEvent::InitCompositionEvent(const nsAString& aType,
                                       bool aCanBubble,
                                       bool aCancelable,
                                       nsIDOMWindow* aView,
                                       const nsAString& aData,
                                       const nsAString& aLocale)
{
  nsresult rv = UIEvent::InitUIEvent(aType, aCanBubble, aCancelable, aView, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  mData = aData;
  mLocale = aLocale;
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMCompositionEvent(nsIDOMEvent** aInstancePtrResult,
                          EventTarget* aOwner,
                          nsPresContext* aPresContext,
                          WidgetCompositionEvent* aEvent)
{
  CompositionEvent* event = new CompositionEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(event, aInstancePtrResult);
}
