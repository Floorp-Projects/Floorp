/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EventTree.h"

#include "LocalAccessible-inl.h"
#include "EmbeddedObjCollector.h"
#include "NotificationController.h"
#include "nsEventShell.h"
#include "DocAccessible.h"
#include "DocAccessible-inl.h"
#ifdef A11Y_LOG
#  include "Logging.h"
#endif

#include "mozilla/UniquePtr.h"

using namespace mozilla;
using namespace mozilla::a11y;

////////////////////////////////////////////////////////////////////////////////
// TreeMutation class

EventTree* const TreeMutation::kNoEventTree = reinterpret_cast<EventTree*>(-1);

TreeMutation::TreeMutation(LocalAccessible* aParent, bool aNoEvents)
    : mParent(aParent),
      mStartIdx(UINT32_MAX),
      mStateFlagsCopy(mParent->mStateFlags),
      mQueueEvents(!aNoEvents) {
#ifdef DEBUG
  mIsDone = false;
#endif

#ifdef A11Y_LOG
  if (mQueueEvents && logging::IsEnabled(logging::eEventTree)) {
    logging::MsgBegin("EVENTS_TREE", "reordering tree before");
    logging::AccessibleInfo("reordering for", mParent);
    Controller()->RootEventTree().Log();
    logging::MsgEnd();

    if (logging::IsEnabled(logging::eVerbose)) {
      logging::Tree("EVENTS_TREE", "Container tree", mParent->Document(),
                    PrefixLog, static_cast<void*>(this));
    }
  }
#endif

  mParent->mStateFlags |= LocalAccessible::eKidsMutating;
}

TreeMutation::~TreeMutation() {
  MOZ_ASSERT(mIsDone, "Done() must be called explicitly");
}

void TreeMutation::AfterInsertion(LocalAccessible* aChild) {
  MOZ_ASSERT(aChild->LocalParent() == mParent);

  if (static_cast<uint32_t>(aChild->mIndexInParent) < mStartIdx) {
    mStartIdx = aChild->mIndexInParent + 1;
  }

  if (!mQueueEvents) {
    return;
  }

  RefPtr<AccShowEvent> ev = new AccShowEvent(aChild);
  DebugOnly<bool> added = Controller()->QueueMutationEvent(ev);
  MOZ_ASSERT(added);
  aChild->SetShowEventTarget(true);
}

void TreeMutation::BeforeRemoval(LocalAccessible* aChild, bool aNoShutdown) {
  MOZ_ASSERT(aChild->LocalParent() == mParent);

  if (static_cast<uint32_t>(aChild->mIndexInParent) < mStartIdx) {
    mStartIdx = aChild->mIndexInParent;
  }

  if (!mQueueEvents) {
    return;
  }

  RefPtr<AccHideEvent> ev = new AccHideEvent(aChild, !aNoShutdown);
  if (Controller()->QueueMutationEvent(ev)) {
    aChild->SetHideEventTarget(true);
  }
}

void TreeMutation::Done() {
  MOZ_ASSERT(mParent->mStateFlags & LocalAccessible::eKidsMutating);
  mParent->mStateFlags &= ~LocalAccessible::eKidsMutating;

  uint32_t length = mParent->mChildren.Length();
#ifdef DEBUG
  for (uint32_t idx = 0; idx < mStartIdx && idx < length; idx++) {
    MOZ_ASSERT(
        mParent->mChildren[idx]->mIndexInParent == static_cast<int32_t>(idx),
        "Wrong index detected");
  }
#endif

  for (uint32_t idx = mStartIdx; idx < length; idx++) {
    mParent->mChildren[idx]->mIndexOfEmbeddedChild = -1;
  }

  for (uint32_t idx = 0; idx < length; idx++) {
    mParent->mChildren[idx]->mStateFlags |= LocalAccessible::eGroupInfoDirty;
  }

  mParent->mEmbeddedObjCollector = nullptr;
  mParent->mStateFlags |= mStateFlagsCopy & LocalAccessible::eKidsMutating;

#ifdef DEBUG
  mIsDone = true;
#endif

#ifdef A11Y_LOG
  if (mQueueEvents && logging::IsEnabled(logging::eEventTree)) {
    logging::MsgBegin("EVENTS_TREE", "reordering tree after");
    logging::AccessibleInfo("reordering for", mParent);
    Controller()->RootEventTree().Log();
    logging::MsgEnd();
  }
#endif
}

