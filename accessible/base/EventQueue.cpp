/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventQueue.h"

#include "Accessible-inl.h"
#include "nsEventShell.h"
#include "DocAccessible.h"
#include "DocAccessibleChild.h"
#include "nsAccessibilityService.h"
#include "nsTextEquivUtils.h"
#ifdef A11Y_LOG
#  include "Logging.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

// Defines the number of selection add/remove events in the queue when they
// aren't packed into single selection within event.
const unsigned int kSelChangeCountToPack = 5;

////////////////////////////////////////////////////////////////////////////////
// EventQueue
////////////////////////////////////////////////////////////////////////////////

bool EventQueue::PushEvent(AccEvent* aEvent) {
  NS_ASSERTION((aEvent->mAccessible && aEvent->mAccessible->IsApplication()) ||
                   aEvent->Document() == mDocument,
               "Queued event belongs to another document!");

  // XXX(Bug 1631371) Check if this should use a fallible operation as it
  // pretended earlier, or change the return type to void.
  mEvents.AppendElement(aEvent);

  // Filter events.
  CoalesceEvents();

  if (aEvent->mEventRule != AccEvent::eDoNotEmit &&
      (aEvent->mEventType == nsIAccessibleEvent::EVENT_NAME_CHANGE ||
       aEvent->mEventType == nsIAccessibleEvent::EVENT_TEXT_REMOVED ||
       aEvent->mEventType == nsIAccessibleEvent::EVENT_TEXT_INSERTED)) {
    PushNameChange(aEvent->mAccessible);
  }
  return true;
}

