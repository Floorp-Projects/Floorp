/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CommandEvent.h"
#include "mozilla/MiscEvents.h"
#include "prtime.h"

namespace mozilla::dom {

CommandEvent::CommandEvent(EventTarget* aOwner, nsPresContext* aPresContext,
                           WidgetCommandEvent* aEvent)
    : Event(aOwner, aPresContext, aEvent ? aEvent : new WidgetCommandEvent()) {
  if (aEvent) {
    mEventIsInternal = false;
  } else {
    mEventIsInternal = true;
  }
}

void CommandEvent::GetCommand(nsAString& aCommand) {
  nsAtom* command = mEvent->AsCommandEvent()->mCommand;
  if (command) {
    command->ToString(aCommand);
  } else {
    aCommand.Truncate();
  }
}

}  // namespace mozilla::dom

using namespace mozilla;
using namespace mozilla::dom;

already_AddRefed<CommandEvent> NS_NewDOMCommandEvent(
    EventTarget* aOwner, nsPresContext* aPresContext,
    WidgetCommandEvent* aEvent) {
  RefPtr<CommandEvent> it = new CommandEvent(aOwner, aPresContext, aEvent);
  return it.forget();
}