#ifdef A11Y_LOG
const char* TreeMutation::PrefixLog(void* aData, LocalAccessible* aAcc) {
  TreeMutation* thisObj = reinterpret_cast<TreeMutation*>(aData);
  if (thisObj->mParent == aAcc) {
    return "_X_";
  }
  const EventTree& ret = thisObj->Controller()->RootEventTree();
  if (ret.Find(aAcc)) {
    return "_с_";
  }
  return "";
}
#endif

////////////////////////////////////////////////////////////////////////////////
// EventTree

void EventTree::Shown(LocalAccessible* aChild) {
  RefPtr<AccShowEvent> ev = new AccShowEvent(aChild);
  Controller(aChild)->WithdrawPrecedingEvents(&ev->mPrecedingEvents);
  Mutated(ev);
}

void EventTree::Hidden(LocalAccessible* aChild, bool aNeedsShutdown) {
  RefPtr<AccHideEvent> ev = new AccHideEvent(aChild, aNeedsShutdown);
  if (!aNeedsShutdown) {
    Controller(aChild)->StorePrecedingEvent(ev);
  }
  Mutated(ev);
}

void EventTree::Process(const RefPtr<DocAccessible>& aDeathGrip) {
  while (mFirst) {
    // Skip a node and its subtree if its container is not in the document.
    if (mFirst->mContainer->IsInDocument()) {
      mFirst->Process(aDeathGrip);
      if (aDeathGrip->IsDefunct()) {
        return;
      }
    }
    mFirst = std::move(mFirst->mNext);
  }

  MOZ_ASSERT(mContainer || mDependentEvents.IsEmpty(),
             "No container, no events");
  MOZ_ASSERT(!mContainer || !mContainer->IsDefunct(),
             "Processing events for defunct container");
  MOZ_ASSERT(!mFireReorder || mContainer, "No target for reorder event");

  // Fire mutation events.
  uint32_t eventsCount = mDependentEvents.Length();
  for (uint32_t jdx = 0; jdx < eventsCount; jdx++) {
    AccMutationEvent* mtEvent = mDependentEvents[jdx];
    MOZ_ASSERT(mtEvent->Document(), "No document for event target");

    // Fire all hide events that has to be fired before this show event.
    if (mtEvent->IsShow()) {
      AccShowEvent* showEv = downcast_accEvent(mtEvent);
      for (uint32_t i = 0; i < showEv->mPrecedingEvents.Length(); i++) {
        nsEventShell::FireEvent(showEv->mPrecedingEvents[i]);
        if (aDeathGrip->IsDefunct()) {
          return;
        }
      }
    }

    nsEventShell::FireEvent(mtEvent);
    if (aDeathGrip->IsDefunct()) {
      return;
    }

    if (mtEvent->mTextChangeEvent) {
      nsEventShell::FireEvent(mtEvent->mTextChangeEvent);
      if (aDeathGrip->IsDefunct()) {
        return;
      }
    }

    if (mtEvent->IsHide()) {
      // Fire menupopup end event before a hide event if a menu goes away.

      // XXX: We don't look into children of hidden subtree to find hiding
      // menupopup (as we did prior bug 570275) because we don't do that when
      // menu is showing (and that's impossible until bug 606924 is fixed).
      // Nevertheless we should do this at least because layout coalesces
      // the changes before our processing and we may miss some menupopup
      // events. Now we just want to be consistent in content insertion/removal
      // handling.
      if (mtEvent->mAccessible->ARIARole() == roles::MENUPOPUP) {
        nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_MENUPOPUP_END,
                                mtEvent->mAccessible);
        if (aDeathGrip->IsDefunct()) {
          return;
        }
      }

      AccHideEvent* hideEvent = downcast_accEvent(mtEvent);
      if (hideEvent->NeedsShutdown()) {
        aDeathGrip->ShutdownChildrenInSubtree(mtEvent->mAccessible);
      }
    }
  }

  // Fire reorder event at last.
  if (mFireReorder) {
    nsEventShell::FireEvent(nsIAccessibleEvent::EVENT_REORDER, mContainer);
    mContainer->Document()->MaybeNotifyOfValueChange(mContainer);
  }

  mDependentEvents.Clear();
}

