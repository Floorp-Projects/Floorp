/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPTimerChild.h"
#include "GMPPlatform.h"
#include "GMPChild.h"

#define MAX_NUM_TIMERS 1000

namespace mozilla {
namespace gmp {

GMPTimerChild::GMPTimerChild(GMPChild* aPlugin)
  : mTimerCount(1)
  , mPlugin(aPlugin)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());
}

GMPTimerChild::~GMPTimerChild()
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());
}

GMPErr
GMPTimerChild::SetTimer(GMPTask* aTask, int64_t aTimeoutMS)
{
  if (!aTask) {
    NS_WARNING("Tried to set timer with null task!");
    return GMPGenericErr;
  }

  if (mPlugin->GMPMessageLoop() != MessageLoop::current()) {
    NS_WARNING("Tried to set GMP timer on non-main thread.");
    return GMPGenericErr;
  }

  if (mTimers.Count() > MAX_NUM_TIMERS) {
    return GMPQuotaExceededErr;
  }
  uint32_t timerId = mTimerCount;
  mTimers.Put(timerId, aTask);
  mTimerCount++;

  if (!SendSetTimer(timerId, aTimeoutMS)) {
    return GMPGenericErr;
  }
  return GMPNoErr;
}

mozilla::ipc::IPCResult
GMPTimerChild::RecvTimerExpired(const uint32_t& aTimerId)
{
  MOZ_ASSERT(mPlugin->GMPMessageLoop() == MessageLoop::current());

  GMPTask* task = mTimers.Get(aTimerId);
  mTimers.Remove(aTimerId);
  if (task) {
    RunOnMainThread(task);
  }
  return IPC_OK();
}

} // namespace gmp
} // namespace mozilla
