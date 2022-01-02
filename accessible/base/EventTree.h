/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_EventTree_h_
#define mozilla_a11y_EventTree_h_

#include "AccEvent.h"
#include "LocalAccessible.h"

#include "mozilla/a11y/DocAccessible.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace a11y {

class NotificationController;

/**
 * This class makes sure required tasks are done before and after tree
 * mutations. Currently this only includes group info invalidation. You must
 * have an object of this class on the stack when calling methods that mutate
 * the accessible tree.
 */
class TreeMutation final {
 public:
  static const bool kNoEvents = true;
  static const bool kNoShutdown = true;

  explicit TreeMutation(LocalAccessible* aParent, bool aNoEvents = false);
  ~TreeMutation();

  void AfterInsertion(LocalAccessible* aChild);
  void BeforeRemoval(LocalAccessible* aChild, bool aNoShutdown = false);
  void Done();

 private:
  NotificationController* Controller() const {
    return mParent->Document()->Controller();
  }

  static EventTree* const kNoEventTree;

#ifdef A11Y_LOG
  static const char* PrefixLog(void* aData, LocalAccessible*);
#endif

  LocalAccessible* mParent;
  uint32_t mStartIdx;
  uint32_t mStateFlagsCopy;

  /*
   * True if mutation events should be queued.
   */
  bool mQueueEvents;

#ifdef DEBUG
  bool mIsDone;
#endif
};

/**
 * A mutation events coalescence structure.
 */
class EventTree final {
 public:
  EventTree()
      : mFirst(nullptr),
        mNext(nullptr),
        mContainer(nullptr),
        mFireReorder(false) {}
  explicit EventTree(LocalAccessible* aContainer, bool aFireReorder)
      : mFirst(nullptr),
        mNext(nullptr),
        mContainer(aContainer),
        mFireReorder(aFireReorder) {}
  ~EventTree() { Clear(); }

  void Shown(LocalAccessible* aTarget);
  void Hidden(LocalAccessible*, bool);

  /**
   * Return an event tree node for the given accessible.
   */
  const EventTree* Find(const LocalAccessible* aContainer) const;

  /**
   * Add a mutation event to this event tree.
   */
  void Mutated(AccMutationEvent* aEv);

#ifdef A11Y_LOG
  void Log(uint32_t aLevel = UINT32_MAX) const;
#endif

 private:
  /**
   * Processes the event queue and fires events.
   */
  void Process(const RefPtr<DocAccessible>& aDeathGrip);

  /**
   * Return an event subtree for the given accessible.
   */
  EventTree* FindOrInsert(LocalAccessible* aContainer);

  void Clear();

  UniquePtr<EventTree> mFirst;
  UniquePtr<EventTree> mNext;

  LocalAccessible* mContainer;
  nsTArray<RefPtr<AccMutationEvent>> mDependentEvents;
  bool mFireReorder;

  static NotificationController* Controller(LocalAccessible* aAcc) {
    return aAcc->Document()->Controller();
  }

  friend class NotificationController;
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_EventQueue_h_
