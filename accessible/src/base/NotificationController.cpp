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
 * Mozilla Foundation.
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

#include "NotificationController.h"

#include "nsAccessibilityService.h"
#include "nsAccUtils.h"
#include "nsCoreUtils.h"
#include "nsDocAccessible.h"
#include "nsEventShell.h"
#include "nsTextAccessible.h"


////////////////////////////////////////////////////////////////////////////////
// NotificationCollector
////////////////////////////////////////////////////////////////////////////////

NotificationController::NotificationController(nsDocAccessible* aDocument,
                                               nsIPresShell* aPresShell) :
  mObservingState(eNotObservingRefresh), mDocument(aDocument),
  mPresShell(aPresShell), mTreeConstructedState(eTreeConstructionPending)
{
  mTextHash.Init();

  // Schedule initial accessible tree construction.
  ScheduleProcessing();
}

NotificationController::~NotificationController()
{
  NS_ASSERTION(!mDocument, "Controller wasn't shutdown properly!");
  if (mDocument)
    Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: AddRef/Release and cycle collection

NS_IMPL_ADDREF(NotificationController)
NS_IMPL_RELEASE(NotificationController)

NS_IMPL_CYCLE_COLLECTION_CLASS(NotificationController)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(NotificationController)
  if (tmp->mDocument)
    tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(NotificationController)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mDocument");
  cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mDocument.get()));
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY_MEMBER(mHangingChildDocuments,
                                                    nsDocAccessible)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY_MEMBER(mContentInsertions,
                                                    ContentInsertion)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY_MEMBER(mEvents, AccEvent)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(NotificationController, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(NotificationController, Release)

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: public

void
NotificationController::Shutdown()
{
  if (mObservingState != eNotObservingRefresh &&
      mPresShell->RemoveRefreshObserver(this, Flush_Display)) {
    mObservingState = eNotObservingRefresh;
  }

  // Shutdown handling child documents.
  PRInt32 childDocCount = mHangingChildDocuments.Length();
  for (PRInt32 idx = childDocCount - 1; idx >= 0; idx--)
    mHangingChildDocuments[idx]->Shutdown();

  mHangingChildDocuments.Clear();

  mDocument = nsnull;
  mPresShell = nsnull;

  mTextHash.Clear();
  mContentInsertions.Clear();
  mNotifications.Clear();
  mEvents.Clear();
}

void
NotificationController::QueueEvent(AccEvent* aEvent)
{
  if (!mEvents.AppendElement(aEvent))
    return;

  // Filter events.
  CoalesceEvents();

  // Associate text change with hide event if it wasn't stolen from hiding
  // siblings during coalescence.
  AccMutationEvent* showOrHideEvent = downcast_accEvent(aEvent);
  if (showOrHideEvent && !showOrHideEvent->mTextChangeEvent)
    CreateTextChangeEventFor(showOrHideEvent);

  ScheduleProcessing();
}

void
NotificationController::ScheduleChildDocBinding(nsDocAccessible* aDocument)
{
  // Schedule child document binding to the tree.
  mHangingChildDocuments.AppendElement(aDocument);
  ScheduleProcessing();
}

void
NotificationController::ScheduleContentInsertion(nsAccessible* aContainer,
                                                 nsIContent* aStartChildNode,
                                                 nsIContent* aEndChildNode)
{
  // Ignore content insertions until we constructed accessible tree.
  if (mTreeConstructedState == eTreeConstructionPending)
    return;

  nsRefPtr<ContentInsertion> insertion =
    new ContentInsertion(mDocument, aContainer, aStartChildNode, aEndChildNode);

  if (insertion && mContentInsertions.AppendElement(insertion))
    ScheduleProcessing();
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: protected

void
NotificationController::ScheduleProcessing()
{
  // If notification flush isn't planed yet start notification flush
  // asynchronously (after style and layout).
  if (mObservingState == eNotObservingRefresh) {
    if (mPresShell->AddRefreshObserver(this, Flush_Display))
      mObservingState = eRefreshObserving;
  }
}

bool
NotificationController::IsUpdatePending()
{
  nsCOMPtr<nsIPresShell_MOZILLA_2_0_BRANCH2> presShell =
    do_QueryInterface(mPresShell);
  return presShell->IsLayoutFlushObserver() ||
    mObservingState == eRefreshProcessingForUpdate ||
    mContentInsertions.Length() != 0 || mNotifications.Length() != 0 ||
    mTextHash.Count() != 0;
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: private

void
NotificationController::WillRefresh(mozilla::TimeStamp aTime)
{
  // If the document accessible that notification collector was created for is
  // now shut down, don't process notifications anymore.
  NS_ASSERTION(mDocument,
               "The document was shut down while refresh observer is attached!");
  if (!mDocument)
    return;

  // Any generic notifications should be queued if we're processing content
  // insertions or generic notifications.
  mObservingState = eRefreshProcessingForUpdate;

  // Initial accessible tree construction.
  if (mTreeConstructedState == eTreeConstructionPending) {
    // If document is not bound to parent at this point then the document is not
    // ready yet (process notifications later).
    if (!mDocument->IsBoundToParent())
      return;

#ifdef DEBUG_NOTIFICATIONS
    printf("\ninitial tree created, document: %p, document node: %p\n",
           mDocument.get(), mDocument->GetDocumentNode());
#endif

    mTreeConstructedState = eTreeConstructed;
    mDocument->CacheChildrenInSubtree(mDocument);

    NS_ASSERTION(mContentInsertions.Length() == 0,
                 "Pending content insertions while initial accessible tree isn't created!");
  }

  // Process content inserted notifications to update the tree. Process other
  // notifications like DOM events and then flush event queue. If any new
  // notifications are queued during this processing then they will be processed
  // on next refresh. If notification processing queues up new events then they
  // are processed in this refresh. If events processing queues up new events
  // then new events are processed on next refresh.
  // Note: notification processing or event handling may shut down the owning
  // document accessible.

  // Process only currently queued content inserted notifications.
  nsTArray<nsRefPtr<ContentInsertion> > contentInsertions;
  contentInsertions.SwapElements(mContentInsertions);

  PRUint32 insertionCount = contentInsertions.Length();
  for (PRUint32 idx = 0; idx < insertionCount; idx++) {
    contentInsertions[idx]->Process();
    if (!mDocument)
      return;
  }

  // Process rendered text change notifications.
  mTextHash.EnumerateEntries(TextEnumerator, mDocument);
  mTextHash.Clear();

  // Bind hanging child documents.
  PRUint32 childDocCount = mHangingChildDocuments.Length();
  for (PRUint32 idx = 0; idx < childDocCount; idx++) {
    nsDocAccessible* childDoc = mHangingChildDocuments[idx];

    nsIContent* ownerContent = mDocument->GetDocumentNode()->
      FindContentForSubDocument(childDoc->GetDocumentNode());
    if (ownerContent) {
      nsAccessible* outerDocAcc = mDocument->GetAccessible(ownerContent);
      if (outerDocAcc && outerDocAcc->AppendChild(childDoc)) {
        if (mDocument->AppendChildDocument(childDoc)) {
          // Fire reorder event to notify new accessible document has been
          // attached to the tree.
          nsRefPtr<AccEvent> reorderEvent =
              new AccEvent(nsIAccessibleEvent::EVENT_REORDER, outerDocAcc,
                           eAutoDetect, AccEvent::eCoalesceFromSameSubtree);
          if (reorderEvent)
            QueueEvent(reorderEvent);

          continue;
        }
        outerDocAcc->RemoveChild(childDoc);
      }

      // Failed to bind the child document, destroy it.
      childDoc->Shutdown();
    }
  }
  mHangingChildDocuments.Clear();

  // Process only currently queued generic notifications.
  nsTArray < nsRefPtr<Notification> > notifications;
  notifications.SwapElements(mNotifications);

  PRUint32 notificationCount = notifications.Length();
  for (PRUint32 idx = 0; idx < notificationCount; idx++) {
    notifications[idx]->Process();
    if (!mDocument)
      return;
  }

  // If a generic notification occurs after this point then we may be allowed to
  // process it synchronously.
  mObservingState = eRefreshObserving;

  // Process only currently queued events.
  nsTArray<nsRefPtr<AccEvent> > events;
  events.SwapElements(mEvents);

  PRUint32 eventCount = events.Length();
  for (PRUint32 idx = 0; idx < eventCount; idx++) {
    AccEvent* accEvent = events[idx];
    if (accEvent->mEventRule != AccEvent::eDoNotEmit) {
      mDocument->ProcessPendingEvent(accEvent);

      AccMutationEvent* showOrhideEvent = downcast_accEvent(accEvent);
      if (showOrhideEvent) {
        if (showOrhideEvent->mTextChangeEvent)
          mDocument->ProcessPendingEvent(showOrhideEvent->mTextChangeEvent);
      }
    }
    if (!mDocument)
      return;
  }

  // Stop further processing if there are no newly queued insertions,
  // notifications or events.
  if (mContentInsertions.Length() == 0 && mNotifications.Length() == 0 &&
      mEvents.Length() == 0 &&
      mPresShell->RemoveRefreshObserver(this, Flush_Display)) {
    mObservingState = eNotObservingRefresh;
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationController: event queue

void
NotificationController::CoalesceEvents()
{
  PRUint32 numQueuedEvents = mEvents.Length();
  PRInt32 tail = numQueuedEvents - 1;
  AccEvent* tailEvent = mEvents[tail];

  // No node means this is application accessible (which can be a subject
  // of reorder events), we do not coalesce events for it currently.
  if (!tailEvent->mNode)
    return;

  switch(tailEvent->mEventRule) {
    case AccEvent::eCoalesceFromSameSubtree:
    {
      for (PRInt32 index = tail - 1; index >= 0; index--) {
        AccEvent* thisEvent = mEvents[index];

        if (thisEvent->mEventType != tailEvent->mEventType)
          continue; // Different type

        // Skip event for application accessible since no coalescence for it
        // is supported. Ignore events from different documents since we don't
        // coalesce them.
        if (!thisEvent->mNode ||
            thisEvent->mNode->GetOwnerDoc() != tailEvent->mNode->GetOwnerDoc())
          continue;

        // Coalesce earlier event for the same target.
        if (thisEvent->mNode == tailEvent->mNode) {
          thisEvent->mEventRule = AccEvent::eDoNotEmit;
          return;
        }

        // If event queue contains an event of the same type and having target
        // that is sibling of target of newly appended event then apply its
        // event rule to the newly appended event.

        // Coalesce hide and show events for sibling targets.
        if (tailEvent->mEventType == nsIAccessibleEvent::EVENT_HIDE) {
          AccHideEvent* tailHideEvent = downcast_accEvent(tailEvent);
          AccHideEvent* thisHideEvent = downcast_accEvent(thisEvent);
          if (thisHideEvent->mParent == tailHideEvent->mParent) {
            tailEvent->mEventRule = thisEvent->mEventRule;

            // Coalesce text change events for hide events.
            if (tailEvent->mEventRule != AccEvent::eDoNotEmit)
              CoalesceTextChangeEventsFor(tailHideEvent, thisHideEvent);

            return;
          }
        } else if (tailEvent->mEventType == nsIAccessibleEvent::EVENT_SHOW) {
          if (thisEvent->mAccessible->GetParent() ==
              tailEvent->mAccessible->GetParent()) {
            tailEvent->mEventRule = thisEvent->mEventRule;

            // Coalesce text change events for show events.
            if (tailEvent->mEventRule != AccEvent::eDoNotEmit) {
              AccShowEvent* tailShowEvent = downcast_accEvent(tailEvent);
              AccShowEvent* thisShowEvent = downcast_accEvent(thisEvent);
              CoalesceTextChangeEventsFor(tailShowEvent, thisShowEvent);
            }

            return;
          }
        }

        // Ignore events unattached from DOM since we don't coalesce them.
        if (!thisEvent->mNode->IsInDoc())
          continue;

        // Coalesce events by sibling targets (this is a case for reorder
        // events).
        if (thisEvent->mNode->GetNodeParent() ==
            tailEvent->mNode->GetNodeParent()) {
          tailEvent->mEventRule = thisEvent->mEventRule;
          return;
        }

        // This and tail events can be anywhere in the tree, make assumptions
        // for mutation events.

        // Coalesce tail event if tail node is descendant of this node. Stop
        // processing if tail event is coalesced since all possible descendants
        // of this node was coalesced before.
        // Note: more older hide event target (thisNode) can't contain recent
        // hide event target (tailNode), i.e. be ancestor of tailNode. Skip
        // this check for hide events.
        if (tailEvent->mEventType != nsIAccessibleEvent::EVENT_HIDE &&
            nsCoreUtils::IsAncestorOf(thisEvent->mNode, tailEvent->mNode)) {
          tailEvent->mEventRule = AccEvent::eDoNotEmit;
          return;
        }

        // If this node is a descendant of tail node then coalesce this event,
        // check other events in the queue. Do not emit thisEvent, also apply
        // this result to sibling nodes of thisNode.
        if (nsCoreUtils::IsAncestorOf(tailEvent->mNode, thisEvent->mNode)) {
          thisEvent->mEventRule = AccEvent::eDoNotEmit;
          ApplyToSiblings(0, index, thisEvent->mEventType,
                          thisEvent->mNode, AccEvent::eDoNotEmit);
          continue;
        }

      } // for (index)

    } break; // case eCoalesceFromSameSubtree

    case AccEvent::eCoalesceFromSameDocument:
    {
      // Used for focus event, coalesce more older event since focus event
      // for accessible can be duplicated by event for its document, we are
      // interested in focus event for accessible.
      for (PRInt32 index = tail - 1; index >= 0; index--) {
        AccEvent* thisEvent = mEvents[index];
        if (thisEvent->mEventType == tailEvent->mEventType &&
            thisEvent->mEventRule == tailEvent->mEventRule &&
            thisEvent->GetDocAccessible() == tailEvent->GetDocAccessible()) {
          thisEvent->mEventRule = AccEvent::eDoNotEmit;
          return;
        }
      }
    } break; // case eCoalesceFromSameDocument

    case AccEvent::eRemoveDupes:
    {
      // Check for repeat events, coalesce newly appended event by more older
      // event.
      for (PRInt32 index = tail - 1; index >= 0; index--) {
        AccEvent* accEvent = mEvents[index];
        if (accEvent->mEventType == tailEvent->mEventType &&
            accEvent->mEventRule == tailEvent->mEventRule &&
            accEvent->mNode == tailEvent->mNode) {
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
NotificationController::ApplyToSiblings(PRUint32 aStart, PRUint32 aEnd,
                                        PRUint32 aEventType, nsINode* aNode,
                                        AccEvent::EEventRule aEventRule)
{
  for (PRUint32 index = aStart; index < aEnd; index ++) {
    AccEvent* accEvent = mEvents[index];
    if (accEvent->mEventType == aEventType &&
        accEvent->mEventRule != AccEvent::eDoNotEmit && accEvent->mNode &&
        accEvent->mNode->GetNodeParent() == aNode->GetNodeParent()) {
      accEvent->mEventRule = aEventRule;
    }
  }
}

void
NotificationController::CoalesceTextChangeEventsFor(AccHideEvent* aTailEvent,
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
    PRUint32 oldLen = textEvent->GetLength();
    aTailEvent->mAccessible->AppendTextTo(textEvent->mModifiedText);
    textEvent->mStart -= textEvent->GetLength() - oldLen;
  }

  aTailEvent->mTextChangeEvent.swap(aThisEvent->mTextChangeEvent);
}

void
NotificationController::CoalesceTextChangeEventsFor(AccShowEvent* aTailEvent,
                                                    AccShowEvent* aThisEvent)
{
  AccTextChangeEvent* textEvent = aThisEvent->mTextChangeEvent;
  if (!textEvent)
    return;

  if (aTailEvent->mAccessible->GetIndexInParent() ==
      aThisEvent->mAccessible->GetIndexInParent() + 1) {
    // If tail target was inserted after this target, i.e. tail target is next
    // sibling of this target.
    aTailEvent->mAccessible->AppendTextTo(textEvent->mModifiedText);

  } else if (aTailEvent->mAccessible->GetIndexInParent() ==
             aThisEvent->mAccessible->GetIndexInParent() -1) {
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
NotificationController::CreateTextChangeEventFor(AccMutationEvent* aEvent)
{
  nsAccessible* container =
    GetAccService()->GetContainerAccessible(aEvent->mNode,
                                            aEvent->mAccessible->GetWeakShell());
  if (!container)
    return;

  nsHyperTextAccessible* textAccessible = container->AsHyperText();
  if (!textAccessible)
    return;

  // Don't fire event for the first html:br in an editor.
  if (aEvent->mAccessible->Role() == nsIAccessibleRole::ROLE_WHITESPACE) {
    nsCOMPtr<nsIEditor> editor;
    textAccessible->GetAssociatedEditor(getter_AddRefs(editor));
    if (editor) {
      PRBool isEmpty = PR_FALSE;
      editor->GetDocumentIsEmpty(&isEmpty);
      if (isEmpty)
        return;
    }
  }

  PRInt32 offset = textAccessible->GetChildOffset(aEvent->mAccessible);

  nsAutoString text;
  aEvent->mAccessible->AppendTextTo(text);
  if (text.IsEmpty())
    return;

  aEvent->mTextChangeEvent =
    new AccTextChangeEvent(textAccessible, offset, text, aEvent->IsShow(),
                           aEvent->mIsFromUserInput ? eFromUserInput : eNoUserInput);
}

////////////////////////////////////////////////////////////////////////////////
// Notification controller: text leaf accessible text update

/**
 * Used to find a difference between old and new text and fire text change
 * events.
 */
class TextUpdater
{
public:
  TextUpdater(nsDocAccessible* aDocument, nsTextAccessible* aTextLeaf) :
    mDocument(aDocument), mTextLeaf(aTextLeaf) { }
  ~TextUpdater() { mDocument = nsnull; mTextLeaf = nsnull; }

  /**
   * Update text of the text leaf accessible, fire text change events for its
   * container hypertext accessible.
   */
  void Run(const nsAString& aNewText);

private:
  TextUpdater();
  TextUpdater(const TextUpdater&);
  TextUpdater& operator = (const TextUpdater&);

  /**
   * Fire text change events based on difference between strings.
   */
  void FindDiffNFireEvents(const nsDependentSubstring& aStr1,
                           const nsDependentSubstring& aStr2,
                           PRUint32** aMatrix,
                           PRUint32 aStartOffset);

  /**
   * Change type used to describe the diff between strings.
   */
  enum ChangeType {
    eNoChange,
    eInsertion,
    eRemoval,
    eSubstitution
  };

  /**
   * Helper to fire text change events.
   */
  inline void MayFireEvent(nsAString* aInsertedText, nsAString* aRemovedText,
                           PRUint32 aOffset, ChangeType* aChange)
  {
    if (*aChange == eNoChange)
      return;

    if (*aChange == eRemoval || *aChange == eSubstitution) {
      FireEvent(*aRemovedText, aOffset, PR_FALSE);
      aRemovedText->Truncate();
    }

    if (*aChange == eInsertion || *aChange == eSubstitution) {
      FireEvent(*aInsertedText, aOffset, PR_TRUE);
      aInsertedText->Truncate();
    }

    *aChange = eNoChange;
  }

  /**
   * Fire text change event.
   */
  void FireEvent(const nsAString& aModText, PRUint32 aOffset, PRBool aType);

private:
  nsDocAccessible* mDocument;
  nsTextAccessible* mTextLeaf;
};

void
TextUpdater::Run(const nsAString& aNewText)
{
  NS_ASSERTION(mTextLeaf, "No text leaf accessible?");

  const nsString& oldText = mTextLeaf->Text();
  PRUint32 oldLen = oldText.Length(), newLen = aNewText.Length();
  PRUint32 minLen = oldLen < newLen ? oldLen : newLen;

  // Skip coinciding begin substrings.
  PRUint32 skipIdx = 0;
  for (; skipIdx < minLen; skipIdx++) {
    if (aNewText[skipIdx] != oldText[skipIdx])
      break;
  }

  // No change, text append or removal to/from the end.
  if (skipIdx == minLen) {
    if (oldLen == newLen)
      return;

    // If text has been appended to the end, fire text inserted event.
    if (oldLen < newLen) {
      FireEvent(Substring(aNewText, oldLen), oldLen, PR_TRUE);
      mTextLeaf->SetText(aNewText);
      return;
    }

    // Text has been removed from the end, fire text removed event.
    FireEvent(Substring(oldText, newLen), newLen, PR_FALSE);
    mTextLeaf->SetText(aNewText);
    return;
  }

  // Trim coinciding substrings from the end.
  PRUint32 endIdx = minLen;
  if (oldLen < newLen) {
    PRUint32 delta = newLen - oldLen;
    for (; endIdx > skipIdx; endIdx--) {
      if (aNewText[endIdx + delta] != oldText[endIdx])
        break;
    }
  } else {
    PRUint32 delta = oldLen - newLen;
    for (; endIdx > skipIdx; endIdx--) {
      if (aNewText[endIdx] != oldText[endIdx + delta])
        break;
    }
  }
  PRUint32 oldEndIdx = oldLen - minLen + endIdx;
  PRUint32 newEndIdx = newLen - minLen + endIdx;

  // Find the difference starting from start character, we can skip initial and
  // final coinciding characters since they don't affect on the Levenshtein
  // distance.

  const nsDependentSubstring& str1 =
    Substring(oldText, skipIdx, oldEndIdx - skipIdx);
  const nsDependentSubstring& str2 =
    Substring(aNewText, skipIdx, newEndIdx - skipIdx);

  // Compute the matrix.
  PRUint32 len1 = str1.Length() + 1, len2 = str2.Length() + 1;

  PRUint32** matrix = new PRUint32*[len1];
  for (PRUint32 i = 0; i < len1; i++)
    matrix[i] = new PRUint32[len2];

  matrix[0][0] = 0;

  for (PRUint32 i = 1; i < len1; i++)
    matrix[i][0] = i;

  for (PRUint32 j = 1; j < len2; j++)
    matrix[0][j] = j;

  for (PRUint32 i = 1; i < len1; i++) {
    for (PRUint32 j = 1; j < len2; j++) {
      if (str1[i - 1] != str2[j - 1]) {
        PRUint32 left = matrix[i - 1][j];
        PRUint32 up = matrix[i][j - 1];

        PRUint32 upleft = matrix[i - 1][j - 1];
        matrix[i][j] =
            (left < up ? (upleft < left ? upleft : left) :
                (upleft < up ? upleft : up)) + 1;
      } else {
        matrix[i][j] = matrix[i - 1][j - 1];
      }
    }
  }

  FindDiffNFireEvents(str1, str2, matrix, skipIdx);

  for (PRUint32 i = 0; i < len1; i++)
    delete[] matrix[i];
  delete[] matrix;

  mTextLeaf->SetText(aNewText);
}

void
TextUpdater::FindDiffNFireEvents(const nsDependentSubstring& aStr1,
                                 const nsDependentSubstring& aStr2,
                                 PRUint32** aMatrix,
                                 PRUint32 aStartOffset)
{
  // Find the difference.
  ChangeType change = eNoChange;
  nsAutoString insertedText;
  nsAutoString removedText;
  PRUint32 offset = 0;

  PRInt32 i = aStr1.Length(), j = aStr2.Length();
  while (i >= 0 && j >= 0) {
    if (aMatrix[i][j] == 0) {
      MayFireEvent(&insertedText, &removedText, offset + aStartOffset, &change);
      return;
    }

    // move up left
    if (i >= 1 && j >= 1) {
      // no change
      if (aStr1[i - 1] == aStr2[j - 1]) {
        MayFireEvent(&insertedText, &removedText, offset + aStartOffset, &change);

        i--; j--;
        continue;
      }

      // substitution
      if (aMatrix[i][j] == aMatrix[i - 1][j - 1] + 1) {
        if (change != eSubstitution)
          MayFireEvent(&insertedText, &removedText, offset + aStartOffset, &change);

        offset = j - 1;
        insertedText.Append(aStr1[i - 1]);
        removedText.Append(aStr2[offset]);
        change = eSubstitution;

        i--; j--;
        continue;
      }
    }

    // move up, insertion
    if (j >= 1 && aMatrix[i][j] == aMatrix[i][j - 1] + 1) {
      if (change != eInsertion)
        MayFireEvent(&insertedText, &removedText, offset + aStartOffset, &change);

      offset = j - 1;
      insertedText.Insert(aStr2[offset], 0);
      change = eInsertion;

      j--;
      continue;
    }

    // move left, removal
    if (i >= 1 && aMatrix[i][j] == aMatrix[i - 1][j] + 1) {
      if (change != eRemoval) {
        MayFireEvent(&insertedText, &removedText, offset + aStartOffset, &change);

        offset = j;
      }

      removedText.Insert(aStr1[i - 1], 0);
      change = eRemoval;

      i--;
      continue;
    }

    NS_NOTREACHED("Huh?");
    return;
  }

  MayFireEvent(&insertedText, &removedText, offset + aStartOffset, &change);
}

void
TextUpdater::FireEvent(const nsAString& aModText, PRUint32 aOffset,
                       PRBool aIsInserted)
{
  nsAccessible* parent = mTextLeaf->GetParent();
  NS_ASSERTION(parent, "No parent for text leaf!");

  nsHyperTextAccessible* hyperText = parent->AsHyperText();
  NS_ASSERTION(hyperText, "Text leaf parnet is not hyper text!");

  PRInt32 textLeafOffset = hyperText->GetChildOffset(mTextLeaf, PR_TRUE);
  NS_ASSERTION(textLeafOffset != -1,
               "Text leaf hasn't offset within hyper text!");

  // Fire text change event.
  nsRefPtr<AccEvent> textChangeEvent =
    new AccTextChangeEvent(hyperText, textLeafOffset + aOffset, aModText,
                           aIsInserted);
  mDocument->FireDelayedAccessibleEvent(textChangeEvent);

  // Fire value change event.
  if (hyperText->Role() == nsIAccessibleRole::ROLE_ENTRY) {
    nsRefPtr<AccEvent> valueChangeEvent =
      new AccEvent(nsIAccessibleEvent::EVENT_VALUE_CHANGE, hyperText,
                   eAutoDetect, AccEvent::eRemoveDupes);
    mDocument->FireDelayedAccessibleEvent(valueChangeEvent);
  }
}

PLDHashOperator
NotificationController::TextEnumerator(nsCOMPtrHashKey<nsIContent>* aEntry,
                                       void* aUserArg)
{
  nsDocAccessible* document = static_cast<nsDocAccessible*>(aUserArg);
  nsIContent* textNode = aEntry->GetKey();
  nsAccessible* textAcc = document->GetAccessible(textNode);

  // If the text node is not in tree or doesn't have frame then this case should
  // have been handled already by content removal notifications.
  nsINode* containerNode = textNode->GetNodeParent();
  if (!containerNode) {
    NS_ASSERTION(!textAcc,
                 "Text node was removed but accessible is kept alive!");
    return PL_DHASH_NEXT;
  }

  nsIFrame* textFrame = textNode->GetPrimaryFrame();
  if (!textFrame) {
    NS_ASSERTION(!textAcc,
                 "Text node isn't rendered but accessible is kept alive!");
    return PL_DHASH_NEXT;
  }

  nsIContent* containerElm = containerNode->IsElement() ?
    containerNode->AsElement() : nsnull;

  nsAutoString text;
  textFrame->GetRenderedText(&text);

  // Remove text accessible if rendered text is empty.
  if (textAcc) {
    if (text.IsEmpty()) {
#ifdef DEBUG_NOTIFICATIONS
      PRUint32 index = containerNode->IndexOf(textNode);

      nsCAutoString tag;
      nsCAutoString id;
      if (containerElm) {
        containerElm->Tag()->ToUTF8String(tag);
        nsIAtom* atomid = containerElm->GetID();
        if (atomid)
          atomid->ToUTF8String(id);
      }

      printf("\npending text node removal: container: %s@id='%s', index in container: %d\n\n",
             tag.get(), id.get(), index);
#endif

      document->ContentRemoved(containerElm, textNode);
      return PL_DHASH_NEXT;
    }

    // Update text of the accessible and fire text change events.
#ifdef DEBUG_TEXTCHANGE
      PRUint32 index = containerNode->IndexOf(textNode);

      nsCAutoString tag;
      nsCAutoString id;
      if (containerElm) {
        containerElm->Tag()->ToUTF8String(tag);
        nsIAtom* atomid = containerElm->GetID();
        if (atomid)
          atomid->ToUTF8String(id);
      }

      printf("\ntext may be changed: container: %s@id='%s', index in container: %d, old text '%s', new text: '%s'\n\n",
             tag.get(), id.get(), index,
             NS_ConvertUTF16toUTF8(textAcc->AsTextLeaf()->Text()).get(),
             NS_ConvertUTF16toUTF8(text).get());
#endif

    TextUpdater updater(document, textAcc->AsTextLeaf());
    updater.Run(text);

    return PL_DHASH_NEXT;
  }

  // Append an accessible if rendered text is not empty.
  if (!text.IsEmpty()) {
#ifdef DEBUG_NOTIFICATIONS
      PRUint32 index = containerNode->IndexOf(textNode);

      nsCAutoString tag;
      nsCAutoString id;
      if (containerElm) {
        containerElm->Tag()->ToUTF8String(tag);
        nsIAtom* atomid = containerElm->GetID();
        if (atomid)
          atomid->ToUTF8String(id);
      }

      printf("\npending text node insertion: container: %s@id='%s', index in container: %d\n\n",
             tag.get(), id.get(), index);
#endif

    nsAccessible* container = document->GetAccessibleOrContainer(containerNode);
    nsTArray<nsCOMPtr<nsIContent> > insertedContents;
    insertedContents.AppendElement(textNode);
    document->ProcessContentInserted(container, &insertedContents);
  }

  return PL_DHASH_NEXT;
}


////////////////////////////////////////////////////////////////////////////////
// NotificationController: content inserted notification

NotificationController::ContentInsertion::
  ContentInsertion(nsDocAccessible* aDocument, nsAccessible* aContainer,
                   nsIContent* aStartChildNode, nsIContent* aEndChildNode) :
  mDocument(aDocument), mContainer(aContainer)
{
  nsIContent* node = aStartChildNode;
  while (node != aEndChildNode) {
    mInsertedContent.AppendElement(node);
    node = node->GetNextSibling();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(NotificationController::ContentInsertion)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_NATIVE(NotificationController::ContentInsertion)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mContainer)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NATIVE_BEGIN(NotificationController::ContentInsertion)
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mContainer");
  cb.NoteXPCOMChild(static_cast<nsIAccessible*>(tmp->mContainer.get()));
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(NotificationController::ContentInsertion,
                                     AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(NotificationController::ContentInsertion,
                                       Release)

void
NotificationController::ContentInsertion::Process()
{
#ifdef DEBUG_NOTIFICATIONS
  nsIContent* firstChildNode = mInsertedContent[0];

  nsCAutoString tag;
  firstChildNode->Tag()->ToUTF8String(tag);

  nsIAtom* atomid = firstChildNode->GetID();
  nsCAutoString id;
  if (atomid)
    atomid->ToUTF8String(id);

  nsCAutoString ctag;
  nsCAutoString cid;
  nsIAtom* catomid = nsnull;
  if (mContainer->IsContent()) {
    mContainer->GetContent()->Tag()->ToUTF8String(ctag);
    catomid = mContainer->GetContent()->GetID();
    if (catomid)
      catomid->ToUTF8String(cid);
  }

  printf("\npending content insertion: %s@id='%s', container: %s@id='%s', inserted content amount: %d\n\n",
         tag.get(), id.get(), ctag.get(), cid.get(), mInsertedContent.Length());
#endif

  mDocument->ProcessContentInserted(mContainer, &mInsertedContent);

  mDocument = nsnull;
  mContainer = nsnull;
  mInsertedContent.Clear();
}

