/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessible_inl_h_
#define mozilla_a11y_DocAccessible_inl_h_

#include "DocAccessible.h"
#include "LocalAccessible-inl.h"
#include "nsAccessibilityService.h"
#include "NotificationController.h"
#include "States.h"
#include "nsIScrollableFrame.h"
#include "mozilla/dom/DocumentInlines.h"

#ifdef A11Y_LOG
#  include "Logging.h"
#endif

namespace mozilla {
namespace a11y {

inline LocalAccessible* DocAccessible::AccessibleOrTrueContainer(
    nsINode* aNode, bool aNoContainerIfPruned) const {
  // HTML comboboxes have no-content list accessible as an intermediate
  // containing all options.
  LocalAccessible* container =
      GetAccessibleOrContainer(aNode, aNoContainerIfPruned);
  if (container && container->IsHTMLCombobox()) {
    return container->LocalFirstChild();
  }
  return container;
}

inline bool DocAccessible::IsContentLoaded() const {
  // eDOMLoaded flag check is used for error pages as workaround to make this
  // method return correct result since error pages do not receive 'pageshow'
  // event and as consequence Document::IsShowing() returns false.
  return mDocumentNode && mDocumentNode->IsVisible() &&
         (mDocumentNode->IsShowing() || HasLoadState(eDOMLoaded));
}

inline bool DocAccessible::IsHidden() const { return mDocumentNode->Hidden(); }

inline void DocAccessible::FireDelayedEvent(AccEvent* aEvent) {
#ifdef A11Y_LOG
  if (logging::IsEnabled(logging::eDocLoad)) logging::DocLoadEventFired(aEvent);
#endif

  mNotificationController->QueueEvent(aEvent);
}

inline void DocAccessible::FireDelayedEvent(uint32_t aEventType,
                                            LocalAccessible* aTarget) {
  RefPtr<AccEvent> event = new AccEvent(aEventType, aTarget);
  FireDelayedEvent(event);
}

inline void DocAccessible::BindChildDocument(DocAccessible* aDocument) {
  mNotificationController->ScheduleChildDocBinding(aDocument);
}

template <class Class, class... Args>
inline void DocAccessible::HandleNotification(
    Class* aInstance, typename TNotification<Class, Args...>::Callback aMethod,
    Args*... aArgs) {
  if (mNotificationController) {
    mNotificationController->HandleNotification<Class, Args...>(
        aInstance, aMethod, aArgs...);
  }
}

inline void DocAccessible::UpdateText(nsIContent* aTextNode) {
  NS_ASSERTION(mNotificationController, "The document was shut down!");

  // Ignore the notification if initial tree construction hasn't been done yet.
  if (mNotificationController && HasLoadState(eTreeConstructed)) {
    mNotificationController->ScheduleTextUpdate(aTextNode);
  }
}

inline void DocAccessible::NotifyOfLoad(uint32_t aLoadEventType) {
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

inline void DocAccessible::MaybeNotifyOfValueChange(
    LocalAccessible* aAccessible) {
  if (aAccessible->IsCombobox() || aAccessible->Role() == roles::ENTRY ||
      aAccessible->Role() == roles::SPINBUTTON) {
    FireDelayedEvent(nsIAccessibleEvent::EVENT_TEXT_VALUE_CHANGE, aAccessible);
  }
}

inline LocalAccessible* DocAccessible::GetAccessibleEvenIfNotInMapOrContainer(
    nsINode* aNode) const {
  LocalAccessible* acc = GetAccessibleEvenIfNotInMap(aNode);
  return acc ? acc : GetContainerAccessible(aNode);
}

inline void DocAccessible::CreateSubtree(LocalAccessible* aChild) {
  // If a focused node has been shown then it could mean its frame was recreated
  // while the node stays focused and we need to fire focus event on
  // the accessible we just created. If the queue contains a focus event for
  // this node already then it will be suppressed by this one.
  LocalAccessible* focusedAcc = nullptr;
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
    } else if (role == roles::ALERT) {
      FireDelayedEvent(nsIAccessibleEvent::EVENT_ALERT, aChild);
    }
  }

  // XXX: do we really want to send focus to focused DOM node not taking into
  // account active item?
  if (focusedAcc) {
    FocusMgr()->DispatchFocusEvent(this, focusedAcc);
    SelectionMgr()->SetControlSelectionListener(
        focusedAcc->GetNode()->AsElement());
  }
}

inline DocAccessible::AttrRelProviders* DocAccessible::GetRelProviders(
    dom::Element* aElement, const nsAString& aID) const {
  DependentIDsHashtable* hash = mDependentIDsHashes.Get(
      aElement->GetUncomposedDocOrConnectedShadowRoot());
  if (hash) {
    return hash->Get(aID);
  }
  return nullptr;
}

inline DocAccessible::AttrRelProviders* DocAccessible::GetOrCreateRelProviders(
    dom::Element* aElement, const nsAString& aID) {
  dom::DocumentOrShadowRoot* docOrShadowRoot =
      aElement->GetUncomposedDocOrConnectedShadowRoot();
  DependentIDsHashtable* hash =
      mDependentIDsHashes.GetOrInsertNew(docOrShadowRoot);

  return hash->GetOrInsertNew(aID);
}

inline void DocAccessible::RemoveRelProvidersIfEmpty(dom::Element* aElement,
                                                     const nsAString& aID) {
  dom::DocumentOrShadowRoot* docOrShadowRoot =
      aElement->GetUncomposedDocOrConnectedShadowRoot();
  DependentIDsHashtable* hash = mDependentIDsHashes.Get(docOrShadowRoot);
  if (hash) {
    AttrRelProviders* providers = hash->Get(aID);
    if (providers && providers->Length() == 0) {
      hash->Remove(aID);
      if (mDependentIDsHashes.IsEmpty()) {
        mDependentIDsHashes.Remove(docOrShadowRoot);
      }
    }
  }
}

}  // namespace a11y
}  // namespace mozilla

#endif