bool EventQueue::PushNameChange(Accessible* aTarget) {
  // Fire name change event on parent given that this event hasn't been
  // coalesced, the parent's name was calculated from its subtree, and the
  // subtree was changed.
  if (aTarget->HasNameDependentParent()) {
    // Only continue traversing up the tree if it's possible that the parent
    // accessible's name can depend on this accessible's name.
    Accessible* parent = aTarget->Parent();
    while (parent &&
           nsTextEquivUtils::HasNameRule(parent, eNameFromSubtreeIfReqRule)) {
      // Test possible name dependent parent.
      if (nsTextEquivUtils::HasNameRule(parent, eNameFromSubtreeRule)) {
        nsAutoString name;
        ENameValueFlag nameFlag = parent->Name(name);
        // If name is obtained from subtree, fire name change event.
        if (nameFlag == eNameFromSubtree) {
          RefPtr<AccEvent> nameChangeEvent =
              new AccEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, parent);
          return PushEvent(nameChangeEvent);
        }
        break;
      }
      parent = parent->Parent();
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// EventQueue: private

void EventQueue::CoalesceEvents() {
  NS_ASSERTION(mEvents.Length(), "There should be at least one pending event!");
  uint32_t tail = mEvents.Length() - 1;
  AccEvent* tailEvent = mEvents[tail];

  switch (tailEvent->mEventRule) {
    case AccEvent::eCoalesceReorder: {
      DebugOnly<Accessible*> target = tailEvent->mAccessible.get();
      MOZ_ASSERT(
          target->IsApplication() || target->IsOuterDoc() ||
              target->IsXULTree(),
          "Only app or outerdoc accessible reorder events are in the queue");
      MOZ_ASSERT(tailEvent->GetEventType() == nsIAccessibleEvent::EVENT_REORDER,
                 "only reorder events should be queued");
      break;  // case eCoalesceReorder
    }

    case AccEvent::eCoalesceOfSameType: {
      // Coalesce old events by newer event.
      for (uint32_t index = tail - 1; index < tail; index--) {
        AccEvent* accEvent = mEvents[index];
        if (accEvent->mEventType == tailEvent->mEventType &&
            accEvent->mEventRule == tailEvent->mEventRule) {
          accEvent->mEventRule = AccEvent::eDoNotEmit;
          return;
        }
      }
      break;  // case eCoalesceOfSameType
    }

    case AccEvent::eCoalesceSelectionChange: {
      AccSelChangeEvent* tailSelChangeEvent = downcast_accEvent(tailEvent);
      for (uint32_t index = tail - 1; index < tail; index--) {
        AccEvent* thisEvent = mEvents[index];
        if (thisEvent->mEventRule == tailEvent->mEventRule) {
          AccSelChangeEvent* thisSelChangeEvent = downcast_accEvent(thisEvent);

          // Coalesce selection change events within same control.
          if (tailSelChangeEvent->mWidget == thisSelChangeEvent->mWidget) {
            CoalesceSelChangeEvents(tailSelChangeEvent, thisSelChangeEvent,
                                    index);
            return;
          }
        }
      }
      break;  // eCoalesceSelectionChange
    }

    case AccEvent::eCoalesceStateChange: {
      // If state change event is duped then ignore previous event. If state
      // change event is opposite to previous event then no event is emitted
      // (accessible state wasn't changed).
      for (uint32_t index = tail - 1; index < tail; index--) {
        AccEvent* thisEvent = mEvents[index];
        if (thisEvent->mEventRule != AccEvent::eDoNotEmit &&
            thisEvent->mEventType == tailEvent->mEventType &&
            thisEvent->mAccessible == tailEvent->mAccessible) {
          AccStateChangeEvent* thisSCEvent = downcast_accEvent(thisEvent);
          AccStateChangeEvent* tailSCEvent = downcast_accEvent(tailEvent);
          if (thisSCEvent->mState == tailSCEvent->mState) {
            thisEvent->mEventRule = AccEvent::eDoNotEmit;
            if (thisSCEvent->mIsEnabled != tailSCEvent->mIsEnabled)
              tailEvent->mEventRule = AccEvent::eDoNotEmit;
          }
        }
      }
      break;  // eCoalesceStateChange
    }

    case AccEvent::eCoalesceTextSelChange: {
      // Coalesce older event by newer event for the same selection or target.
      // Events for same selection may have different targets and vice versa one
      // target may be pointed by different selections (for latter see
      // bug 927159).
      for (uint32_t index = tail - 1; index < tail; index--) {
        AccEvent* thisEvent = mEvents[index];
        if (thisEvent->mEventRule != AccEvent::eDoNotEmit &&
            thisEvent->mEventType == tailEvent->mEventType) {
          AccTextSelChangeEvent* thisTSCEvent = downcast_accEvent(thisEvent);
          AccTextSelChangeEvent* tailTSCEvent = downcast_accEvent(tailEvent);
          if (thisTSCEvent->mSel == tailTSCEvent->mSel ||
              thisEvent->mAccessible == tailEvent->mAccessible)
            thisEvent->mEventRule = AccEvent::eDoNotEmit;
        }
      }
      break;  // eCoalesceTextSelChange
    }

    case AccEvent::eRemoveDupes: {
      // Check for repeat events, coalesce newly appended event by more older
      // event.
      for (uint32_t index = tail - 1; index < tail; index--) {
        AccEvent* accEvent = mEvents[index];
        if (accEvent->mEventType == tailEvent->mEventType &&
            accEvent->mEventRule == tailEvent->mEventRule &&
            accEvent->mAccessible == tailEvent->mAccessible) {
          tailEvent->mEventRule = AccEvent::eDoNotEmit;
          return;
        }
      }
      break;  // case eRemoveDupes
    }

    default:
      break;  // case eAllowDupes, eDoNotEmit
  }           // switch
}

void EventQueue::CoalesceSelChangeEvents(AccSelChangeEvent* aTailEvent,
                                         AccSelChangeEvent* aThisEvent,
                                         uint32_t aThisIndex) {
  aTailEvent->mPreceedingCount = aThisEvent->mPreceedingCount + 1;

  // Pack all preceding events into single selection within event
  // when we receive too much selection add/remove events.
  if (aTailEvent->mPreceedingCount >= kSelChangeCountToPack) {
    aTailEvent->mEventType = nsIAccessibleEvent::EVENT_SELECTION_WITHIN;
    aTailEvent->mAccessible = aTailEvent->mWidget;
    aThisEvent->mEventRule = AccEvent::eDoNotEmit;

    // Do not emit any preceding selection events for same widget if they
    // weren't coalesced yet.
    if (aThisEvent->mEventType != nsIAccessibleEvent::EVENT_SELECTION_WITHIN) {
      for (uint32_t jdx = aThisIndex - 1; jdx < aThisIndex; jdx--) {
        AccEvent* prevEvent = mEvents[jdx];
        if (prevEvent->mEventRule == aTailEvent->mEventRule) {
          AccSelChangeEvent* prevSelChangeEvent = downcast_accEvent(prevEvent);
          if (prevSelChangeEvent->mWidget == aTailEvent->mWidget)
            prevSelChangeEvent->mEventRule = AccEvent::eDoNotEmit;
        }
      }
    }
    return;
  }

  // Pack sequential selection remove and selection add events into
  // single selection change event.
  if (aTailEvent->mPreceedingCount == 1 &&
      aTailEvent->mItem != aThisEvent->mItem) {
    if (aTailEvent->mSelChangeType == AccSelChangeEvent::eSelectionAdd &&
        aThisEvent->mSelChangeType == AccSelChangeEvent::eSelectionRemove) {
      aThisEvent->mEventRule = AccEvent::eDoNotEmit;
      aTailEvent->mEventType = nsIAccessibleEvent::EVENT_SELECTION;
      aTailEvent->mPackedEvent = aThisEvent;
      return;
    }

    if (aThisEvent->mSelChangeType == AccSelChangeEvent::eSelectionAdd &&
        aTailEvent->mSelChangeType == AccSelChangeEvent::eSelectionRemove) {
      aTailEvent->mEventRule = AccEvent::eDoNotEmit;
      aThisEvent->mEventType = nsIAccessibleEvent::EVENT_SELECTION;
      aThisEvent->mPackedEvent = aTailEvent;
      return;
    }
  }

  // Unpack the packed selection change event because we've got one
  // more selection add/remove.
  if (aThisEvent->mEventType == nsIAccessibleEvent::EVENT_SELECTION) {
    if (aThisEvent->mPackedEvent) {
      aThisEvent->mPackedEvent->mEventType =
          aThisEvent->mPackedEvent->mSelChangeType ==
                  AccSelChangeEvent::eSelectionAdd
              ? nsIAccessibleEvent::EVENT_SELECTION_ADD
              : nsIAccessibleEvent::EVENT_SELECTION_REMOVE;

      aThisEvent->mPackedEvent->mEventRule = AccEvent::eCoalesceSelectionChange;

      aThisEvent->mPackedEvent = nullptr;
    }

    aThisEvent->mEventType =
        aThisEvent->mSelChangeType == AccSelChangeEvent::eSelectionAdd
            ? nsIAccessibleEvent::EVENT_SELECTION_ADD
            : nsIAccessibleEvent::EVENT_SELECTION_REMOVE;

    return;
  }

  // Convert into selection add since control has single selection but other
  // selection events for this control are queued.
  if (aTailEvent->mEventType == nsIAccessibleEvent::EVENT_SELECTION)
    aTailEvent->mEventType = nsIAccessibleEvent::EVENT_SELECTION_ADD;
}

////////////////////////////////////////////////////////////////////////////////
// EventQueue: event queue

void EventQueue::ProcessEventQueue() {
  // Process only currently queued events.
  nsTArray<RefPtr<AccEvent> > events;
  events.SwapElements(mEvents);

  uint32_t eventCount = events.Length();
#ifdef A11Y_LOG
  if (eventCount > 0 && logging::IsEnabled(logging::eEvents)) {
    logging::MsgBegin("EVENTS", "events processing");
    logging::Address("document", mDocument);
    logging::MsgEnd();
  }
#endif

  for (uint32_t idx = 0; idx < eventCount; idx++) {
    AccEvent* event = events[idx];
    if (event->mEventRule != AccEvent::eDoNotEmit) {
      Accessible* target = event->GetAccessible();
      if (!target || target->IsDefunct()) continue;

      // Dispatch the focus event if target is still focused.
      if (event->mEventType == nsIAccessibleEvent::EVENT_FOCUS) {
        FocusMgr()->ProcessFocusEvent(event);
        continue;
      }

      // Dispatch caret moved and text selection change events.
      if (event->mEventType ==
          nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED) {
        SelectionMgr()->ProcessTextSelChangeEvent(event);
        continue;
      }

      // Fire selected state change events in support to selection events.
      if (event->mEventType == nsIAccessibleEvent::EVENT_SELECTION_ADD) {
        nsEventShell::FireEvent(event->mAccessible, states::SELECTED, true,
                                event->mIsFromUserInput);

      } else if (event->mEventType ==
                 nsIAccessibleEvent::EVENT_SELECTION_REMOVE) {
        nsEventShell::FireEvent(event->mAccessible, states::SELECTED, false,
                                event->mIsFromUserInput);

      } else if (event->mEventType == nsIAccessibleEvent::EVENT_SELECTION) {
        AccSelChangeEvent* selChangeEvent = downcast_accEvent(event);
        nsEventShell::FireEvent(event->mAccessible, states::SELECTED,
                                (selChangeEvent->mSelChangeType ==
                                 AccSelChangeEvent::eSelectionAdd),
                                event->mIsFromUserInput);

        if (selChangeEvent->mPackedEvent) {
          nsEventShell::FireEvent(
              selChangeEvent->mPackedEvent->mAccessible, states::SELECTED,
              (selChangeEvent->mPackedEvent->mSelChangeType ==
               AccSelChangeEvent::eSelectionAdd),
              selChangeEvent->mPackedEvent->mIsFromUserInput);
        }
      }

      nsEventShell::FireEvent(event);
    }

    if (!mDocument) return;
  }
}
