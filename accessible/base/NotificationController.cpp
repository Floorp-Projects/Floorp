/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationController.h"

#include "DocAccessible-inl.h"
#include "DocAccessibleChild.h"
#include "TextLeafAccessible.h"
#include "TextUpdater.h"

#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/Element.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector
////////////////////////////////////////////////////////////////////////////////

NotificationController::NotificationController(DocAccessible* aDocument,
                                               nsIPresShell* aPresShell) :
  EventQueue(aDocument), mObservingState(eNotObservingRefresh),
  mPresShell(aPresShell), mEventGeneration(0)
{
#ifdef DEBUG
  mMoveGuardOnStack = false;
#endif

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

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(NotificationController)
NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE(NotificationController)

NS_IMPL_CYCLE_COLLECTION_CLASS(NotificationController)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(NotificationController)
  if (tmp->mDocument)
    tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(NotificationController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHangingChildDocuments)
  for (auto it = tmp->mContentInsertions.ConstIter(); !it.Done(); it.Next()) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mContentInsertions key");
    cb.NoteXPCOMChild(it.Key());
    nsTArray<nsCOMPtr<nsIContent>>* list = it.UserData();
    for (uint32_t i = 0; i < list->Length(); i++) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb,
                                         "mContentInsertions value item");
      cb.NoteXPCOMChild(list->ElementAt(i));
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvents)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRelocations)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(NotificationController, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(NotificationController, Release)

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: public

void
NotificationController::Shutdown()
{
  if (mObservingState != eNotObservingRefresh &&
      mPresShell->RemoveRefreshObserver(this, FlushType::Display)) {
    mObservingState = eNotObservingRefresh;
  }

  // Shutdown handling child documents.
  int32_t childDocCount = mHangingChildDocuments.Length();
  for (int32_t idx = childDocCount - 1; idx >= 0; idx--) {
    if (!mHangingChildDocuments[idx]->IsDefunct())
      mHangingChildDocuments[idx]->Shutdown();
  }

  mHangingChildDocuments.Clear();

  mDocument = nullptr;
  mPresShell = nullptr;

  mTextHash.Clear();
  mContentInsertions.Clear();
  mNotifications.Clear();
  mEvents.Clear();
  mRelocations.Clear();
  mEventTree.Clear();
}

EventTree*
NotificationController::QueueMutation(Accessible* aContainer)
{
  EventTree* tree = mEventTree.FindOrInsert(aContainer);
  if (tree) {
    ScheduleProcessing();
  }
  return tree;
}

bool
NotificationController::QueueMutationEvent(AccTreeMutationEvent* aEvent)
{
  // We have to allow there to be a hide and then a show event for a target
  // because of targets getting moved.  However we need to coalesce a show and
  // then a hide for a target which means we need to check for that here.
  if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_HIDE  &&
      aEvent->GetAccessible()->ShowEventTarget()) {
    AccTreeMutationEvent* showEvent = mMutationMap.GetEvent(aEvent->GetAccessible(), EventMap::ShowEvent);
    DropMutationEvent(showEvent);
    return false;
  }

  AccMutationEvent* mutEvent = downcast_accEvent(aEvent);
  mEventGeneration++;
  mutEvent->SetEventGeneration(mEventGeneration);

  if (!mFirstMutationEvent) {
    mFirstMutationEvent = aEvent;
    ScheduleProcessing();
  }

  if (mLastMutationEvent) {
    NS_ASSERTION(!mLastMutationEvent->NextEvent(), "why isn't the last event the end?");
    mLastMutationEvent->SetNextEvent(aEvent);
  }

  aEvent->SetPrevEvent(mLastMutationEvent);
  mLastMutationEvent = aEvent;
  mMutationMap.PutEvent(aEvent);

  // Because we could be hiding the target of a show event we need to get rid
  // of any such events.  It may be possible to do less than coallesce all
  // events, however that is easiest.
  if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_HIDE) {
    CoalesceMutationEvents();

    // mLastMutationEvent will point to something other than aEvent if and only
    // if aEvent was just coalesced away.  In that case a parent accessible
    // must already have the required reorder and text change events so we are
    // done here.
    if (mLastMutationEvent != aEvent) {
      return false;
    }
  }

  // We need to fire a reorder event after all of the events targeted at shown or
  // hidden children of a container.  So either queue a new one, or move an
  // existing one to the end of the queue if the container already has a
  // reorder event.
  Accessible* target = aEvent->GetAccessible();
  Accessible* container = aEvent->GetAccessible()->Parent();
  RefPtr<AccReorderEvent> reorder;
  if (!container->ReorderEventTarget()) {
    reorder = new AccReorderEvent(container);
    container->SetReorderEventTarget(true);
    mMutationMap.PutEvent(reorder);

    // Since this is the first child of container that is changing, the name of
    // container may be changing.
    QueueNameChange(target);
  } else {
    AccReorderEvent* event = downcast_accEvent(mMutationMap.GetEvent(container, EventMap::ReorderEvent));
    reorder = event;
    if (mFirstMutationEvent == event) {
      mFirstMutationEvent = event->NextEvent();
    } else {
      event->PrevEvent()->SetNextEvent(event->NextEvent());
    }

      event->NextEvent()->SetPrevEvent(event->PrevEvent());
      event->SetNextEvent(nullptr);
  }

  reorder->SetEventGeneration(mEventGeneration);
  reorder->SetPrevEvent(mLastMutationEvent);
  mLastMutationEvent->SetNextEvent(reorder);
  mLastMutationEvent = reorder;

  // It is not possible to have a text change event for something other than a
  // hyper text accessible.
  if (!container->IsHyperText()) {
    return true;
  }

  MOZ_ASSERT(mutEvent);

  nsString text;
  aEvent->GetAccessible()->AppendTextTo(text);
  if (text.IsEmpty()) {
    return true;
  }

  int32_t offset = container->AsHyperText()->GetChildOffset(target);
  AccTreeMutationEvent* prevEvent = aEvent->PrevEvent();
  while (prevEvent && prevEvent->GetEventType() == nsIAccessibleEvent::EVENT_REORDER) {
    prevEvent = prevEvent->PrevEvent();
  }

  if (prevEvent && prevEvent->GetEventType() == nsIAccessibleEvent::EVENT_HIDE &&
      mutEvent->IsHide()) {
    AccHideEvent* prevHide = downcast_accEvent(prevEvent);
    AccTextChangeEvent* prevTextChange = prevHide->mTextChangeEvent;
    if (prevTextChange && prevHide->Parent() == mutEvent->Parent()) {
      if (prevHide->mNextSibling == target) {
        target->AppendTextTo(prevTextChange->mModifiedText);
        prevHide->mTextChangeEvent.swap(mutEvent->mTextChangeEvent);
      } else if (prevHide->mPrevSibling == target) {
        nsString temp;
        target->AppendTextTo(temp);

        uint32_t extraLen = temp.Length();
        temp += prevTextChange->mModifiedText;;
        prevTextChange->mModifiedText = temp;
        prevTextChange->mStart -= extraLen;
        prevHide->mTextChangeEvent.swap(mutEvent->mTextChangeEvent);
      }
    }
  } else if (prevEvent && mutEvent->IsShow() &&
             prevEvent->GetEventType() == nsIAccessibleEvent::EVENT_SHOW) {
    AccShowEvent* prevShow = downcast_accEvent(prevEvent);
    AccTextChangeEvent* prevTextChange = prevShow->mTextChangeEvent;
    if (prevTextChange && prevShow->Parent() == target->Parent()) {
      int32_t index = target->IndexInParent();
      int32_t prevIndex = prevShow->GetAccessible()->IndexInParent();
      if (prevIndex + 1 == index) {
        target->AppendTextTo(prevTextChange->mModifiedText);
        prevShow->mTextChangeEvent.swap(mutEvent->mTextChangeEvent);
      } else if (index + 1 == prevIndex) {
        nsString temp;
        target->AppendTextTo(temp);
        prevTextChange->mStart -= temp.Length();
        temp += prevTextChange->mModifiedText;
        prevTextChange->mModifiedText = temp;
        prevShow->mTextChangeEvent.swap(mutEvent->mTextChangeEvent);
      }
    }
  }

  if (!mutEvent->mTextChangeEvent) {
    mutEvent->mTextChangeEvent =
      new AccTextChangeEvent(container, offset, text, mutEvent->IsShow(),
                             aEvent->mIsFromUserInput ? eFromUserInput : eNoUserInput);
  }

  return true;
}

