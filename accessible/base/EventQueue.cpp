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
#include "Logging.h"
#endif

using namespace mozilla;
using namespace mozilla::a11y;

// Defines the number of selection add/remove events in the queue when they
// aren't packed into single selection within event.
const unsigned int kSelChangeCountToPack = 5;

////////////////////////////////////////////////////////////////////////////////
// EventQueue
////////////////////////////////////////////////////////////////////////////////

bool
EventQueue::PushEvent(AccEvent* aEvent)
{
  NS_ASSERTION((aEvent->mAccessible && aEvent->mAccessible->IsApplication()) ||
               aEvent->GetDocAccessible() == mDocument,
               "Queued event belongs to another document!");

  if (!mEvents.AppendElement(aEvent))
    return false;

  // Filter events.
  CoalesceEvents();

  // Fire name change event on parent given that this event hasn't been
  // coalesced, the parent's name was calculated from its subtree, and the
  // subtree was changed.
  Accessible* target = aEvent->mAccessible;
  if (aEvent->mEventRule != AccEvent::eDoNotEmit &&
      target->HasNameDependentParent() &&
      (aEvent->mEventType == nsIAccessibleEvent::EVENT_NAME_CHANGE ||
       aEvent->mEventType == nsIAccessibleEvent::EVENT_TEXT_REMOVED ||
       aEvent->mEventType == nsIAccessibleEvent::EVENT_TEXT_INSERTED ||
       aEvent->mEventType == nsIAccessibleEvent::EVENT_SHOW ||
       aEvent->mEventType == nsIAccessibleEvent::EVENT_HIDE)) {
    // Only continue traversing up the tree if it's possible that the parent
    // accessible's name can depend on this accessible's name.
    Accessible* parent = target->Parent();
    while (parent &&
           nsTextEquivUtils::HasNameRule(parent, eNameFromSubtreeIfReqRule)) {
      // Test possible name dependent parent.
      if (nsTextEquivUtils::HasNameRule(parent, eNameFromSubtreeRule)) {
        nsAutoString name;
        ENameValueFlag nameFlag = parent->Name(name);
        // If name is obtained from subtree, fire name change event.
        if (nameFlag == eNameFromSubtree) {
          nsRefPtr<AccEvent> nameChangeEvent =
            new AccEvent(nsIAccessibleEvent::EVENT_NAME_CHANGE, parent);
          PushEvent(nameChangeEvent);
        }
        break;
      }
      parent = parent->Parent();
    }
  }

  // Associate text change with hide event if it wasn't stolen from hiding
  // siblings during coalescence.
  AccMutationEvent* showOrHideEvent = downcast_accEvent(aEvent);
  if (showOrHideEvent && !showOrHideEvent->mTextChangeEvent)
    CreateTextChangeEventFor(showOrHideEvent);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// EventQueue: private

void
EventQueue::CoalesceEvents()
{
  NS_ASSERTION(mEvents.Length(), "There should be at least one pending event!");
  uint32_t tail = mEvents.Length() - 1;
  AccEvent* tailEvent = mEvents[tail];

  switch(tailEvent->mEventRule) {
    case AccEvent::eCoalesceReorder:
      CoalesceReorderEvents(tailEvent);
      break; // case eCoalesceReorder

    case AccEvent::eCoalesceMutationTextChange:
    {
      for (uint32_t index = tail - 1; index < tail; index--) {
        AccEvent* thisEvent = mEvents[index];
        if (thisEvent->mEventRule != tailEvent->mEventRule)
          continue;

        // We don't currently coalesce text change events from show/hide events.
        if (thisEvent->mEventType != tailEvent->mEventType)
          continue;

        // Show events may be duped because of reinsertion (removal is ignored
        // because initial insertion is not processed). Ignore initial
        // insertion.
        if (thisEvent->mAccessible == tailEvent->mAccessible)
          thisEvent->mEventRule = AccEvent::eDoNotEmit;

        AccMutationEvent* tailMutationEvent = downcast_accEvent(tailEvent);
        AccMutationEvent* thisMutationEvent = downcast_accEvent(thisEvent);
        if (tailMutationEvent->mParent != thisMutationEvent->mParent)
          continue;

        // Coalesce text change events for hide and show events.
        if (thisMutationEvent->IsHide()) {
          AccHideEvent* tailHideEvent = downcast_accEvent(tailEvent);
          AccHideEvent* thisHideEvent = downcast_accEvent(thisEvent);
          CoalesceTextChangeEventsFor(tailHideEvent, thisHideEvent);
          break;
        }

        AccShowEvent* tailShowEvent = downcast_accEvent(tailEvent);
        AccShowEvent* thisShowEvent = downcast_accEvent(thisEvent);
        CoalesceTextChangeEventsFor(tailShowEvent, thisShowEvent);
        break;
      }
    } break; // case eCoalesceMutationTextChange

    case AccEvent::eCoalesceOfSameType:
    {
      // Coalesce old events by newer event.
      for (uint32_t index = tail - 1; index < tail; index--) {
        AccEvent* accEvent = mEvents[index];
        if (accEvent->mEventType == tailEvent->mEventType &&
          accEvent->mEventRule == tailEvent->mEventRule) {
          accEvent->mEventRule = AccEvent::eDoNotEmit;
          return;
        }
      }
    } break; // case eCoalesceOfSameType

    case AccEvent::eCoalesceSelectionChange:
    {
      AccSelChangeEvent* tailSelChangeEvent = downcast_accEvent(tailEvent);
      for (uint32_t index = tail - 1; index < tail; index--) {
        AccEvent* thisEvent = mEvents[index];
        if (thisEvent->mEventRule == tailEvent->mEventRule) {
          AccSelChangeEvent* thisSelChangeEvent =
            downcast_accEvent(thisEvent);

          // Coalesce selection change events within same control.
          if (tailSelChangeEvent->mWidget == thisSelChangeEvent->mWidget) {
            CoalesceSelChangeEvents(tailSelChangeEvent, thisSelChangeEvent, index);
            return;
          }
        }
      }

    } break; // eCoalesceSelectionChange

    case AccEvent::eCoalesceStateChange:
    {
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
      break; // eCoalesceStateChange
    }

    case AccEvent::eCoalesceTextSelChange:
    {
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
    } break; // eCoalesceTextSelChange

    case AccEvent::eRemoveDupes:
    {
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
    } break; // case eRemoveDupes

    default:
      break; // case eAllowDupes, eDoNotEmit
  } // switch
}

void
EventQueue::CoalesceReorderEvents(AccEvent* aTailEvent)
{
  uint32_t count = mEvents.Length();
  for (uint32_t index = count - 2; index < count; index--) {
    AccEvent* thisEvent = mEvents[index];

    // Skip events of different types and targeted to application accessible.
    if (thisEvent->mEventType != aTailEvent->mEventType ||
        thisEvent->mAccessible->IsApplication())
      continue;

    // If thisEvent target is not in document longer, i.e. if it was
    // removed from the tree then do not emit the event.
    if (!thisEvent->mAccessible->IsDoc() &&
        !thisEvent->mAccessible->IsInDocument()) {
      thisEvent->mEventRule = AccEvent::eDoNotEmit;
      continue;
    }

    // Coalesce earlier event of the same target.
    if (thisEvent->mAccessible == aTailEvent->mAccessible) {
      if (thisEvent->mEventRule == AccEvent::eDoNotEmit) {
        AccReorderEvent* tailReorder = downcast_accEvent(aTailEvent);
        tailReorder->DoNotEmitAll();
      } else {
        thisEvent->mEventRule = AccEvent::eDoNotEmit;
      }

      return;
    }

    // If tailEvent contains thisEvent
    // then
    //   if show or hide of tailEvent contains a grand parent of thisEvent
    //   then ignore thisEvent and its show and hide events
    //   otherwise ignore thisEvent but not its show and hide events
    Accessible* thisParent = thisEvent->mAccessible;
    while (thisParent && thisParent != mDocument) {
      if (thisParent->Parent() == aTailEvent->mAccessible) {
        AccReorderEvent* tailReorder = downcast_accEvent(aTailEvent);
        uint32_t eventType = tailReorder->IsShowHideEventTarget(thisParent);

        // Sometimes InvalidateChildren() and
        // DocAccessible::CacheChildrenInSubtree() can conspire to reparent an
        // accessible in this case no need for mutation events.  Se bug 883708
        // for details.
        if (eventType == nsIAccessibleEvent::EVENT_SHOW ||
            eventType == nsIAccessibleEvent::EVENT_HIDE) {
          AccReorderEvent* thisReorder = downcast_accEvent(thisEvent);
          thisReorder->DoNotEmitAll();
        } else {
          thisEvent->mEventRule = AccEvent::eDoNotEmit;
        }

        return;
      }

      thisParent = thisParent->Parent();
    }

    // If tailEvent is contained by thisEvent
    // then
    //   if show of thisEvent contains the tailEvent
    //   then ignore tailEvent
    //   if hide of thisEvent contains the tailEvent
    //   then assert
    //   otherwise ignore tailEvent but not its show and hide events
    Accessible* tailParent = aTailEvent->mAccessible;
    while (tailParent && tailParent != mDocument) {
      if (tailParent->Parent() == thisEvent->mAccessible) {
        AccReorderEvent* thisReorder = downcast_accEvent(thisEvent);
        AccReorderEvent* tailReorder = downcast_accEvent(aTailEvent);
        uint32_t eventType = thisReorder->IsShowHideEventTarget(tailParent);
        if (eventType == nsIAccessibleEvent::EVENT_SHOW)
          tailReorder->DoNotEmitAll();
        else if (eventType == nsIAccessibleEvent::EVENT_HIDE)
          NS_ERROR("Accessible tree was modified after it was removed! Huh?");
        else
          aTailEvent->mEventRule = AccEvent::eDoNotEmit;

        return;
      }

      tailParent = tailParent->Parent();
    }

  } // for (index)
}

void
EventQueue::CoalesceSelChangeEvents(AccSelChangeEvent* aTailEvent,
                                    AccSelChangeEvent* aThisEvent,
                                    uint32_t aThisIndex)
{
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
          AccSelChangeEvent* prevSelChangeEvent =
            downcast_accEvent(prevEvent);
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
        aThisEvent->mPackedEvent->mSelChangeType == AccSelChangeEvent::eSelectionAdd ?
          nsIAccessibleEvent::EVENT_SELECTION_ADD :
          nsIAccessibleEvent::EVENT_SELECTION_REMOVE;

      aThisEvent->mPackedEvent->mEventRule =
        AccEvent::eCoalesceSelectionChange;

      aThisEvent->mPackedEvent = nullptr;
    }

    aThisEvent->mEventType =
      aThisEvent->mSelChangeType == AccSelChangeEvent::eSelectionAdd ?
        nsIAccessibleEvent::EVENT_SELECTION_ADD :
        nsIAccessibleEvent::EVENT_SELECTION_REMOVE;

    return;
  }

  // Convert into selection add since control has single selection but other
  // selection events for this control are queued.
  if (aTailEvent->mEventType == nsIAccessibleEvent::EVENT_SELECTION)
    aTailEvent->mEventType = nsIAccessibleEvent::EVENT_SELECTION_ADD;
}

