/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsEventShell.h"

#include "nsDocAccessible.h"

////////////////////////////////////////////////////////////////////////////////
// nsEventShell
////////////////////////////////////////////////////////////////////////////////

void
nsEventShell::FireEvent(nsAccEvent *aEvent)
{
  if (!aEvent)
    return;

  nsRefPtr<nsAccessible> acc =
    nsAccUtils::QueryObject<nsAccessible>(aEvent->GetAccessible());
  NS_ENSURE_TRUE(acc,);

  nsCOMPtr<nsIDOMNode> node;
  aEvent->GetDOMNode(getter_AddRefs(node));
  if (node) {
    sEventTargetNode = node;
    sEventFromUserInput = aEvent->IsFromUserInput();
  }

  acc->HandleAccEvent(aEvent);

  sEventTargetNode = nsnull;
}

void
nsEventShell::FireEvent(PRUint32 aEventType, nsIAccessible *aAccessible,
                        PRBool aIsAsynch, EIsFromUserInput aIsFromUserInput)
{
  NS_ENSURE_TRUE(aAccessible,);

  nsRefPtr<nsAccEvent> event = new nsAccEvent(aEventType, aAccessible,
                                              aIsAsynch, aIsFromUserInput);

  FireEvent(event);
}

void 
nsEventShell::GetEventAttributes(nsIDOMNode *aNode,
                                 nsIPersistentProperties *aAttributes)
{
  if (aNode != sEventTargetNode)
    return;

  nsAccUtils::SetAccAttr(aAttributes, nsAccessibilityAtoms::eventFromInput,
                         sEventFromUserInput ? NS_LITERAL_STRING("true") :
                                               NS_LITERAL_STRING("false"));
}

////////////////////////////////////////////////////////////////////////////////
// nsEventShell: private

PRBool nsEventShell::sEventFromUserInput = PR_FALSE;
nsCOMPtr<nsIDOMNode> nsEventShell::sEventTargetNode;


////////////////////////////////////////////////////////////////////////////////
// nsAccEventQueue
////////////////////////////////////////////////////////////////////////////////

nsAccEventQueue::nsAccEventQueue(nsDocAccessible *aDocument):
  mProcessingStarted(PR_FALSE), mDocument(aDocument)
{
}

nsAccEventQueue::~nsAccEventQueue()
{
  NS_ASSERTION(mDocument, "Queue wasn't shut down!");
}

////////////////////////////////////////////////////////////////////////////////
// nsAccEventQueue: nsISupports and cycle collection

NS_IMPL_CYCLE_COLLECTION_CLASS(nsAccEventQueue)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsAccEventQueue)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsAccEventQueue)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mDocument");
  cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mDocument.get()));

  PRUint32 i, length = tmp->mEvents.Length();
  for (i = 0; i < length; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mEvents[i]");
    cb.NoteXPCOMChild(tmp->mEvents[i].get());
  }
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsAccEventQueue)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mDocument)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mEvents)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsAccEventQueue)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsAccEventQueue)

////////////////////////////////////////////////////////////////////////////////
// nsAccEventQueue: public

void
nsAccEventQueue::Push(nsAccEvent *aEvent)
{
  mEvents.AppendElement(aEvent);
  
  // Filter events.
  CoalesceEvents();
  
  // Process events.
  PrepareFlush();
}

void
nsAccEventQueue::Shutdown()
{
  mDocument = nsnull;
  mEvents.Clear();
}

////////////////////////////////////////////////////////////////////////////////
// nsAccEventQueue: private

void
nsAccEventQueue::PrepareFlush()
{
  // If there are pending events in the queue and events flush isn't planed
  // yet start events flush asyncroniously.
  if (mEvents.Length() > 0 && !mProcessingStarted) {
    NS_DISPATCH_RUNNABLEMETHOD(Flush, this)
    mProcessingStarted = PR_TRUE;
  }
}