void
NotificationController::DropMutationEvent(AccTreeMutationEvent* aEvent)
{
  // unset the event bits since the event isn't being fired any more.
  if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_REORDER) {
    aEvent->GetAccessible()->SetReorderEventTarget(false);
  } else if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_SHOW) {
    aEvent->GetAccessible()->SetShowEventTarget(false);
  } else {
    AccHideEvent* hideEvent = downcast_accEvent(aEvent);
    MOZ_ASSERT(hideEvent);

    if (hideEvent->NeedsShutdown()) {
      mDocument->ShutdownChildrenInSubtree(aEvent->GetAccessible());
    }
  }

  // Do the work to splice the event out of the list.
  if (mFirstMutationEvent == aEvent) {
    mFirstMutationEvent = aEvent->NextEvent();
  } else {
    aEvent->PrevEvent()->SetNextEvent(aEvent->NextEvent());
  }

  if (mLastMutationEvent == aEvent) {
    mLastMutationEvent = aEvent->PrevEvent();
  } else {
    aEvent->NextEvent()->SetPrevEvent(aEvent->PrevEvent());
  }

  aEvent->SetPrevEvent(nullptr);
  aEvent->SetNextEvent(nullptr);
  mMutationMap.RemoveEvent(aEvent);
}

void
NotificationController::CoalesceMutationEvents()
{
  AccTreeMutationEvent* event = mFirstMutationEvent;
  while (event) {
    AccTreeMutationEvent* nextEvent = event->NextEvent();
    uint32_t eventType = event->GetEventType();
    if (event->GetEventType() == nsIAccessibleEvent::EVENT_REORDER) {
      Accessible* acc = event->GetAccessible();
      while (acc) {
        if (acc->IsDoc()) {
          break;
        }

        // if a parent of the reorder event's target is being hidden that
        // hide event's target must have a parent that is also a reorder event
        // target.  That means we don't need this reorder event.
        if (acc->HideEventTarget()) {
          DropMutationEvent(event);
          break;
        }

        Accessible* parent = acc->Parent();
        if (parent->ReorderEventTarget()) {
          AccReorderEvent* reorder = downcast_accEvent(mMutationMap.GetEvent(parent, EventMap::ReorderEvent));

          // We want to make sure that a reorder event comes after any show or
          // hide events targeted at the children of its target.  We keep the
          // invariant that event generation goes up as you are farther in the
          // queue, so we want to use the spot of the event with the higher
          // generation number, and keep that generation number.
          if (reorder && reorder->EventGeneration() < event->EventGeneration()) {
            reorder->SetEventGeneration(event->EventGeneration());

            // It may be true that reorder was before event, and we coalesced
            // away all the show / hide events between them.  In that case
            // event is already immediately after reorder in the queue and we
            // do not need to rearrange the list of events.
            if (event != reorder->NextEvent()) {
              // There really should be a show or hide event before the first
              // reorder event.
              if (reorder->PrevEvent()) {
                reorder->PrevEvent()->SetNextEvent(reorder->NextEvent());
              } else {
                mFirstMutationEvent = reorder->NextEvent();
              }

              reorder->NextEvent()->SetPrevEvent(reorder->PrevEvent());
              event->PrevEvent()->SetNextEvent(reorder);
              reorder->SetPrevEvent(event->PrevEvent());
              event->SetPrevEvent(reorder);
              reorder->SetNextEvent(event);
            }
          }
          DropMutationEvent(event);
          break;
        }

        acc = parent;
      }
    } else if (eventType == nsIAccessibleEvent::EVENT_SHOW) {
      Accessible* parent = event->GetAccessible()->Parent();
      while (parent) {
        if (parent->IsDoc()) {
          break;
        }

        // if the parent of a show event is being either shown or hidden then
        // we don't need to fire a show event for a subtree of that change.
        if (parent->ShowEventTarget() || parent->HideEventTarget()) {
          DropMutationEvent(event);
          break;
        }

        parent = parent->Parent();
      }
    } else {
      MOZ_ASSERT(eventType == nsIAccessibleEvent::EVENT_HIDE, "mutation event list has an invalid event");

      AccHideEvent* hideEvent = downcast_accEvent(event);
      Accessible* parent = hideEvent->Parent();
      while (parent) {
        if (parent->IsDoc()) {
          break;
        }

        if (parent->HideEventTarget()) {
          DropMutationEvent(event);
          break;
        }

        if (parent->ShowEventTarget()) {
          AccShowEvent* showEvent = downcast_accEvent(mMutationMap.GetEvent(parent, EventMap::ShowEvent));
          if (showEvent->EventGeneration() < hideEvent->EventGeneration()) {
            DropMutationEvent(hideEvent);
            break;
          }
        }

        parent = parent->Parent();
      }
    }

    event = nextEvent;
  }
}

