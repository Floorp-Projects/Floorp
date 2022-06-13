/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NotificationController.h"

#include "DocAccessible-inl.h"
#include "DocAccessibleChild.h"
#include "nsEventShell.h"
#include "TextLeafAccessible.h"
#include "TextUpdater.h"

#include "nsIContentInlines.h"

#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/Element.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/StaticPrefs_accessibility.h"
#include "mozilla/Telemetry.h"

using namespace mozilla;
using namespace mozilla::a11y;
using namespace mozilla::dom;

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector
////////////////////////////////////////////////////////////////////////////////

NotificationController::NotificationController(DocAccessible* aDocument,
                                               PresShell* aPresShell)
    : EventQueue(aDocument),
      mObservingState(eNotObservingRefresh),
      mPresShell(aPresShell),
      mEventGeneration(0) {
#ifdef DEBUG
  mMoveGuardOnStack = false;
#endif

  // Schedule initial accessible tree construction.
  ScheduleProcessing();
}

NotificationController::~NotificationController() {
  NS_ASSERTION(!mDocument, "Controller wasn't shutdown properly!");
  if (mDocument) Shutdown();
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: AddRef/Release and cycle collection

NS_IMPL_CYCLE_COLLECTING_NATIVE_ADDREF(NotificationController)
NS_IMPL_CYCLE_COLLECTING_NATIVE_RELEASE(NotificationController)

NS_IMPL_CYCLE_COLLECTION_CLASS(NotificationController)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(NotificationController)
  if (tmp->mDocument) tmp->Shutdown();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(NotificationController)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHangingChildDocuments)
  for (const auto& entry : tmp->mContentInsertions) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mContentInsertions key");
    cb.NoteXPCOMChild(entry.GetKey());
    nsTArray<nsCOMPtr<nsIContent>>* list = entry.GetData().get();
    for (uint32_t i = 0; i < list->Length(); i++) {
      NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mContentInsertions value item");
      cb.NoteXPCOMChild(list->ElementAt(i));
    }
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFocusEvent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mEvents)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRelocations)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(NotificationController, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(NotificationController, Release)

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: public

void NotificationController::Shutdown() {
  if (mObservingState != eNotObservingRefresh &&
      mPresShell->RemoveRefreshObserver(this, FlushType::Display) &&
      mPresShell->RemovePostRefreshObserver(this)) {
    mObservingState = eNotObservingRefresh;
  }

  // Shutdown handling child documents.
  int32_t childDocCount = mHangingChildDocuments.Length();
  for (int32_t idx = childDocCount - 1; idx >= 0; idx--) {
    if (!mHangingChildDocuments[idx]->IsDefunct()) {
      mHangingChildDocuments[idx]->Shutdown();
    }
  }

  mHangingChildDocuments.Clear();

  mDocument = nullptr;
  mPresShell = nullptr;

  mTextHash.Clear();
  mContentInsertions.Clear();
  mNotifications.Clear();
  mFocusEvent = nullptr;
  mEvents.Clear();
  mRelocations.Clear();
  mEventTree.Clear();
}

EventTree* NotificationController::QueueMutation(LocalAccessible* aContainer) {
  EventTree* tree = mEventTree.FindOrInsert(aContainer);
  if (tree) {
    ScheduleProcessing();
  }
  return tree;
}

void NotificationController::CoalesceHideEvent(AccHideEvent* aHideEvent) {
  LocalAccessible* parent = aHideEvent->LocalParent();
  while (parent) {
    if (parent->IsDoc()) {
      break;
    }

    if (parent->HideEventTarget()) {
      DropMutationEvent(aHideEvent);
      break;
    }

    if (parent->ShowEventTarget()) {
      AccShowEvent* showEvent =
          downcast_accEvent(mMutationMap.GetEvent(parent, EventMap::ShowEvent));
      if (showEvent->EventGeneration() < aHideEvent->EventGeneration()) {
        DropMutationEvent(aHideEvent);
        break;
      }
    }

    parent = parent->LocalParent();
  }
}

bool NotificationController::QueueMutationEvent(AccTreeMutationEvent* aEvent) {
  if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_HIDE) {
    // We have to allow there to be a hide and then a show event for a target
    // because of targets getting moved.  However we need to coalesce a show and
    // then a hide for a target which means we need to check for that here.
    if (aEvent->GetAccessible()->ShowEventTarget()) {
      AccTreeMutationEvent* showEvent =
          mMutationMap.GetEvent(aEvent->GetAccessible(), EventMap::ShowEvent);
      DropMutationEvent(showEvent);
      return false;
    }

    // If this is an additional hide event, the accessible may be hidden, or
    // moved again after a move. Preserve the original hide event since
    // its properties are consistent with the tree that existed before
    // the next batch of mutation events is processed.
    if (aEvent->GetAccessible()->HideEventTarget()) {
      return false;
    }
  }

  AccMutationEvent* mutEvent = downcast_accEvent(aEvent);
  mEventGeneration++;
  mutEvent->SetEventGeneration(mEventGeneration);

  if (!mFirstMutationEvent) {
    mFirstMutationEvent = aEvent;
    ScheduleProcessing();
  }

  if (mLastMutationEvent) {
    NS_ASSERTION(!mLastMutationEvent->NextEvent(),
                 "why isn't the last event the end?");
    mLastMutationEvent->SetNextEvent(aEvent);
  }

  aEvent->SetPrevEvent(mLastMutationEvent);
  mLastMutationEvent = aEvent;
  mMutationMap.PutEvent(aEvent);

  // Because we could be hiding the target of a show event we need to get rid
  // of any such events.
  if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_HIDE) {
    CoalesceHideEvent(downcast_accEvent(aEvent));

    // mLastMutationEvent will point to something other than aEvent if and only
    // if aEvent was just coalesced away.  In that case a parent accessible
    // must already have the required reorder and text change events so we are
    // done here.
    if (mLastMutationEvent != aEvent) {
      return false;
    }
  }

  // We need to fire a reorder event after all of the events targeted at shown
  // or hidden children of a container.  So either queue a new one, or move an
  // existing one to the end of the queue if the container already has a
  // reorder event.
  LocalAccessible* target = aEvent->GetAccessible();
  LocalAccessible* container = aEvent->GetAccessible()->LocalParent();
  RefPtr<AccReorderEvent> reorder;
  if (!container->ReorderEventTarget()) {
    reorder = new AccReorderEvent(container);
    container->SetReorderEventTarget(true);
    mMutationMap.PutEvent(reorder);

    // Since this is the first child of container that is changing, the name
    // and/or description of dependent Accessibles may be changing.
    if (PushNameOrDescriptionChange(target)) {
      ScheduleProcessing();
    }
  } else {
    AccReorderEvent* event = downcast_accEvent(
        mMutationMap.GetEvent(container, EventMap::ReorderEvent));
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
  while (prevEvent &&
         prevEvent->GetEventType() == nsIAccessibleEvent::EVENT_REORDER) {
    prevEvent = prevEvent->PrevEvent();
  }

  if (prevEvent &&
      prevEvent->GetEventType() == nsIAccessibleEvent::EVENT_HIDE &&
      mutEvent->IsHide()) {
    AccHideEvent* prevHide = downcast_accEvent(prevEvent);
    AccTextChangeEvent* prevTextChange = prevHide->mTextChangeEvent;
    if (prevTextChange && prevHide->LocalParent() == mutEvent->LocalParent()) {
      if (prevHide->mNextSibling == target) {
        target->AppendTextTo(prevTextChange->mModifiedText);
        prevHide->mTextChangeEvent.swap(mutEvent->mTextChangeEvent);
      } else if (prevHide->mPrevSibling == target) {
        nsString temp;
        target->AppendTextTo(temp);

        uint32_t extraLen = temp.Length();
        temp += prevTextChange->mModifiedText;
        ;
        prevTextChange->mModifiedText = temp;
        prevTextChange->mStart -= extraLen;
        prevHide->mTextChangeEvent.swap(mutEvent->mTextChangeEvent);
      }
    }
  } else if (prevEvent && mutEvent->IsShow() &&
             prevEvent->GetEventType() == nsIAccessibleEvent::EVENT_SHOW) {
    AccShowEvent* prevShow = downcast_accEvent(prevEvent);
    AccTextChangeEvent* prevTextChange = prevShow->mTextChangeEvent;
    if (prevTextChange && prevShow->LocalParent() == target->LocalParent()) {
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
    mutEvent->mTextChangeEvent = new AccTextChangeEvent(
        container, offset, text, mutEvent->IsShow(),
        aEvent->mIsFromUserInput ? eFromUserInput : eNoUserInput);
  }

  return true;
}

void NotificationController::DropMutationEvent(AccTreeMutationEvent* aEvent) {
  if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_REORDER) {
    // We don't fully drop reorder events, we just change them to inner reorder
    // events.
    AccReorderEvent* reorderEvent = downcast_accEvent(aEvent);

    MOZ_ASSERT(reorderEvent);
    reorderEvent->SetInner();
    return;
  } else if (aEvent->GetEventType() == nsIAccessibleEvent::EVENT_SHOW) {
    // unset the event bits since the event isn't being fired any more.
    aEvent->GetAccessible()->SetShowEventTarget(false);
  } else {
    // unset the event bits since the event isn't being fired any more.
    aEvent->GetAccessible()->SetHideEventTarget(false);

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

void NotificationController::CoalesceMutationEvents() {
  AccTreeMutationEvent* event = mFirstMutationEvent;
  while (event) {
    AccTreeMutationEvent* nextEvent = event->NextEvent();
    uint32_t eventType = event->GetEventType();
    if (event->GetEventType() == nsIAccessibleEvent::EVENT_REORDER) {
      LocalAccessible* acc = event->GetAccessible();
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

        LocalAccessible* parent = acc->LocalParent();
        if (parent && parent->ReorderEventTarget()) {
          AccReorderEvent* reorder = downcast_accEvent(
              mMutationMap.GetEvent(parent, EventMap::ReorderEvent));

          // We want to make sure that a reorder event comes after any show or
          // hide events targeted at the children of its target.  We keep the
          // invariant that event generation goes up as you are farther in the
          // queue, so we want to use the spot of the event with the higher
          // generation number, and keep that generation number.
          if (reorder &&
              reorder->EventGeneration() < event->EventGeneration()) {
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
      LocalAccessible* parent = event->GetAccessible()->LocalParent();
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

        parent = parent->LocalParent();
      }
    } else if (eventType == nsIAccessibleEvent::EVENT_HIDE) {
      MOZ_ASSERT(eventType == nsIAccessibleEvent::EVENT_HIDE,
                 "mutation event list has an invalid event");

      AccHideEvent* hideEvent = downcast_accEvent(event);
      CoalesceHideEvent(hideEvent);
    }

    event = nextEvent;
  }
}

void NotificationController::ScheduleChildDocBinding(DocAccessible* aDocument) {
  // Schedule child document binding to the tree.
  mHangingChildDocuments.AppendElement(aDocument);
  ScheduleProcessing();
}

void NotificationController::ScheduleContentInsertion(
    LocalAccessible* aContainer, nsTArray<nsCOMPtr<nsIContent>>& aInsertions) {
  if (!aInsertions.IsEmpty()) {
    mContentInsertions.GetOrInsertNew(aContainer)->AppendElements(aInsertions);
    ScheduleProcessing();
  }
}

void NotificationController::ScheduleProcessing() {
  // If notification flush isn't planed yet start notification flush
  // asynchronously (after style and layout).
  if (mObservingState == eNotObservingRefresh) {
    if (mPresShell->AddRefreshObserver(this, FlushType::Display,
                                       "Accessibility notifications") &&
        mPresShell->AddPostRefreshObserver(this)) {
      mObservingState = eRefreshObserving;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: protected

bool NotificationController::IsUpdatePending() {
  return mPresShell->IsLayoutFlushObserver() ||
         mObservingState == eRefreshProcessingForUpdate || WaitingForParent() ||
         mContentInsertions.Count() != 0 || mNotifications.Length() != 0 ||
         mTextHash.Count() != 0 ||
         !mDocument->HasLoadState(DocAccessible::eTreeConstructed);
}

bool NotificationController::WaitingForParent() {
  DocAccessible* parentdoc = mDocument->ParentDocument();
  if (!parentdoc) {
    return false;
  }

  NotificationController* parent = parentdoc->mNotificationController;
  if (!parent || parent == this) {
    // Do not wait for nothing or ourselves
    return false;
  }

  // Wait for parent's notifications processing
  return parent->mContentInsertions.Count() != 0 ||
         parent->mNotifications.Length() != 0;
}

void NotificationController::ProcessMutationEvents() {
  // there is no reason to fire a hide event for a child of a show event
  // target.  That can happen if something is inserted into the tree and
  // removed before the next refresh driver tick, but it should not be
  // observable outside gecko so it should be safe to coalesce away any such
  // events.  This means that it should be fine to fire all of the hide events
  // first, and then deal with any shown subtrees.
  for (AccTreeMutationEvent* event = mFirstMutationEvent; event;
       event = event->NextEvent()) {
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
  nsTHashMap<nsPtrHashKey<LocalAccessible>, nsTArray<AccTreeMutationEvent*>>
      showEvents;
  for (AccTreeMutationEvent* event = mFirstMutationEvent; event;
       event = event->NextEvent()) {
    if (event->GetEventType() != nsIAccessibleEvent::EVENT_SHOW) {
      continue;
    }

    LocalAccessible* parent = event->GetAccessible()->LocalParent();
    showEvents.LookupOrInsert(parent).AppendElement(event);
  }

  // We need to fire show events for the children of an accessible in the order
  // of their indices at this point.  So sort each set of events for the same
  // container by the index of their target.
  for (auto iter = showEvents.Iter(); !iter.Done(); iter.Next()) {
    struct AccIdxComparator {
      bool LessThan(const AccTreeMutationEvent* a,
                    const AccTreeMutationEvent* b) const {
        int32_t aIdx = a->GetAccessible()->IndexInParent();
        int32_t bIdx = b->GetAccessible()->IndexInParent();
        MOZ_ASSERT(aIdx >= 0 && bIdx >= 0 && aIdx != bIdx);
        return aIdx < bIdx;
      }
      bool Equals(const AccTreeMutationEvent* a,
                  const AccTreeMutationEvent* b) const {
        DebugOnly<int32_t> aIdx = a->GetAccessible()->IndexInParent();
        DebugOnly<int32_t> bIdx = b->GetAccessible()->IndexInParent();
        MOZ_ASSERT(aIdx >= 0 && bIdx >= 0 && aIdx != bIdx);
        return false;
      }
    };

    nsTArray<AccTreeMutationEvent*>& events = iter.Data();
    events.Sort(AccIdxComparator());
    for (AccTreeMutationEvent* event : events) {
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
  for (const uint32_t reorderType : {nsIAccessibleEvent::EVENT_INNER_REORDER,
                                     nsIAccessibleEvent::EVENT_REORDER}) {
    for (AccTreeMutationEvent* event = mFirstMutationEvent; event;
         event = event->NextEvent()) {
      if (event->GetEventType() != reorderType) {
        continue;
      }

      if (event->GetAccessible()->IsDefunct()) {
        // An inner reorder target may have been hidden itself and no
        // longer bound to the document.
        MOZ_ASSERT(reorderType == nsIAccessibleEvent::EVENT_INNER_REORDER,
                   "An 'outer' reorder target should not be defunct");
        continue;
      }

      nsEventShell::FireEvent(event);
      if (!mDocument) {
        return;
      }

      LocalAccessible* target = event->GetAccessible();
      target->Document()->MaybeNotifyOfValueChange(target);
      if (!mDocument) {
        return;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationCollector: private

void NotificationController::WillRefresh(mozilla::TimeStamp aTime) {
  Telemetry::AutoTimer<Telemetry::A11Y_TREE_UPDATE_TIMING_MS> timer;

  AUTO_PROFILER_LABEL("NotificationController::WillRefresh", OTHER);

  // If the document accessible that notification collector was created for is
  // now shut down, don't process notifications anymore.
  NS_ASSERTION(
      mDocument,
      "The document was shut down while refresh observer is attached!");
  if (!mDocument) return;

  // Wait until an update, we have started, or an interruptible reflow is
  // finished.
  if (mObservingState == eRefreshProcessing ||
      mObservingState == eRefreshProcessingForUpdate ||
      mPresShell->IsReflowInterrupted()) {
    return;
  }

  // Process parent's notifications before ours, to get proper ordering between
  // e.g. tab event and content event.
  if (WaitingForParent()) {
    mDocument->ParentDocument()->mNotificationController->WillRefresh(aTime);
    if (!mDocument) {
      return;
    }
  }

  // Any generic notifications should be queued if we're processing content
  // insertions or generic notifications.
  mObservingState = eRefreshProcessingForUpdate;

  // Initial accessible tree construction.
  if (!mDocument->HasLoadState(DocAccessible::eTreeConstructed)) {
    // (1) If document is not bound to parent at this point, or
    // (2) the PresShell is not initialized (and it isn't about:blank),
    // then the document is not ready yet (process notifications later).
    if (!mDocument->IsBoundToParent() ||
        (!mPresShell->DidInitialize() &&
         !mDocument->DocumentNode()->IsInitialDocument())) {
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
                 "Pending content insertions while initial accessible tree "
                 "isn't created!");
  }

  mDocument->ProcessPendingUpdates();

  // Process rendered text change notifications.
  for (nsIContent* textNode : mTextHash) {
    LocalAccessible* textAcc = mDocument->GetAccessible(textNode);

    // If the text node is not in tree or doesn't have a frame, or placed in
    // another document, then this case should have been handled already by
    // content removal notifications.
    nsINode* containerNode = textNode->GetFlattenedTreeParentNode();
    if (!containerNode || textNode->OwnerDoc() != mDocument->DocumentNode()) {
      MOZ_ASSERT(!textAcc,
                 "Text node was removed but accessible is kept alive!");
      continue;
    }

    nsIFrame* textFrame = textNode->GetPrimaryFrame();
    if (!textFrame) {
      MOZ_ASSERT(!textAcc,
                 "Text node isn't rendered but accessible is kept alive!");
      continue;
    }

#ifdef A11Y_LOG
    nsIContent* containerElm =
        containerNode->IsElement() ? containerNode->AsElement() : nullptr;
#endif

    nsIFrame::RenderedText text = textFrame->GetRenderedText(
        0, UINT32_MAX, nsIFrame::TextOffsetType::OffsetsInContentText,
        nsIFrame::TrailingWhitespace::DontTrim);

    // Remove text accessible if rendered text is empty.
    if (textAcc) {
      if (text.mString.IsEmpty()) {
#ifdef A11Y_LOG
        if (logging::IsEnabled(logging::eTree | logging::eText)) {
          logging::MsgBegin("TREE", "text node lost its content; doc: %p",
                            mDocument);
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
        logging::MsgEntry(
            "old text '%s'",
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
        logging::MsgBegin("TREE", "text node gains new content; doc: %p",
                          mDocument);
        logging::Node("container", containerElm);
        logging::Node("content", textNode);
        logging::MsgEnd();
      }
#endif

      MOZ_ASSERT(mDocument->AccessibleOrTrueContainer(containerNode),
                 "Text node having rendered text hasn't accessible document!");

      LocalAccessible* container =
          mDocument->AccessibleOrTrueContainer(containerNode, true);
      if (container) {
        nsTArray<nsCOMPtr<nsIContent>>* list =
            mContentInsertions.GetOrInsertNew(container);
        list->AppendElement(textNode);
      }
    }
  }
  mTextHash.Clear();

  // Process content inserted notifications to update the tree.
  // Processing an insertion can indirectly run script (e.g. querying a XUL
  // interface), which might result in another insertion being queued.
  // We don't want to lose any queued insertions if this happens. Therefore, we
  // move the current insertions into a temporary data structure and process
  // them from there. Any insertions queued during processing will get handled
  // in subsequent refresh driver ticks.
  const auto contentInsertions = std::move(mContentInsertions);
  for (const auto& entry : contentInsertions) {
    mDocument->ProcessContentInserted(entry.GetKey(), entry.GetData().get());
    if (!mDocument) {
      return;
    }
  }

  // Bind hanging child documents unless we are using IPC and the
  // document has no IPC actor.  If we fail to bind the child doc then
  // shut it down.
  uint32_t hangingDocCnt = mHangingChildDocuments.Length();
  nsTArray<RefPtr<DocAccessible>> newChildDocs;
  for (uint32_t idx = 0; idx < hangingDocCnt; idx++) {
    DocAccessible* childDoc = mHangingChildDocuments[idx];
    if (childDoc->IsDefunct()) continue;

    if (IPCAccessibilityActive() && !mDocument->IPCDoc()) {
      childDoc->Shutdown();
      continue;
    }

    nsIContent* ownerContent = childDoc->DocumentNode()->GetEmbedderElement();
    if (ownerContent) {
      LocalAccessible* outerDocAcc = mDocument->GetAccessible(ownerContent);
      if (outerDocAcc && outerDocAcc->AppendChild(childDoc)) {
        if (mDocument->AppendChildDocument(childDoc)) {
          newChildDocs.AppendElement(std::move(mHangingChildDocuments[idx]));
          continue;
        }

        outerDocAcc->RemoveChild(childDoc);
      }

      // Failed to bind the child document, destroy it.
      childDoc->Shutdown();
    }
  }

  // Clear the hanging documents list, even if we didn't bind them.
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
      if (!childDoc->HasLoadState(DocAccessible::eCompletelyLoaded)) break;
    }

    if (childDocIdx == childDocCnt) {
      mDocument->ProcessLoad();
      if (!mDocument) return;
    }
  }

  // Process invalidation list of the document after all accessible tree
  // mutation is done.
  mDocument->ProcessInvalidationList();

  // Process relocation list.
  for (uint32_t idx = 0; idx < mRelocations.Length(); idx++) {
    // owner should be in a document and have na associated DOM node (docs
    // sometimes don't)
    if (mRelocations[idx]->IsInDocument() &&
        mRelocations[idx]->HasOwnContent()) {
      mDocument->DoARIAOwnsRelocation(mRelocations[idx]);
    }
  }
  mRelocations.Clear();

  // Process only currently queued generic notifications.
  // These are used for processing aria-activedescendant, DOMMenuItemActive,
  // etc. Therefore, they must be processed after relocations, since relocated
  // subtrees might not have been created before relocation processing and the
  // target might be inside a relocated subtree.
  const nsTArray<RefPtr<Notification>> notifications =
      std::move(mNotifications);

  uint32_t notificationCount = notifications.Length();
  for (uint32_t idx = 0; idx < notificationCount; idx++) {
    notifications[idx]->Process();
    if (!mDocument) return;
  }

  // If a generic notification occurs after this point then we may be allowed to
  // process it synchronously.  However we do not want to reenter if fireing
  // events causes script to run.
  mObservingState = eRefreshProcessing;

  mDocument->SendAccessiblesWillMove();

  // Send any queued cache updates before we fire any mutation events so the
  // cache is up to date when mutation events are fired. We do this after
  // insertions (but not their events) so that cache updates dependent on the
  // tree work correctly; e.g. line start calculation.
  if (IPCAccessibilityActive() && mDocument) {
    mDocument->ProcessQueuedCacheUpdates();
  }

  CoalesceMutationEvents();
  ProcessMutationEvents();
  mEventGeneration = 0;

  // Now that we are done with them get rid of the events we fired.
  RefPtr<AccTreeMutationEvent> mutEvent = std::move(mFirstMutationEvent);
  mLastMutationEvent = nullptr;
  mFirstMutationEvent = nullptr;
  while (mutEvent) {
    RefPtr<AccTreeMutationEvent> nextEvent = mutEvent->NextEvent();
    LocalAccessible* target = mutEvent->GetAccessible();

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

  if (mDocument) {
    mDocument->ClearMovedAccessibles();
  }

  ProcessEventQueue();

  if (IPCAccessibilityActive()) {
    size_t newDocCount = newChildDocs.Length();
    for (size_t i = 0; i < newDocCount; i++) {
      DocAccessible* childDoc = newChildDocs[i];
      if (childDoc->IsDefunct()) {
        continue;
      }

      LocalAccessible* parent = childDoc->LocalParent();
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
      parentIPCDoc->ConstructChildDocInParentProcess(
          ipcDoc, id,
          StaticPrefs::accessibility_cache_enabled_AtStartup()
              ? 0
              : MsaaAccessible::GetChildIDFor(childDoc));
#else
      nsCOMPtr<nsIBrowserChild> browserChild =
          do_GetInterface(mDocument->DocumentNode()->GetDocShell());
      if (browserChild) {
        static_cast<BrowserChild*>(browserChild.get())
            ->SendPDocAccessibleConstructor(
                ipcDoc, parentIPCDoc, id,
                childDoc->DocumentNode()->GetBrowsingContext(), 0, 0);
        ipcDoc->SendPDocAccessiblePlatformExtConstructor();
      }
#endif
    }
  }

  mObservingState = eRefreshObserving;
  if (!mDocument) return;

  // Stop further processing if there are no new notifications of any kind or
  // events and document load is processed.
  if (mContentInsertions.Count() == 0 && mNotifications.IsEmpty() &&
      !mFocusEvent && mEvents.IsEmpty() && mTextHash.Count() == 0 &&
      mHangingChildDocuments.IsEmpty() &&
      mDocument->HasLoadState(DocAccessible::eCompletelyLoaded) &&
      mPresShell->RemoveRefreshObserver(this, FlushType::Display) &&
      mPresShell->RemovePostRefreshObserver(this)) {
    mObservingState = eNotObservingRefresh;
  }
}

void NotificationController::DidRefresh() {
  if (IPCAccessibilityActive() && mDocument->IsViewportCacheDirty()) {
    // It is now safe to send the viewport cache, because
    // we know painting has finished.
    RefPtr<AccAttributes> fields = mDocument->BundleFieldsForCache(
        CacheDomain::Viewport, CacheUpdateType::Update);

    if (fields->Count()) {
      nsTArray<CacheData> data(1);
      data.AppendElement(CacheData(0, fields));
      MOZ_ASSERT(mDocument->IPCDoc());
      mDocument->IPCDoc()->SendCache(CacheUpdateType::Update, data, true);
    }

    mDocument->SetViewportCacheDirty(false);
  }
}

void NotificationController::EventMap::PutEvent(AccTreeMutationEvent* aEvent) {
  EventType type = GetEventType(aEvent);
  uint64_t addr = reinterpret_cast<uintptr_t>(aEvent->GetAccessible());
  MOZ_ASSERT((addr & 0x3) == 0, "accessible is not 4 byte aligned");
  addr |= type;
  mTable.InsertOrUpdate(addr, RefPtr{aEvent});
}

AccTreeMutationEvent* NotificationController::EventMap::GetEvent(
    LocalAccessible* aTarget, EventType aType) {
  uint64_t addr = reinterpret_cast<uintptr_t>(aTarget);
  MOZ_ASSERT((addr & 0x3) == 0, "target is not 4 byte aligned");

  addr |= aType;
  return mTable.GetWeak(addr);
}

void NotificationController::EventMap::RemoveEvent(
    AccTreeMutationEvent* aEvent) {
  EventType type = GetEventType(aEvent);
  uint64_t addr = reinterpret_cast<uintptr_t>(aEvent->GetAccessible());
  MOZ_ASSERT((addr & 0x3) == 0, "accessible is not 4 byte aligned");
  addr |= type;

  MOZ_ASSERT(mTable.GetWeak(addr) == aEvent, "mTable has the wrong event");
  mTable.Remove(addr);
}

NotificationController::EventMap::EventType
NotificationController::EventMap::GetEventType(AccTreeMutationEvent* aEvent) {
  switch (aEvent->GetEventType()) {
    case nsIAccessibleEvent::EVENT_SHOW:
      return ShowEvent;
    case nsIAccessibleEvent::EVENT_HIDE:
      return HideEvent;
    case nsIAccessibleEvent::EVENT_REORDER:
    case nsIAccessibleEvent::EVENT_INNER_REORDER:
      return ReorderEvent;
    default:
      MOZ_ASSERT_UNREACHABLE("event has invalid type");
      return ShowEvent;
  }
}