void
nsAccEventQueue::Flush()
{
  // If the document accessible is now shut down, don't fire events in it
  // anymore.
  if (!mDocument)
    return;

  nsCOMPtr<nsIPresShell> presShell = mDocument->GetPresShell();
  if (!presShell)
    return;

  // Flush layout so that all the frame construction, reflow, and styles are
  // up-to-date. This will ensure we can get frames for the related nodes, as
  // well as get the most current information for calculating things like
  // visibility. We don't flush the display because we don't care about
  // painting. If no flush is necessary the method will simple return.
  presShell->FlushPendingNotifications(Flush_Layout);

  // Process only currently queued events. Newly appended events duiring events
  // flusing won't be processed.
  PRUint32 length = mEvents.Length();
  NS_ASSERTION(length, "How did we get here without events to fire?");

  for (PRUint32 index = 0; index < length; index ++) {

    // No presshell means the document was shut down duiring event handling
    // by AT.
    if (!mDocument || !mDocument->HasWeakShell())
      break;

    nsAccEvent *accEvent = mEvents[index];
    if (accEvent->mEventRule != nsAccEvent::eDoNotEmit)
      mDocument->ProcessPendingEvent(accEvent);
  }

  // Mark we are ready to start event processing again.
  mProcessingStarted = PR_FALSE;

  // If the document accessible is alive then remove processed events from the
  // queue (otherwise they were removed on shutdown already) and reinitialize
  // queue processing callback if necessary (new events might occur duiring
  // delayed event processing).
  if (mDocument && mDocument->HasWeakShell()) {
    mEvents.RemoveElementsAt(0, length);
    PrepareFlush();
  }
}

void
nsAccEventQueue::CoalesceEvents()
{
  PRUint32 numQueuedEvents = mEvents.Length();
  PRInt32 tail = numQueuedEvents - 1;

  nsAccEvent* tailEvent = mEvents[tail];
  switch(tailEvent->mEventRule) {
    case nsAccEvent::eCoalesceFromSameSubtree:
    {
      for (PRInt32 index = 0; index < tail; index ++) {
        nsAccEvent* thisEvent = mEvents[index];
        if (thisEvent->mEventType != tailEvent->mEventType)
          continue; // Different type

        if (thisEvent->mEventRule == nsAccEvent::eAllowDupes ||
            thisEvent->mEventRule == nsAccEvent::eDoNotEmit)
          continue; //  Do not need to check

        if (thisEvent->mNode == tailEvent->mNode) {
          if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
            CoalesceReorderEventsFromSameSource(thisEvent, tailEvent);
            continue;
          }

          // Dupe
          thisEvent->mEventRule = nsAccEvent::eDoNotEmit;
          continue;
        }

        // More older show event target (thisNode) can't be contained by recent
        // show event target (tailNode), i.e be a descendant of tailNode.
        // XXX: target of older show event caused by DOM node appending can be
        // contained by target of recent show event caused by style change.
        // XXX: target of older show event caused by style change can be
        // contained by target of recent show event caused by style change.
        PRBool thisCanBeDescendantOfTail =
          tailEvent->mEventType != nsIAccessibleEvent::EVENT_SHOW ||
          tailEvent->mIsAsync;

        if (thisCanBeDescendantOfTail &&
            nsCoreUtils::IsAncestorOf(tailEvent->mNode, thisEvent->mNode)) {
          // thisNode is a descendant of tailNode.

          if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
            CoalesceReorderEventsFromSameTree(tailEvent, thisEvent);
            continue;
          }

          // Do not emit thisEvent, also apply this result to sibling nodes of
          // thisNode.
          thisEvent->mEventRule = nsAccEvent::eDoNotEmit;
          ApplyToSiblings(0, index, thisEvent->mEventType,
                          thisEvent->mNode, nsAccEvent::eDoNotEmit);
          continue;
        }

#ifdef DEBUG
        if (!thisCanBeDescendantOfTail &&
            nsCoreUtils::IsAncestorOf(tailEvent->mNode, thisEvent->mNode)) {
          NS_NOTREACHED("Older event target is a descendant of recent event target!");
        }
#endif

        // More older hide event target (thisNode) can't contain recent hide
        // event target (tailNode), i.e. be ancestor of tailNode.
        if (tailEvent->mEventType != nsIAccessibleEvent::EVENT_HIDE &&
            nsCoreUtils::IsAncestorOf(thisEvent->mNode, tailEvent->mNode)) {
          // tailNode is a descendant of thisNode

          if (thisEvent->mEventType == nsIAccessibleEvent::EVENT_REORDER) {
            CoalesceReorderEventsFromSameTree(thisEvent, tailEvent);
            continue;
          }

          // Do not emit tailEvent, also apply this result to sibling nodes of
          // tailNode.
          tailEvent->mEventRule = nsAccEvent::eDoNotEmit;
          ApplyToSiblings(0, tail, tailEvent->mEventType,
                          tailEvent->mNode, nsAccEvent::eDoNotEmit);
          break;
        }

#ifdef DEBUG
        if (tailEvent->mEventType == nsIAccessibleEvent::EVENT_HIDE &&
            nsCoreUtils::IsAncestorOf(thisEvent->mNode, tailEvent->mNode)) {
          NS_NOTREACHED("More older hide event target is an ancestor of recent hide event target!");
        }
#endif

      } // for (index)

      if (tailEvent->mEventRule != nsAccEvent::eDoNotEmit) {
        // Not in another event node's subtree, and no other event is in this
        // event node's subtree. This event should be emitted. Apply this result
        // to sibling nodes of tailNode.

        // We should do this in all cases even when tailEvent is hide event and
        // it's caused by DOM node removal because the rule can applied for
        // sibling event targets caused by style changes.
        ApplyToSiblings(0, tail, tailEvent->mEventType,
                        tailEvent->mNode, nsAccEvent::eAllowDupes);
      }
    } break; // case eCoalesceFromSameSubtree

    case nsAccEvent::eRemoveDupes:
    {
      // Check for repeat events.
      for (PRInt32 index = 0; index < tail; index ++) {
        nsAccEvent* accEvent = mEvents[index];
        if (accEvent->mEventType == tailEvent->mEventType &&
            accEvent->mEventRule == tailEvent->mEventRule &&
            accEvent->mNode == tailEvent->mNode) {
          accEvent->mEventRule = nsAccEvent::eDoNotEmit;
        }
      }
    } break; // case eRemoveDupes

    default:
      break; // case eAllowDupes, eDoNotEmit
  } // switch
}