void
NotificationController::ScheduleChildDocBinding(DocAccessible* aDocument)
{
  // Schedule child document binding to the tree.
  mHangingChildDocuments.AppendElement(aDocument);
  ScheduleProcessing();
}

void
NotificationController::ScheduleContentInsertion(Accessible* aContainer,
                                                 nsIContent* aStartChildNode,
                                                 nsIContent* aEndChildNode)
{
  nsTArray<nsCOMPtr<nsIContent>>* list =
    mContentInsertions.LookupOrAdd(aContainer);

  bool needsProcessing = false;
  nsIContent* node = aStartChildNode;
  while (node != aEndChildNode) {
    // Notification triggers for content insertion even if no content was
    // actually inserted, check if the given content has a frame to discard
    // this case early.
    if (node->GetPrimaryFrame()) {
      if (list->AppendElement(node))
        needsProcessing = true;
    }
    node = node->GetNextSibling();
  }

  if (needsProcessing) {
    ScheduleProcessing();
  }
}

void
NotificationController::ScheduleProcessing()
{
  // If notification flush isn't planed yet start notification flush
  // asynchronously (after style and layout).
  if (mObservingState == eNotObservingRefresh) {
    if (mPresShell->AddRefreshObserver(this, FlushType::Display))
      mObservingState = eRefreshObserving;
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: protected

bool
NotificationController::IsUpdatePending()
{
  return mPresShell->IsLayoutFlushObserver() ||
    mObservingState == eRefreshProcessingForUpdate ||
    mContentInsertions.Count() != 0 || mNotifications.Length() != 0 ||
    mTextHash.Count() != 0 ||
    !mDocument->HasLoadState(DocAccessible::eTreeConstructed);
}

void
NotificationController::ProcessMutationEvents()
{
  // there is no reason to fire a hide event for a child of a show event
  // target.  That can happen if something is inserted into the tree and
  // removed before the next refresh driver tick, but it should not be
  // observable outside gecko so it should be safe to coalesce away any such
  // events.  This means that it should be fine to fire all of the hide events
  // first, and then deal with any shown subtrees.
  for (AccTreeMutationEvent* event = mFirstMutationEvent;
       event; event = event->NextEvent()) {
    if (event->GetEventType() != nsIAccessibleEvent::EVENT_HIDE) {
      continue;
    }

    nsEventShell::FireEvent(event);
    if (!mDocument) {
      return;
    }

    AccMutationEvent* mutEvent = downcast_accEvent(event);
    if (mutEvent->mTextChangeEvent) {
      nsEventShell::FireEvent(mutEvent->mTextChangeEvent);
      if (!mDocument) {
        return;
      }
    }

    // Fire menupopup end event before a hide event if a menu goes away.

    // XXX: We don't look into children of hidden subtree to find hiding
    // menupopup (as we did prior bug 570275) because we don't do that when
    // menu is showing (and that's impossible until bug 606924 is fixed).
    // Nevertheless we should do this at least because layout coalesces
    // the changes before our processing and we may miss some menupopup
    // events. Now we just want to be consistent in content insertion/removal
    // handling.
    if (event->mAccessible->ARIARole() == roles::MENUPOPUP) {
      nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_END,
                              event->mAccessible);
      if (!mDocument) {
        return;
      }
    }

    AccHideEvent* hideEvent = downcast_accEvent(event);
    if (hideEvent->NeedsShutdown()) {
      mDocument->ShutdownChildrenInSubtree(event->mAccessible);
    }
  }

  // Group the show events by the parent of their target.
  nsDataHashtable<nsPtrHashKey<Accessible>, nsTArray<AccTreeMutationEvent*>> showEvents;
  for (AccTreeMutationEvent* event = mFirstMutationEvent;
       event; event = event->NextEvent()) {
    if (event->GetEventType() != nsIAccessibleEvent::EVENT_SHOW) {
      continue;
    }

    Accessible* parent = event->GetAccessible()->Parent();
    showEvents.GetOrInsert(parent).AppendElement(event);
  }

  // We need to fire show events for the children of an accessible in the order
  // of their indices at this point.  So sort each set of events for the same
  // container by the index of their target.
  for (auto iter = showEvents.Iter(); !iter.Done(); iter.Next()) {
    struct AccIdxComparator {
      bool LessThan(const AccTreeMutationEvent* a, const AccTreeMutationEvent* b) const
      {
        int32_t aIdx = a->GetAccessible()->IndexInParent();
        int32_t bIdx = b->GetAccessible()->IndexInParent();
        MOZ_ASSERT(aIdx >= 0 && bIdx >= 0 && aIdx != bIdx);
        return aIdx < bIdx;
      }
      bool Equals(const AccTreeMutationEvent* a, const AccTreeMutationEvent* b) const
      {
        DebugOnly<int32_t> aIdx = a->GetAccessible()->IndexInParent();
        DebugOnly<int32_t> bIdx = b->GetAccessible()->IndexInParent();
        MOZ_ASSERT(aIdx >= 0 && bIdx >= 0 && aIdx != bIdx);
        return false;
      }
    };

    nsTArray<AccTreeMutationEvent*>& events = iter.Data();
    events.Sort(AccIdxComparator());
    for (AccTreeMutationEvent* event: events) {
      nsEventShell::FireEvent(event);
      if (!mDocument) {
        return;
      }

      AccMutationEvent* mutEvent = downcast_accEvent(event);
      if (mutEvent->mTextChangeEvent) {
        nsEventShell::FireEvent(mutEvent->mTextChangeEvent);
        if (!mDocument) {
          return;
        }
      }
    }
  }

  // Now we can fire the reorder events after all the show and hide events.
  for (AccTreeMutationEvent* event = mFirstMutationEvent;
       event; event = event->NextEvent()) {
    if (event->GetEventType() != nsIAccessibleEvent::EVENT_REORDER) {
      continue;
    }

    nsEventShell::FireEvent(event);
    if (!mDocument) {
      return;
    }

    Accessible* target = event->GetAccessible();
    target->Document()->MaybeNotifyOfValueChange(target);
    if (!mDocument) {
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: private

void
NotificationController::WillRefresh(mozilla::TimeStamp aTime)
{
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);
  Telemetry::AutoTimer<Telemetry::A11Y_UPDATE_TIME> updateTimer;

  // If the document accessible that notification collector was created for is
  // now shut down, don't process notifications anymore.
  NS_ASSERTION(mDocument,
               "The document was shut down while refresh observer is attached!");
  if (!mDocument)
    return;

  if (mObservingState == eRefreshProcessing ||
      mObservingState == eRefreshProcessingForUpdate)
    return;

  // Any generic notifications should be queued if we're processing content
  // insertions or generic notifications.
  mObservingState = eRefreshProcessingForUpdate;

  // Initial accessible tree construction.
  if (!mDocument->HasLoadState(DocAccessible::eTreeConstructed)) {
    // If document is not bound to parent at this point then the document is not
    // ready yet (process notifications later).
    if (!mDocument->IsBoundToParent()) {
      mObservingState = eRefreshObserving;
      return;
    }

#ifdef A11Y_LOG
    if (logging::IsEnabled(logging::eTree)) {
      logging::MsgBegin("TREE", "initial tree created");
      logging::Address("document", mDocument);
      logging::MsgEnd();
    }
#endif

    mDocument->DoInitialUpdate();

    NS_ASSERTION(mContentInsertions.Count() == 0,
                 "Pending content insertions while initial accessible tree isn't created!");
  }

  // Initialize scroll support if needed.
  if (!(mDocument->mDocFlags & DocAccessible::eScrollInitialized))
    mDocument->AddScrollListener();

  // Process rendered text change notifications.
  for (auto iter = mTextHash.Iter(); !iter.Done(); iter.Next()) {
    nsCOMPtrHashKey<nsIContent>* entry = iter.Get();
    nsIContent* textNode = entry->GetKey();
    Accessible* textAcc = mDocument->GetAccessible(textNode);

    // If the text node is not in tree or doesn't have frame then this case should
    // have been handled already by content removal notifications.
    nsINode* containerNode = textNode->GetParentNode();
    if (!containerNode) {
      NS_ASSERTION(!textAcc,
                   "Text node was removed but accessible is kept alive!");
      continue;
    }

    nsIFrame* textFrame = textNode->GetPrimaryFrame();
    if (!textFrame) {
      NS_ASSERTION(!textAcc,
                   "Text node isn't rendered but accessible is kept alive!");
      continue;
    }

    nsIContent* containerElm = containerNode->IsElement() ?
      containerNode->AsElement() : nullptr;

    nsIFrame::RenderedText text = textFrame->GetRenderedText(0,
        UINT32_MAX, nsIFrame::TextOffsetType::OFFSETS_IN_CONTENT_TEXT,
        nsIFrame::TrailingWhitespace::DONT_TRIM_TRAILING_WHITESPACE);

    // Remove text accessible if rendered text is empty.
    if (textAcc) {
      if (text.mString.IsEmpty()) {
  #ifdef A11Y_LOG
        if (logging::IsEnabled(logging::eTree | logging::eText)) {
          logging::MsgBegin("TREE", "text node lost its content; doc: %p", mDocument);
          logging::Node("container", containerElm);
          logging::Node("content", textNode);
          logging::MsgEnd();
        }
  #endif

        mDocument->ContentRemoved(textAcc);
        continue;
      }

      // Update text of the accessible and fire text change events.
  #ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eText)) {
        logging::MsgBegin("TEXT", "text may be changed; doc: %p", mDocument);
        logging::Node("container", containerElm);
        logging::Node("content", textNode);
        logging::MsgEntry("old text '%s'",
                          NS_ConvertUTF16toUTF8(textAcc->AsTextLeaf()->Text()).get());
        logging::MsgEntry("new text: '%s'",
                          NS_ConvertUTF16toUTF8(text.mString).get());
        logging::MsgEnd();
      }
  #endif

      TextUpdater::Run(mDocument, textAcc->AsTextLeaf(), text.mString);
      continue;
    }

    // Append an accessible if rendered text is not empty.
    if (!text.mString.IsEmpty()) {
  #ifdef A11Y_LOG
      if (logging::IsEnabled(logging::eTree | logging::eText)) {
        logging::MsgBegin("TREE", "text node gains new content; doc: %p", mDocument);
        logging::Node("container", containerElm);
        logging::Node("content", textNode);
        logging::MsgEnd();
      }
  #endif

      Accessible* container = mDocument->AccessibleOrTrueContainer(containerNode);
      MOZ_ASSERT(container,
                 "Text node having rendered text hasn't accessible document!");
      if (container) {
        nsTArray<nsCOMPtr<nsIContent>>* list =
          mContentInsertions.LookupOrAdd(container);
        list->AppendElement(textNode);
      }
    }
  }
  mTextHash.Clear();

  // Process content inserted notifications to update the tree.
  for (auto iter = mContentInsertions.ConstIter(); !iter.Done(); iter.Next()) {
    mDocument->ProcessContentInserted(iter.Key(), iter.UserData());
    if (!mDocument) {
      return;
    }
  }
  mContentInsertions.Clear();

  // Bind hanging child documents.
  uint32_t hangingDocCnt = mHangingChildDocuments.Length();
  nsTArray<RefPtr<DocAccessible>> newChildDocs;
  for (uint32_t idx = 0; idx < hangingDocCnt; idx++) {
    DocAccessible* childDoc = mHangingChildDocuments[idx];
    if (childDoc->IsDefunct())
      continue;

    nsIContent* ownerContent = mDocument->DocumentNode()->
      FindContentForSubDocument(childDoc->DocumentNode());
    if (ownerContent) {
      Accessible* outerDocAcc = mDocument->GetAccessible(ownerContent);
      if (outerDocAcc && outerDocAcc->AppendChild(childDoc)) {
        if (mDocument->AppendChildDocument(childDoc)) {
          newChildDocs.AppendElement(Move(mHangingChildDocuments[idx]));
          continue;
        }

        outerDocAcc->RemoveChild(childDoc);
      }

      // Failed to bind the child document, destroy it.
      childDoc->Shutdown();
    }
  }
  mHangingChildDocuments.Clear();
  MOZ_ASSERT(mDocument, "Illicit document shutdown");
  if (!mDocument) {
    return;
  }

  // If the document is ready and all its subdocuments are completely loaded
  // then process the document load.
  if (mDocument->HasLoadState(DocAccessible::eReady) &&
      !mDocument->HasLoadState(DocAccessible::eCompletelyLoaded) &&
      hangingDocCnt == 0) {
    uint32_t childDocCnt = mDocument->ChildDocumentCount(), childDocIdx = 0;
    for (; childDocIdx < childDocCnt; childDocIdx++) {
      DocAccessible* childDoc = mDocument->GetChildDocumentAt(childDocIdx);
      if (!childDoc->HasLoadState(DocAccessible::eCompletelyLoaded))
        break;
    }

    if (childDocIdx == childDocCnt) {
      mDocument->ProcessLoad();
      if (!mDocument)
        return;
    }
  }

  // Process only currently queued generic notifications.
  nsTArray < RefPtr<Notification> > notifications;
  notifications.SwapElements(mNotifications);

  uint32_t notificationCount = notifications.Length();
  for (uint32_t idx = 0; idx < notificationCount; idx++) {
    notifications[idx]->Process();
    if (!mDocument)
      return;
  }

  // Process invalidation list of the document after all accessible tree
  // modification are done.
  mDocument->ProcessInvalidationList();

  // We cannot rely on DOM tree to keep aria-owns relations updated. Make
  // a validation to remove dead links.
  mDocument->ValidateARIAOwned();

  // Process relocation list.
  for (uint32_t idx = 0; idx < mRelocations.Length(); idx++) {
    if (mRelocations[idx]->IsInDocument()) {
      mDocument->DoARIAOwnsRelocation(mRelocations[idx]);
    }
  }
  mRelocations.Clear();

  // If a generic notification occurs after this point then we may be allowed to
  // process it synchronously.  However we do not want to reenter if fireing
  // events causes script to run.
  mObservingState = eRefreshProcessing;

  CoalesceMutationEvents();
  ProcessMutationEvents();
  mEventGeneration = 0;

  // Now that we are done with them get rid of the events we fired.
  RefPtr<AccTreeMutationEvent> mutEvent = Move(mFirstMutationEvent);
  mLastMutationEvent = nullptr;
  mFirstMutationEvent = nullptr;
  while (mutEvent) {
    RefPtr<AccTreeMutationEvent> nextEvent = mutEvent->NextEvent();
    Accessible* target = mutEvent->GetAccessible();

    // We need to be careful here, while it may seem that we can simply 0 all
    // the pending event bits that is not true.  Because accessibles may be
    // reparented they may be the target of both a hide event and a show event
    // at the same time.
    if (mutEvent->GetEventType() == nsIAccessibleEvent::EVENT_SHOW) {
      target->SetShowEventTarget(false);
    }

    if (mutEvent->GetEventType() == nsIAccessibleEvent::EVENT_HIDE) {
      target->SetHideEventTarget(false);
    }

    // However it is not possible for a reorder event target to also be the
    // target of a show or hide, so we can just zero that.
    target->SetReorderEventTarget(false);

    mutEvent->SetPrevEvent(nullptr);
    mutEvent->SetNextEvent(nullptr);
    mMutationMap.RemoveEvent(mutEvent);
    mutEvent = nextEvent;
  }

  ProcessEventQueue();

  if (IPCAccessibilityActive()) {
    size_t newDocCount = newChildDocs.Length();
    for (size_t i = 0; i < newDocCount; i++) {
      DocAccessible* childDoc = newChildDocs[i];
      if (childDoc->IsDefunct()) {
        continue;
      }

      Accessible* parent = childDoc->Parent();
      DocAccessibleChild* parentIPCDoc = mDocument->IPCDoc();
      MOZ_DIAGNOSTIC_ASSERT(parentIPCDoc);
      uint64_t id = reinterpret_cast<uintptr_t>(parent->UniqueID());
      MOZ_DIAGNOSTIC_ASSERT(id);
      DocAccessibleChild* ipcDoc = childDoc->IPCDoc();
      if (ipcDoc) {
        parentIPCDoc->SendBindChildDoc(ipcDoc, id);
        continue;
      }

      ipcDoc = new DocAccessibleChild(childDoc, parentIPCDoc->Manager());
      childDoc->SetIPCDoc(ipcDoc);

#if defined(XP_WIN)
      parentIPCDoc->ConstructChildDocInParentProcess(ipcDoc, id,
                                                     AccessibleWrap::GetChildIDFor(childDoc));
#else
      nsCOMPtr<nsITabChild> tabChild =
        do_GetInterface(mDocument->DocumentNode()->GetDocShell());
      if (tabChild) {
        static_cast<TabChild*>(tabChild.get())->
          SendPDocAccessibleConstructor(ipcDoc, parentIPCDoc, id, 0, 0);
      }
#endif
    }
  }

  mObservingState = eRefreshObserving;
  if (!mDocument)
    return;

  // Stop further processing if there are no new notifications of any kind or
  // events and document load is processed.
  if (mContentInsertions.Count() == 0 && mNotifications.IsEmpty() &&
      mEvents.IsEmpty() && mTextHash.Count() == 0 &&
      mHangingChildDocuments.IsEmpty() &&
      mDocument->HasLoadState(DocAccessible::eCompletelyLoaded) &&
      mPresShell->RemoveRefreshObserver(this, FlushType::Display)) {
    mObservingState = eNotObservingRefresh;
  }
}

void
NotificationController::EventMap::PutEvent(AccTreeMutationEvent* aEvent)
{
  EventType type = GetEventType(aEvent);
  uint64_t addr = reinterpret_cast<uintptr_t>(aEvent->GetAccessible());
  MOZ_ASSERT((addr & 0x3) == 0, "accessible is not 4 byte aligned");
  addr |= type;
  mTable.Put(addr, aEvent);
}

AccTreeMutationEvent*
NotificationController::EventMap::GetEvent(Accessible* aTarget, EventType aType)
{
  uint64_t addr = reinterpret_cast<uintptr_t>(aTarget);
  MOZ_ASSERT((addr & 0x3) == 0, "target is not 4 byte aligned");

  addr |= aType;
  return mTable.GetWeak(addr);
}

void
NotificationController::EventMap::RemoveEvent(AccTreeMutationEvent* aEvent)
{
  EventType type = GetEventType(aEvent);
  uint64_t addr = reinterpret_cast<uintptr_t>(aEvent->GetAccessible());
  MOZ_ASSERT((addr & 0x3) == 0, "accessible is not 4 byte aligned");
  addr |= type;

  MOZ_ASSERT(mTable.GetWeak(addr) == aEvent, "mTable has the wrong event");
  mTable.Remove(addr);
}

  NotificationController::EventMap::EventType
NotificationController::EventMap::GetEventType(AccTreeMutationEvent* aEvent)
{
  switch(aEvent->GetEventType())
  {
    case nsIAccessibleEvent::EVENT_SHOW:
      return ShowEvent;
    case nsIAccessibleEvent::EVENT_HIDE:
      return HideEvent;
    case nsIAccessibleEvent::EVENT_REORDER:
      return ReorderEvent;
    default:
      MOZ_ASSERT_UNREACHABLE("event has invalid type");
      return ShowEvent;
  }
}
