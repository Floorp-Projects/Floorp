/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessible_inl_h_
#define mozilla_a11y_DocAccessible_inl_h_

#include "DocAccessible.h"
#include "nsAccessibilityService.h"
#include "nsAccessiblePivot.h"
#include "NotificationController.h"
#include "States.h"
#include "nsIScrollableFrame.h"
#include "nsIDocumentInlines.h"

#ifdef A11Y_LOG
#include "Logging.h"
#endif

namespace mozilla {
namespace a11y {

inline Accessible*
DocAccessible::AccessibleOrTrueContainer(nsINode* aNode) const
{
  // HTML comboboxes have no-content list accessible as an intermediate
  // containing all options.
  Accessible* container = GetAccessibleOrContainer(aNode);
  if (container && container->IsHTMLCombobox()) {
    return container->FirstChild();
  }
  return container;
}

inline nsIAccessiblePivot*
DocAccessible::VirtualCursor()
{
  if (!mVirtualCursor) {
    mVirtualCursor = new nsAccessiblePivot(this);
    mVirtualCursor->AddObserver(this);
  }
  return mVirtualCursor;
}

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
  RefPtr<AccEvent> event = new AccEvent(aEventType, aTarget);
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
    RefPtr<AccEvent> stateEvent =
      new AccStateChangeEvent(this, states::BUSY, false);
    FireDelayedEvent(stateEvent);
  }
}

inline void
DocAccessible::MaybeNotifyOfValueChange(Accessible* aAccessible)
{
  if (aAccessible->IsCombobox() || aAccessible->Role() == roles::ENTRY)
    FireDelayedEvent(nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE, aAccessible);
}

inline Accessible*
DocAccessible::GetAccessibleEvenIfNotInMapOrContainer(nsINode* aNode) const
{
  Accessible* acc = GetAccessibleEvenIfNotInMap(aNode);
  return acc ? acc : GetContainerAccessible(aNode);
}

inline void
DocAccessible::CreateSubtree(Accessible* aChild)
{
  // If a focused node has been shown then it could mean its frame was recreated
  // while the node stays focused and we need to fire focus event on
  // the accessible we just created. If the queue contains a focus event for
  // this node already then it will be suppressed by this one.
  Accessible* focusedAcc = nullptr;
  CacheChildrenInSubtree(aChild, &focusedAcc);

#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eVerbose)) {
    logging::Tree("TREE", "Created subtree", aChild);
  }
#endif

  // Fire events for ARIA elements.
  if (aChild->HasARIARole()) {
    roles::Role role = aChild->ARIARole();
    if (role == roles::MENUPOPUP) {
      FireDelayedEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_START, aChild);
    }
    else if (role == roles::ALERT) {
      FireDelayedEvent(nsIAccessibleEvent::EVENT_ALERT, aChild);
    }
  }

  // XXX: do we really want to send focus to focused DOM node not taking into
  // account active item?
  if (focusedAcc) {
    FocusMgr()->DispatchFocusEvent(this, focusedAcc);
    SelectionMgr()->
      SetControlSelectionListener(focusedAcc->GetNode()->AsElement());
  }
}

} // namespace a11y
} // namespace mozilla

#endif
