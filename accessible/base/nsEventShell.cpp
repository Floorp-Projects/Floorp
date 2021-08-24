/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsEventShell.h"

#include "nsAccUtils.h"
#include "Logging.h"
#include "AccAttributes.h"

#include "mozilla/StaticPtr.h"
#include "mozilla/dom/DOMStringList.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// nsEventShell
////////////////////////////////////////////////////////////////////////////////

void nsEventShell::FireEvent(AccEvent* aEvent) {
  if (!aEvent || aEvent->mEventRule == AccEvent::eDoNotEmit) return;

  LocalAccessible* accessible = aEvent->GetAccessible();
  NS_ENSURE_TRUE_VOID(accessible);

  nsINode* node = accessible->GetNode();
  if (node) {
    sEventTargetNode = node;
    sEventFromUserInput = aEvent->IsFromUserInput();
  }

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eEvents)) {
    logging::MsgBegin("EVENTS", "events fired");
    nsAutoString type;
    GetAccService()->GetStringEventType(aEvent->GetEventType(), type);
    logging::MsgEntry("type: %s", NS_ConvertUTF16toUTF8(type).get());
    if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_STATE_CHANGE) {
      AccStateChangeEvent* event = downcast_accEvent(aEvent);
      RefPtr<dom::DOMStringList> stringStates =
          GetAccService()->GetStringStates(event->GetState());
      nsAutoString state;
      stringStates->Item(0, state);
      logging::MsgEntry("state: %s = %s", NS_ConvertUTF16toUTF8(state).get(),
                        event->IsStateEnabled() ? "true" : "false");
    }
    logging::AccessibleInfo("target", aEvent->GetAccessible());
    logging::MsgEnd();
  }
#endif

  accessible->HandleAccEvent(aEvent);
  aEvent->mEventRule = AccEvent::eDoNotEmit;

  sEventTargetNode = nullptr;
}

void nsEventShell::FireEvent(uint32_t aEventType, LocalAccessible* aAccessible,
                             EIsFromUserInput aIsFromUserInput) {
  NS_ENSURE_TRUE_VOID(aAccessible);

  RefPtr<AccEvent> event =
      new AccEvent(aEventType, aAccessible, aIsFromUserInput);

  FireEvent(event);
}

void nsEventShell::GetEventAttributes(nsINode* aNode,
                                      AccAttributes* aAttributes) {
  if (aNode != sEventTargetNode) return;

  aAttributes->SetAttribute(nsGkAtoms::eventFromInput, sEventFromUserInput);
}

////////////////////////////////////////////////////////////////////////////////
// nsEventShell: private

bool nsEventShell::sEventFromUserInput = false;
StaticRefPtr<nsINode> nsEventShell::sEventTargetNode;
