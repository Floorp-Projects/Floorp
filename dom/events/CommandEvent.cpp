/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
  mEvent->mTime = PR_Now();
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
  Event::InitEvent(aTypeArg, aCanBubbleArg, aCancelableArg);

  mEvent->AsCommandEvent()->command = NS_Atomize(aCommand);
  return NS_OK;
}

} // namespace dom
} // namespace mozilla

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<CommandEvent>
NS_NewDOMCommandEvent(EventTarget* aOwner,
                      nsPresContext* aPresContext,
                      WidgetCommandEvent* aEvent)
{
  RefPtr<CommandEvent> it =
    new CommandEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
