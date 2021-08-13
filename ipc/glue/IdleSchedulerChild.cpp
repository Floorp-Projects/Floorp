/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IdleSchedulerChild.h"
#include "mozilla/ipc/IdleSchedulerParent.h"
#include "mozilla/ipc/PBackgroundChild.h"
#include "mozilla/Atomics.h"
#include "mozilla/IdlePeriodState.h"
#include "mozilla/Telemetry.h"
#include "BackgroundChild.h"

namespace mozilla::ipc {

static IdleSchedulerChild* sMainThreadIdleScheduler = nullptr;
static bool sIdleSchedulerDestroyed = false;

IdleSchedulerChild::~IdleSchedulerChild() {
  if (sMainThreadIdleScheduler == this) {
    sMainThreadIdleScheduler = nullptr;
    sIdleSchedulerDestroyed = true;
  }
  MOZ_ASSERT(!mIdlePeriodState);
}

void IdleSchedulerChild::Init(IdlePeriodState* aIdlePeriodState) {
  mIdlePeriodState = aIdlePeriodState;

  RefPtr<IdleSchedulerChild> scheduler = this;
  auto resolve =
      [&](Tuple<mozilla::Maybe<SharedMemoryHandle>, uint32_t>&& aResult) {
        if (Get<0>(aResult)) {
          mActiveCounter.SetHandle(*Get<0>(aResult), false);
          mActiveCounter.Map(sizeof(int32_t));
          mChildId = Get<1>(aResult);
          if (mChildId && mIdlePeriodState && mIdlePeriodState->IsActive()) {
            SetActive();
          }
        }
      };

  auto reject = [&](ResponseRejectReason) {};
  SendInitForIdleUse(std::move(resolve), std::move(reject));
}

IPCResult IdleSchedulerChild::RecvIdleTime(uint64_t aId, TimeDuration aBudget) {
  if (mIdlePeriodState) {
    mIdlePeriodState->SetIdleToken(aId, aBudget);
  }
  return IPC_OK();
}

void IdleSchedulerChild::SetActive() {
  if (mChildId && CanSend() && mActiveCounter.memory()) {
    ++(static_cast<Atomic<int32_t>*>(
        mActiveCounter.memory())[NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER]);
    ++(static_cast<Atomic<int32_t>*>(mActiveCounter.memory())[mChildId]);
  }
}

bool IdleSchedulerChild::SetPaused() {
  if (mChildId && CanSend() && mActiveCounter.memory()) {
    --(static_cast<Atomic<int32_t>*>(mActiveCounter.memory())[mChildId]);
    // The following expression reduces the global activity count and checks if
    // it drops below the cpu counter limit.
    return (static_cast<Atomic<int32_t>*>(
               mActiveCounter
                   .memory())[NS_IDLE_SCHEDULER_INDEX_OF_ACTIVITY_COUNTER])-- ==
           static_cast<Atomic<int32_t>*>(
               mActiveCounter.memory())[NS_IDLE_SCHEDULER_INDEX_OF_CPU_COUNTER];
  }

  return false;
}

RefPtr<IdleSchedulerChild::MayGCPromise> IdleSchedulerChild::MayGCNow() {
  if (mIsRequestingGC || mIsDoingGC) {
    return MayGCPromise::CreateAndResolve(false, __func__);
  }
  TimeStamp wait_since = TimeStamp::Now();

  mIsRequestingGC = true;
  return SendRequestGC()->Then(
      GetMainThreadSerialEventTarget(), __func__,
      [self = RefPtr(this), wait_since](bool aIgnored) {
        MOZ_ASSERT(self->mIsRequestingGC && !self->mIsDoingGC);
        // The parent process always says yes, sometimes after a delay.

        Telemetry::AccumulateTimeDelta(Telemetry::GC_WAIT_FOR_IDLE_MS,
                                       wait_since);
        self->mIsRequestingGC = false;
        self->mIsDoingGC = true;
        return MayGCPromise::CreateAndResolve(true, __func__);
      },
      [self = RefPtr(this)](ResponseRejectReason reason) {
        self->mIsRequestingGC = false;
        return MayGCPromise::CreateAndReject(reason, __func__);
      });
}

void IdleSchedulerChild::DoneGC() {
  if (mIsDoingGC) {
    SendDoneGC();
    mIsDoingGC = false;
  }
}

IdleSchedulerChild* IdleSchedulerChild::GetMainThreadIdleScheduler() {
  MOZ_ASSERT(NS_IsMainThread());

  if (sMainThreadIdleScheduler) {
    return sMainThreadIdleScheduler;
  }

  if (sIdleSchedulerDestroyed) {
    return nullptr;
  }

  ipc::PBackgroundChild* background =
      ipc::BackgroundChild::GetOrCreateForCurrentThread();
  if (background) {
    sMainThreadIdleScheduler = new ipc::IdleSchedulerChild();
    background->SendPIdleSchedulerConstructor(sMainThreadIdleScheduler);
  }
  return sMainThreadIdleScheduler;
}

}  // namespace mozilla::ipc