EventTree* EventTree::FindOrInsert(LocalAccessible* aContainer) {
  if (!mFirst) {
    mFirst.reset(new EventTree(aContainer, mDependentEvents.IsEmpty()));
    return mFirst.get();
  }

  EventTree* prevNode = nullptr;
  EventTree* node = mFirst.get();
  do {
    MOZ_ASSERT(!node->mContainer->IsApplication(),
               "No event for application accessible is expected here");
    MOZ_ASSERT(!node->mContainer->IsDefunct(),
               "An event target has to be alive");

    // Case of same target.
    if (node->mContainer == aContainer) {
      return node;
    }

    // Check if the given container is contained by a current node
    LocalAccessible* top = mContainer ? mContainer : aContainer->Document();
    LocalAccessible* parent = aContainer;
    while (parent) {
      // Reached a top, no match for a current event.
      if (parent == top) {
        break;
      }

      // We got a match.
      if (parent->LocalParent() == node->mContainer) {
        // Reject the node if it's contained by a show/hide event target
        uint32_t evCount = node->mDependentEvents.Length();
        for (uint32_t idx = 0; idx < evCount; idx++) {
          AccMutationEvent* ev = node->mDependentEvents[idx];
          if (ev->GetAccessible() == parent) {
#ifdef A11Y_LOG
            if (logging::IsEnabled(logging::eEventTree)) {
              logging::MsgBegin("EVENTS_TREE",
                                "Rejecting node contained by show/hide");
              logging::AccessibleInfo("Node", aContainer);
              logging::MsgEnd();
            }
#endif
            // If the node is rejected, then check if it has related hide event
            // on stack, and if so, then connect it to the parent show event.
            if (ev->IsShow()) {
              AccShowEvent* showEv = downcast_accEvent(ev);
              Controller(aContainer)
                  ->WithdrawPrecedingEvents(&showEv->mPrecedingEvents);
            }
            return nullptr;
          }
        }

        return node->FindOrInsert(aContainer);
      }

      parent = parent->LocalParent();
      MOZ_ASSERT(parent, "Wrong tree");
    }

    // If the given container contains a current node
    // then
    //   if show or hide of the given node contains a grand parent of the
    //   current node then ignore the current node and its show and hide events
    //   otherwise ignore the current node, but not its show and hide events
    LocalAccessible* curParent = node->mContainer;
    while (curParent && !curParent->IsDoc()) {
      if (curParent->LocalParent() != aContainer) {
        curParent = curParent->LocalParent();
        continue;
      }

      // Insert the tail node into the hierarchy between the current node and
      // its parent.
      node->mFireReorder = false;
      UniquePtr<EventTree>& nodeOwnerRef = prevNode ? prevNode->mNext : mFirst;
      UniquePtr<EventTree> newNode(
          new EventTree(aContainer, mDependentEvents.IsEmpty()));
      newNode->mFirst = std::move(nodeOwnerRef);
      nodeOwnerRef = std::move(newNode);
      nodeOwnerRef->mNext = std::move(node->mNext);

      // Check if a next node is contained by the given node too, and move them
      // under the given node if so.
      prevNode = nodeOwnerRef.get();
      node = nodeOwnerRef->mNext.get();
      UniquePtr<EventTree>* nodeRef = &nodeOwnerRef->mNext;
      EventTree* insNode = nodeOwnerRef->mFirst.get();
      while (node) {
        LocalAccessible* curParent = node->mContainer;
        while (curParent && !curParent->IsDoc()) {
          if (curParent->LocalParent() != aContainer) {
            curParent = curParent->LocalParent();
            continue;
          }

          MOZ_ASSERT(!insNode->mNext);

          node->mFireReorder = false;
          insNode->mNext = std::move(*nodeRef);
          insNode = insNode->mNext.get();

          prevNode->mNext = std::move(node->mNext);
          node = prevNode;
          break;
        }

        prevNode = node;
        nodeRef = &node->mNext;
        node = node->mNext.get();
      }

      return nodeOwnerRef.get();
    }

    prevNode = node;
  } while ((node = node->mNext.get()));

  MOZ_ASSERT(prevNode, "Nowhere to insert");
  MOZ_ASSERT(!prevNode->mNext, "Taken by another node");

  // If 'this' node contains the given container accessible, then
  //   do not emit a reorder event for the container
  //   if a dependent show event target contains the given container then do not
  //   emit show / hide events (see Process() method)

  prevNode->mNext.reset(new EventTree(aContainer, mDependentEvents.IsEmpty()));
  return prevNode->mNext.get();
}