void
nsAccEventQueue::ApplyToSiblings(PRUint32 aStart, PRUint32 aEnd,
                                 PRUint32 aEventType, nsINode* aNode,
                                 nsAccEvent::EEventRule aEventRule)
{
  for (PRUint32 index = aStart; index < aEnd; index ++) {
    nsAccEvent* accEvent = mEvents[index];
    if (accEvent->mEventType == aEventType &&
        accEvent->mEventRule != nsAccEvent::eDoNotEmit &&
        nsCoreUtils::AreSiblings(accEvent->mNode, aNode)) {
      accEvent->mEventRule = aEventRule;
    }
  }
}

void
nsAccEventQueue::CoalesceReorderEventsFromSameSource(nsAccEvent *aAccEvent1,
                                                     nsAccEvent *aAccEvent2)
{
  // Do not emit event2 if event1 is unconditional.
  nsCOMPtr<nsAccReorderEvent> reorderEvent1 = do_QueryInterface(aAccEvent1);
  if (reorderEvent1->IsUnconditionalEvent()) {
    aAccEvent2->mEventRule = nsAccEvent::eDoNotEmit;
    return;
  }

  // Do not emit event1 if event2 is unconditional.
  nsCOMPtr<nsAccReorderEvent> reorderEvent2 = do_QueryInterface(aAccEvent2);
  if (reorderEvent2->IsUnconditionalEvent()) {
    aAccEvent1->mEventRule = nsAccEvent::eDoNotEmit;
    return;
  }

  // Do not emit event2 if event1 is valid, otherwise do not emit event1.
  if (reorderEvent1->HasAccessibleInReasonSubtree())
    aAccEvent2->mEventRule = nsAccEvent::eDoNotEmit;
  else
    aAccEvent1->mEventRule = nsAccEvent::eDoNotEmit;
}

void
nsAccEventQueue::CoalesceReorderEventsFromSameTree(nsAccEvent *aAccEvent,
                                                   nsAccEvent *aDescendantAccEvent)
{
  // Do not emit descendant event if this event is unconditional.
  nsCOMPtr<nsAccReorderEvent> reorderEvent = do_QueryInterface(aAccEvent);
  if (reorderEvent->IsUnconditionalEvent()) {
    aDescendantAccEvent->mEventRule = nsAccEvent::eDoNotEmit;
    return;
  }

  // Do not emit descendant event if this event is valid otherwise do not emit
  // this event.
  if (reorderEvent->HasAccessibleInReasonSubtree())
    aDescendantAccEvent->mEventRule = nsAccEvent::eDoNotEmit;
  else
    aAccEvent->mEventRule = nsAccEvent::eDoNotEmit;
}
