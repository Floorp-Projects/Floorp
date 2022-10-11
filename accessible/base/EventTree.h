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

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_EventQueue_h_