void EventTree::Clear() {
  mFirst = nullptr;
  mNext = nullptr;
  mContainer = nullptr;

  uint32_t eventsCount = mDependentEvents.Length();
  for (uint32_t jdx = 0; jdx < eventsCount; jdx++) {
    mDependentEvents[jdx]->mEventType = AccEvent::eDoNotEmit;
    AccHideEvent* ev = downcast_accEvent(mDependentEvents[jdx]);
    if (ev && ev->NeedsShutdown()) {
      ev->Document()->ShutdownChildrenInSubtree(ev->mAccessible);
    }
  }
  mDependentEvents.Clear();
}

const EventTree* EventTree::Find(const LocalAccessible* aContainer) const {
  const EventTree* et = this;
  while (et) {
    if (et->mContainer == aContainer) {
      return et;
    }

    if (et->mFirst) {
      et = et->mFirst.get();
      const EventTree* cet = et->Find(aContainer);
      if (cet) {
        return cet;
      }
    }

    et = et->mNext.get();
    const EventTree* cet = et->Find(aContainer);
    if (cet) {
      return cet;
    }
  }

  return nullptr;
}

#ifdef A11Y_LOG
void EventTree::Log(uint32_t aLevel) const {
  if (aLevel == UINT32_MAX) {
    if (mFirst) {
      mFirst->Log(0);
    }
    return;
  }

  for (uint32_t i = 0; i < aLevel; i++) {
    printf("  ");
  }
  logging::AccessibleInfo("container", mContainer);

  for (uint32_t i = 0; i < mDependentEvents.Length(); i++) {
    AccMutationEvent* ev = mDependentEvents[i];
    if (ev->IsShow()) {
      for (uint32_t i = 0; i < aLevel + 1; i++) {
        printf("  ");
      }
      logging::AccessibleInfo("shown", ev->mAccessible);

      AccShowEvent* showEv = downcast_accEvent(ev);
      for (uint32_t i = 0; i < showEv->mPrecedingEvents.Length(); i++) {
        for (uint32_t j = 0; j < aLevel + 1; j++) {
          printf("  ");
        }
        logging::AccessibleInfo("preceding",
                                showEv->mPrecedingEvents[i]->mAccessible);
      }
    } else {
      for (uint32_t i = 0; i < aLevel + 1; i++) {
        printf("  ");
      }
      logging::AccessibleInfo("hidden", ev->mAccessible);
    }
  }

  if (mFirst) {
    mFirst->Log(aLevel + 1);
  }

  if (mNext) {
    mNext->Log(aLevel);
  }
}
#endif

