/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_IdleSchedulerChild_h__
#define mozilla_ipc_IdleSchedulerChild_h__

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/PIdleSchedulerChild.h"

class nsIIdlePeriod;

namespace mozilla {
class PrioritizedEventQueue;

namespace ipc {

class BackgroundChildImpl;

class IdleSchedulerChild final : public PIdleSchedulerChild {
 public:
  IdleSchedulerChild() = default;

  NS_INLINE_DECL_REFCOUNTING(IdleSchedulerChild)

  IPCResult RecvIdleTime(uint64_t aId, TimeDuration aBudget);

  void Init(PrioritizedEventQueue* aEventQueue);

  void Disconnect() { mEventQueue = nullptr; }

  // See similar methods on PrioritizedEventQueue.
  void SetActive();
  // Returns true if activity state dropped below cpu count.
  bool SetPaused();

  static IdleSchedulerChild* GetMainThreadIdleScheduler();

 private:
  ~IdleSchedulerChild();

  friend class BackgroundChildImpl;

  // See IdleScheduleParent::sActiveChildCounter
  base::SharedMemory mActiveCounter;

  PrioritizedEventQueue* mEventQueue = nullptr;

  uint32_t mChildId = 0;
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_ipc_IdleSchedulerChild_h__