void
EventQueue::CoalesceTextChangeEventsFor(AccHideEvent* aTailEvent,
                                        AccHideEvent* aThisEvent)
{
  // XXX: we need a way to ignore SplitNode and JoinNode() when they do not
  // affect the text within the hypertext.

  AccTextChangeEvent* textEvent = aThisEvent->mTextChangeEvent;
  if (!textEvent)
    return;

  if (aThisEvent->mNextSibling == aTailEvent->mAccessible) {
    aTailEvent->mAccessible->AppendTextTo(textEvent->mModifiedText);

  } else if (aThisEvent->mPrevSibling == aTailEvent->mAccessible) {
    uint32_t oldLen = textEvent->GetLength();
    aTailEvent->mAccessible->AppendTextTo(textEvent->mModifiedText);
    textEvent->mStart -= textEvent->GetLength() - oldLen;
  }

  aTailEvent->mTextChangeEvent.swap(aThisEvent->mTextChangeEvent);
}

void
EventQueue::CoalesceTextChangeEventsFor(AccShowEvent* aTailEvent,
                                        AccShowEvent* aThisEvent)
{
  AccTextChangeEvent* textEvent = aThisEvent->mTextChangeEvent;
  if (!textEvent)
    return;

  if (aTailEvent->mAccessible->IndexInParent() ==
      aThisEvent->mAccessible->IndexInParent() + 1) {
    // If tail target was inserted after this target, i.e. tail target is next
    // sibling of this target.
    aTailEvent->mAccessible->AppendTextTo(textEvent->mModifiedText);

  } else if (aTailEvent->mAccessible->IndexInParent() ==
             aThisEvent->mAccessible->IndexInParent() -1) {
    // If tail target was inserted before this target, i.e. tail target is
    // previous sibling of this target.
    nsAutoString startText;
    aTailEvent->mAccessible->AppendTextTo(startText);
    textEvent->mModifiedText = startText + textEvent->mModifiedText;
    textEvent->mStart -= startText.Length();
  }

  aTailEvent->mTextChangeEvent.swap(aThisEvent->mTextChangeEvent);
}

