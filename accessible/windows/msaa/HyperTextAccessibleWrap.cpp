/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HyperTextAccessibleWrap.h"
#include "LocalAccessible-inl.h"

#include "nsEventShell.h"

#include "mozilla/StaticPtr.h"

using namespace mozilla;
using namespace mozilla::a11y;

NS_IMPL_ISUPPORTS_INHERITED0(HyperTextAccessibleWrap, HyperTextAccessible)

nsresult HyperTextAccessibleWrap::HandleAccEvent(AccEvent* aEvent) {
  uint32_t eventType = aEvent->GetEventType();

  if (eventType == nsIAccessibleEvent::EVENT_TEXT_REMOVED ||
      eventType == nsIAccessibleEvent::EVENT_TEXT_INSERTED) {
    LocalAccessible* accessible = aEvent->GetAccessible();
    if (accessible && accessible->IsHyperText()) {
      AccTextChangeEvent* event = downcast_accEvent(aEvent);
      HyperTextAccessibleWrap* text =
          static_cast<HyperTextAccessibleWrap*>(accessible->AsHyperText());
      ia2AccessibleText::UpdateTextChangeData(
          text, event->IsTextInserted(), event->ModifiedText(),
          event->GetStartOffset(), event->GetLength());
    }
  }

  return HyperTextAccessible::HandleAccEvent(aEvent);
}
