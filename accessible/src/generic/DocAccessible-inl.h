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

#ifdef A11Y_LOG
#include "Logging.h"
#endif

namespace mozilla {
namespace a11y {

inline void
DocAccessible::FireDelayedEvent(AccEvent* aEvent)
{
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocLoad))
    logging::DocLoadEventFired(aEvent);
#endif

  mNotificationController->QueueEvent(aEvent);
}

inline void
DocAccessible::FireDelayedEvent(uint32_t aEventType, Accessible* aTarget)
{
  nsRefPtr<AccEvent> event = new AccEvent(aEventType, aTarget);
  FireDelayedEvent(event);
}

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
DocAccessible::AddScrollListener()
{
  // Delay scroll initializing until the document has a root frame.
  if (!mPresShell->GetRootFrame())
    return;

  mDocFlags |= eScrollInitialized;
  nsIScrollableFrame* sf = mPresShell->GetRootScrollFrameAsScrollable();
  if (sf) {
    sf->AddScrollPositionListener(this);

#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eDocCreate))
      logging::Text("add scroll listener");
#endif
  }
}

inline void
DocAccessible::RemoveScrollListener()
{
  nsIScrollableFrame* sf = mPresShell->GetRootScrollFrameAsScrollable();
  if (sf)
    sf->RemoveScrollPositionListener(this);
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
    FireDelayedEvent(stateEvent);
  }
}

inline void
DocAccessible::MaybeNotifyOfValueChange(Accessible* aAccessible)
{
  a11y::role role = aAccessible->Role();
  if (role == roles::ENTRY || role == roles::COMBOBOX)
    FireDelayedEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, aAccessible);
}

} // namespace a11y
} // namespace mozilla

#endif