void
EventQueue::CreateTextChangeEventFor(AccMutationEvent* aEvent)
{
  Accessible* container = aEvent->mAccessible->Parent();
  if (!container)
    return;

  HyperTextAccessible* textAccessible = container->AsHyperText();
  if (!textAccessible)
    return;

  // Don't fire event for the first html:br in an editor.
  if (aEvent->mAccessible->Role() == roles::WHITESPACE) {
    nsCOMPtr<nsIEditor> editor = textAccessible->GetEditor();
    if (editor) {
      bool isEmpty = false;
      editor->GetDocumentIsEmpty(&isEmpty);
      if (isEmpty)
        return;
    }
  }

  int32_t offset = textAccessible->GetChildOffset(aEvent->mAccessible);

  nsAutoString text;
  aEvent->mAccessible->AppendTextTo(text);
  if (text.IsEmpty())
    return;

  aEvent->mTextChangeEvent =
    new AccTextChangeEvent(textAccessible, offset, text, aEvent->IsShow(),
                           aEvent->mIsFromUserInput ? eFromUserInput : eNoUserInput);
}

////////////////////////////////////////////////////////////////////////////////
// EventQueue: event queue

void
EventQueue::ProcessEventQueue()
{
  // Process only currently queued events.
  nsTArray<nsRefPtr<AccEvent> > events;
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
      if (!target || target->IsDefunct())
        continue;

      // Dispatch the focus event if target is still focused.
      if (event->mEventType == nsIAccessibleEvent::EVENT_FOCUS) {
        FocusMgr()->ProcessFocusEvent(event);
        continue;
      }

      // Dispatch caret moved and text selection change events.
      if (event->mEventType == nsIAccessibleEvent::EVENT_TEXT_SELECTION_CHANGED) {
        SelectionMgr()->ProcessTextSelChangeEvent(event);
        continue;
      }

      // Fire selected state change events in support to selection events.
      if (event->mEventType == nsIAccessibleEvent::EVENT_SELECTION_ADD) {
        nsEventShell::FireEvent(event->mAccessible, states::SELECTED,
                                true, event->mIsFromUserInput);

      } else if (event->mEventType == nsIAccessibleEvent::EVENT_SELECTION_REMOVE) {
        nsEventShell::FireEvent(event->mAccessible, states::SELECTED,
                                false, event->mIsFromUserInput);

      } else if (event->mEventType == nsIAccessibleEvent::EVENT_SELECTION) {
        AccSelChangeEvent* selChangeEvent = downcast_accEvent(event);
        nsEventShell::FireEvent(event->mAccessible, states::SELECTED,
                                (selChangeEvent->mSelChangeType == AccSelChangeEvent::eSelectionAdd),
                                event->mIsFromUserInput);

        if (selChangeEvent->mPackedEvent) {
          nsEventShell::FireEvent(selChangeEvent->mPackedEvent->mAccessible,
                                  states::SELECTED,
                                  (selChangeEvent->mPackedEvent->mSelChangeType == AccSelChangeEvent::eSelectionAdd),
                                  selChangeEvent->mPackedEvent->mIsFromUserInput);
        }
      }

      nsEventShell::FireEvent(event);

      // Fire text change events.
      AccMutationEvent* mutationEvent = downcast_accEvent(event);
      if (mutationEvent) {
        if (mutationEvent->mTextChangeEvent)
          nsEventShell::FireEvent(mutationEvent->mTextChangeEvent);
      }
    }

    if (event->mEventType == nsIAccessibleEvent::EVENT_HIDE)
      mDocument->ShutdownChildrenInSubtree(event->mAccessible);

    if (!mDocument)
      return;

    if (IPCAccessibilityActive()) {
    DocAccessibleChild* ipcDoc = mDocument->IPCDoc();
      if (event->mEventType == nsIAccessibleEvent::EVENT_SHOW)
        ipcDoc->ShowEvent(downcast_accEvent(event));
      else if (event->mEventType == nsIAccessibleEvent::EVENT_HIDE)
        ipcDoc->SendHideEvent(reinterpret_cast<uintptr_t>(event->GetAccessible()));
      else
        ipcDoc->SendEvent(event->GetEventType());
    }
  }
}