void EventTree::Mutated(AccMutationEvent* aEv) {
  // If shown or hidden node is a root of previously mutated subtree, then
  // discard those subtree mutations as we are no longer interested in them.
  UniquePtr<EventTree>* node = &mFirst;
  while (*node) {
    LocalAccessible* cntr = (*node)->mContainer;
    while (cntr != mContainer) {
      if (cntr == aEv->mAccessible) {
#ifdef A11Y_LOG
        if (logging::IsEnabled(logging::eEventTree)) {
          logging::MsgBegin("EVENTS_TREE", "Trim subtree");
          logging::AccessibleInfo("Show/hide container", aEv->mAccessible);
          logging::AccessibleInfo("Trimmed subtree root", (*node)->mContainer);
          logging::MsgEnd();
        }
#endif

        // If the new hide is part of a move and it contains existing child
        // shows, then move preceding events from the child shows to the buffer,
        // so the ongoing show event will pick them up.
        if (aEv->IsHide()) {
          AccHideEvent* hideEv = downcast_accEvent(aEv);
          if (!hideEv->mNeedsShutdown) {
            for (uint32_t i = 0; i < (*node)->mDependentEvents.Length(); i++) {
              AccMutationEvent* childEv = (*node)->mDependentEvents[i];
              if (childEv->IsShow()) {
                AccShowEvent* childShowEv = downcast_accEvent(childEv);
                if (childShowEv->mPrecedingEvents.Length() > 0) {
                  Controller(mContainer)
                      ->StorePrecedingEvents(
                          std::move(childShowEv->mPrecedingEvents));
                }
              }
            }
          }
        }
        // If the new show contains existing child shows, then move preceding
        // events from the child shows to the new show.
        else if (aEv->IsShow()) {
          AccShowEvent* showEv = downcast_accEvent(aEv);
          for (uint32_t i = 0; (*node)->mDependentEvents.Length(); i++) {
            AccMutationEvent* childEv = (*node)->mDependentEvents[i];
            if (childEv->IsShow()) {
              AccShowEvent* showChildEv = downcast_accEvent(childEv);
              if (showChildEv->mPrecedingEvents.Length() > 0) {
#ifdef A11Y_LOG
                if (logging::IsEnabled(logging::eEventTree)) {
                  logging::MsgBegin("EVENTS_TREE", "Adopt preceding events");
                  logging::AccessibleInfo("Parent", aEv->mAccessible);
                  for (uint32_t j = 0;
                       j < showChildEv->mPrecedingEvents.Length(); j++) {
                    logging::AccessibleInfo(
                        "Adoptee",
                        showChildEv->mPrecedingEvents[i]->mAccessible);
                  }
                  logging::MsgEnd();
                }
#endif
                showEv->mPrecedingEvents.AppendElements(
                    showChildEv->mPrecedingEvents);
              }
            }
          }
        }

        *node = std::move((*node)->mNext);
        break;
      }
      cntr = cntr->LocalParent();
    }
    if (cntr == aEv->mAccessible) {
      continue;
    }
    node = &(*node)->mNext;
  }

  AccMutationEvent* prevEvent = mDependentEvents.SafeLastElement(nullptr);
  mDependentEvents.AppendElement(aEv);

  // Coalesce text change events from this hide/show event and the previous one.
  if (prevEvent && aEv->mEventType == prevEvent->mEventType) {
    if (aEv->IsHide()) {
      // XXX: we need a way to ignore SplitNode and JoinNode() when they do not
      // affect the text within the hypertext.
      AccTextChangeEvent* prevTextEvent = prevEvent->mTextChangeEvent;
      if (prevTextEvent) {
        AccHideEvent* hideEvent = downcast_accEvent(aEv);
        AccHideEvent* prevHideEvent = downcast_accEvent(prevEvent);

        if (prevHideEvent->mNextSibling == hideEvent->mAccessible) {
          hideEvent->mAccessible->AppendTextTo(prevTextEvent->mModifiedText);
        } else if (prevHideEvent->mPrevSibling == hideEvent->mAccessible) {
          uint32_t oldLen = prevTextEvent->GetLength();
          hideEvent->mAccessible->AppendTextTo(prevTextEvent->mModifiedText);
          prevTextEvent->mStart -= prevTextEvent->GetLength() - oldLen;
        }

        hideEvent->mTextChangeEvent.swap(prevEvent->mTextChangeEvent);
      }
    } else {
      AccTextChangeEvent* prevTextEvent = prevEvent->mTextChangeEvent;
      if (prevTextEvent) {
        if (aEv->mAccessible->IndexInParent() ==
            prevEvent->mAccessible->IndexInParent() + 1) {
          // If tail target was inserted after this target, i.e. tail target is
          // next sibling of this target.
          aEv->mAccessible->AppendTextTo(prevTextEvent->mModifiedText);
        } else if (aEv->mAccessible->IndexInParent() ==
                   prevEvent->mAccessible->IndexInParent() - 1) {
          // If tail target was inserted before this target, i.e. tail target is
          // previous sibling of this target.
          nsAutoString startText;
          aEv->mAccessible->AppendTextTo(startText);
          prevTextEvent->mModifiedText =
              startText + prevTextEvent->mModifiedText;
          prevTextEvent->mStart -= startText.Length();
        }

        aEv->mTextChangeEvent.swap(prevEvent->mTextChangeEvent);
      }
    }
  }

  // Create a text change event caused by this hide/show event. When a node is
  // hidden/removed or shown/appended, the text in an ancestor hyper text will
  // lose or get new characters.
  if (aEv->mTextChangeEvent || !mContainer->IsHyperText()) {
    return;
  }

  nsAutoString text;
  aEv->mAccessible->AppendTextTo(text);
  if (text.IsEmpty()) {
    return;
  }

  int32_t offset = mContainer->AsHyperText()->GetChildOffset(aEv->mAccessible);
  aEv->mTextChangeEvent = new AccTextChangeEvent(
      mContainer, offset, text, aEv->IsShow(),
      aEv->mIsFromUserInput ? eFromUserInput : eNoUserInput);
}
