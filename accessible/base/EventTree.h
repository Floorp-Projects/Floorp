/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_EventTree_h_
#define mozilla_a11y_EventTree_h_

#include "AccEvent.h"
#include "Accessible.h"

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace a11y {

/**
 * This class makes sure required tasks are done before and after tree
 * mutations. Currently this only includes group info invalidation. You must
 * have an object of this class on the stack when calling methods that mutate
 * the accessible tree.
 */
class TreeMutation final
{
public:
  static const bool kNoEvents = true;
  static const bool kNoShutdown = true;

  explicit TreeMutation(Accessible* aParent, bool aNoEvents = false);
  ~TreeMutation();

  void AfterInsertion(Accessible* aChild);
  void BeforeRemoval(Accessible* aChild, bool aNoShutdown = false);
  void Done();

private:
  NotificationController* Controller() const
    { return mParent->Document()->Controller(); }

  static EventTree* const kNoEventTree;

#ifdef A11Y_LOG
  static const char* PrefixLog(void* aData, Accessible*);
#endif

  Accessible* mParent;
  uint32_t mStartIdx;
  uint32_t mStateFlagsCopy;
  EventTree* mEventTree;

#ifdef DEBUG
  bool mIsDone;
#endif
};


/**
 * A mutation events coalescence structure.
 */
class EventTree final {
public:
  EventTree() :
    mFirst(nullptr), mNext(nullptr), mContainer(nullptr), mFireReorder(false) { }
  explicit EventTree(Accessible* aContainer, bool aFireReorder) :
    mFirst(nullptr), mNext(nullptr), mContainer(aContainer),
    mFireReorder(aFireReorder) { }
  ~EventTree() { Clear(); }

  void Shown(Accessible* aChild);

  void Hidden(Accessible* aChild, bool aNeedsShutdown = true);

  /**
   * Return an event tree node for the given accessible.
   */
  const EventTree* Find(const Accessible* aContainer) const;

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
  EventTree* FindOrInsert(Accessible* aContainer);

  void Mutated(AccMutationEvent* aEv);
  void Clear();

  UniquePtr<EventTree> mFirst;
  UniquePtr<EventTree> mNext;

  Accessible* mContainer;
  nsTArray<RefPtr<AccMutationEvent>> mDependentEvents;
  bool mFireReorder;

  static NotificationController* Controller(Accessible* aAcc)
    { return aAcc->Document()->Controller(); }

  friend class NotificationController;
};


} // namespace a11y
} // namespace mozilla

#endif // mozilla_a11y_EventQueue_h_
