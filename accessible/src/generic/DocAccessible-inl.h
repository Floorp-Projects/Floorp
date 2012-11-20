/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessible_inl_h_
#define mozilla_a11y_DocAccessible_inl_h_

#include "DocAccessible.h"
#include "nsAccessibilityService.h"
#include "NotificationController.h"
#include "States.h"

namespace mozilla {
namespace a11y {

inline void
DocAccessible::BindChildDocument(DocAccessible* aDocument)
{
  mNotificationController->ScheduleChildDocBinding(aDocument);
}

template<class Class, class Arg>
inline void
DocAccessible::HandleNotification(Class* aInstance,
                                  typename TNotification<Class, Arg>::Callback aMethod,
                                  Arg* aArg)
{
  if (mNotificationController) {
    mNotificationController->HandleNotification<Class, Arg>(aInstance,
                                                            aMethod, aArg);
  }
}

inline void
DocAccessible::UpdateText(nsIContent* aTextNode)
{
  NS_ASSERTION(mNotificationController, "The document was shut down!");

  // Ignore the notification if initial tree construction hasn't been done yet.
  if (mNotificationController && HasLoadState(eTreeConstructed))
    mNotificationController->ScheduleTextUpdate(aTextNode);
}

inline void
DocAccessible::NotifyOfLoad(uint32_t aLoadEventType)
{
  mLoadState |= eDOMLoaded;
  mLoadEventType = aLoadEventType;

  // If the document is loaded completely then network activity was presumingly
  // caused by file loading. Fire busy state change event.
  if (HasLoadState(eCompletelyLoaded) && IsLoadEventTarget()) {
    nsRefPtr<AccEvent> stateEvent =
      new AccStateChangeEvent(this, states::BUSY, false);
    FireDelayedAccessibleEvent(stateEvent);
  }
}

inline void
DocAccessible::MaybeNotifyOfValueChange(Accessible* aAccessible)
{
  mozilla::a11y::role role = aAccessible->Role();
  if (role == roles::ENTRY || role == roles::COMBOBOX) {
    nsRefPtr<AccEvent> valueChangeEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, aAccessible,
                   eAutoDetect, AccEvent::eRemoveDupes);
    FireDelayedAccessibleEvent(valueChangeEvent);
  }
}

} // namespace a11y
} // namespace mozilla

#endif
