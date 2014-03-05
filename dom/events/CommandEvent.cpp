/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CommandEvent.h"
#include "mozilla/MiscEvents.h"
#include "prtime.h"

namespace mozilla {
namespace dom {

CommandEvent::CommandEvent(EventTarget* aOwner,
                           nsPresContext* aPresContext,
                           WidgetCommandEvent* aEvent)
  : Event(aOwner, aPresContext,
          aEvent ? aEvent :
                   new WidgetCommandEvent(false, nullptr, nullptr, nullptr))
{
  mEvent->time = PR_Now();
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
  }
}

NS_INTERFACE_MAP_BEGIN(CommandEvent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCommandEvent)
NS_INTERFACE_MAP_END_INHERITING(Event)

NS_IMPL_ADDREF_INHERITED(CommandEvent, Event)
NS_IMPL_RELEASE_INHERITED(CommandEvent, Event)

NS_IMETHODIMP
CommandEvent::GetCommand(nsAString& aCommand)
{
  nsIAtom* command = mEvent->AsCommandEvent()->command;
  if (command) {
    command->ToString(aCommand);
  } else {
    aCommand.Truncate();
  }
  return NS_OK;
}

NS_IMETHODIMP
CommandEvent::InitCommandEvent(const nsAString& aTypeArg,
                               bool aCanBubbleArg,
                               bool aCancelableArg,
                               const nsAString& aCommand)
{
  nsresult rv = Event::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);
  NS_ENSURE_SUCCESS(rv, rv);

  mEvent->AsCommandEvent()->command = do_GetAtom(aCommand);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

nsresult
NS_NewDOMCommandEvent(nsIDOMEvent** aInstancePtrResult,
                      EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      WidgetCommandEvent* aEvent)
{
  CommandEvent* it = new CommandEvent(aOwner, aPresContext, aEvent);
  return CallQueryInterface(it, aInstancePtrResult);
}
