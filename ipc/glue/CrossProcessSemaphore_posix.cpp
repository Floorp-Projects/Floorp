/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessSemaphore.h"
#include "mozilla/Unused.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"
#include <errno.h>

static const uint64_t kNsPerMs = 1000000;
static const uint64_t kNsPerSec = 1000000000;

namespace {


struct SemaphoreData {
  sem_t mSemaphore;
  mozilla::Atomic<int32_t> mRefCount;
  uint32_t mInitialValue;
};

} // namespace

namespace mozilla {

CrossProcessSemaphore::CrossProcessSemaphore(const char*, uint32_t aInitialValue)
    : mSemaphore(nullptr)
    , mRefCount(nullptr)
{
  mSharedBuffer = new ipc::SharedMemoryBasic;
  if (!mSharedBuffer->Create(sizeof(SemaphoreData))) {
    MOZ_CRASH();
  }

  if (!mSharedBuffer->Map(sizeof(SemaphoreData))) {
    MOZ_CRASH();
  }

  SemaphoreData* data = static_cast<SemaphoreData*>(mSharedBuffer->memory());

  if (!data) {
    MOZ_CRASH();
  }

  data->mInitialValue = aInitialValue;
  mSemaphore = &data->mSemaphore;
  mRefCount = &data->mRefCount;

  *mRefCount = 1;
  if (sem_init(mSemaphore, 1, data->mInitialValue)) {
    MOZ_CRASH();
  }

  MOZ_COUNT_CTOR(CrossProcessSemaphore);
}

CrossProcessSemaphore::CrossProcessSemaphore(CrossProcessSemaphoreHandle aHandle)
    : mSemaphore(nullptr)
    , mRefCount(nullptr)
{
  mSharedBuffer = new ipc::SharedMemoryBasic;

  if (!mSharedBuffer->IsHandleValid(aHandle)) {
    MOZ_CRASH();
  }

  if (!mSharedBuffer->SetHandle(aHandle, ipc::SharedMemory::RightsReadWrite)) {
    MOZ_CRASH();
  }

  if (!mSharedBuffer->Map(sizeof(SemaphoreData))) {
    MOZ_CRASH();
  }

  SemaphoreData* data = static_cast<SemaphoreData*>(mSharedBuffer->memory());

  if (!data) {
    MOZ_CRASH();
  }

  mSemaphore = &data->mSemaphore;
  mRefCount = &data->mRefCount;
  int32_t oldCount = (*mRefCount)++;

  if (oldCount == 0) {
    // The other side has already let go of their CrossProcessSemaphore, so now
    // mSemaphore is garbage. We need to re-initialize it.
    if (sem_init(mSemaphore, 1, data->mInitialValue)) {
      MOZ_CRASH();
    }
  }

  MOZ_COUNT_CTOR(CrossProcessSemaphore);
}

CrossProcessSemaphore::~CrossProcessSemaphore()
{
  int32_t oldCount = --(*mRefCount);

  if (oldCount == 0) {
    // Nothing can be done if the destroy fails so ignore return code.
    Unused << sem_destroy(mSemaphore);
  }

  MOZ_COUNT_DTOR(CrossProcessSemaphore);
}

bool
CrossProcessSemaphore::Wait(const Maybe<TimeDuration>& aWaitTime)
{
  MOZ_ASSERT(*mRefCount > 0, "Attempting to wait on a semaphore with zero ref count");
  int ret;
  if (aWaitTime.isSome()) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
      return false;
    }

    ts.tv_nsec += (kNsPerMs * aWaitTime->ToMilliseconds());
    ts.tv_sec += ts.tv_nsec / kNsPerSec;
    ts.tv_nsec %= kNsPerSec;

    while ((ret = sem_timedwait(mSemaphore, &ts)) == -1 && errno == EINTR) {
      continue;
    }
  } else {
    while ((ret = sem_wait(mSemaphore)) == -1 && errno == EINTR) {
      continue;
    }
  }
  return ret == 0;
}

void
CrossProcessSemaphore::Signal()
{
  MOZ_ASSERT(*mRefCount > 0, "Attempting to signal a semaphore with zero ref count");
  sem_post(mSemaphore);
}

CrossProcessSemaphoreHandle
CrossProcessSemaphore::ShareToProcess(base::ProcessId aTargetPid)
{
  CrossProcessSemaphoreHandle result = ipc::SharedMemoryBasic::NULLHandle();

  if (mSharedBuffer && !mSharedBuffer->ShareToProcess(aTargetPid, &result)) {
    MOZ_CRASH();
  }

  return result;
}

} // namespace mozilla
