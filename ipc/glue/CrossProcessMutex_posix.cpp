/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CrossProcessMutex.h"
#include "mozilla/unused.h"
#include "nsDebug.h"
#include "nsISupportsImpl.h"

#ifdef OS_MACOSX
#include "nsCocoaFeatures.h"
#endif

namespace {

struct MutexData {
  pthread_mutex_t mMutex;
  mozilla::Atomic<int32_t> mCount;
};

}

namespace mozilla {

static void
InitMutex(pthread_mutex_t* mMutex)
{
  pthread_mutexattr_t mutexAttributes;
  pthread_mutexattr_init(&mutexAttributes);
  // Make the mutex reentrant so it behaves the same as a win32 mutex
  if (pthread_mutexattr_settype(&mutexAttributes, PTHREAD_MUTEX_RECURSIVE)) {
    MOZ_CRASH();
  }
  if (pthread_mutexattr_setpshared(&mutexAttributes, PTHREAD_PROCESS_SHARED)) {
    MOZ_CRASH();
  }

  if (pthread_mutex_init(mMutex, &mutexAttributes)) {
    MOZ_CRASH();
  }
}

CrossProcessMutex::CrossProcessMutex(const char*)
    : mMutex(nullptr)
    , mCount(nullptr)
{
#ifdef OS_MACOSX
  if (!nsCocoaFeatures::OnLionOrLater()) {
    // Don't allow using the cross-process mutex before OS X 10.7 because it
    // probably doesn't work very well. See discussion in bug 1072093 for more
    // details.
    MOZ_CRASH();
  }
#endif

  mSharedBuffer = new ipc::SharedMemoryBasic;
  if (!mSharedBuffer->Create(sizeof(MutexData))) {
    MOZ_CRASH();
  }

  if (!mSharedBuffer->Map(sizeof(MutexData))) {
    MOZ_CRASH();
  }

  MutexData* data = static_cast<MutexData*>(mSharedBuffer->memory());

  if (!data) {
    MOZ_CRASH();
  }

  mMutex = &(data->mMutex);
  mCount = &(data->mCount);

  *mCount = 1;
  InitMutex(mMutex);

  MOZ_COUNT_CTOR(CrossProcessMutex);
}

CrossProcessMutex::CrossProcessMutex(CrossProcessMutexHandle aHandle)
    : mMutex(nullptr)
    , mCount(nullptr)
{
  if (!ipc::SharedMemoryBasic::IsHandleValid(aHandle)) {
    MOZ_CRASH();
  }

  mSharedBuffer = new ipc::SharedMemoryBasic(aHandle);

  if (!mSharedBuffer->Map(sizeof(MutexData))) {
    MOZ_CRASH();
  }

  MutexData* data = static_cast<MutexData*>(mSharedBuffer->memory());

  if (!data) {
    MOZ_CRASH();
  }

  mMutex = &(data->mMutex);
  mCount = &(data->mCount);
  int32_t count = (*mCount)++;

  if (count == 0) {
    // The other side has already let go of their CrossProcessMutex, so now
    // mMutex is garbage. We need to re-initialize it.
    InitMutex(mMutex);
  }

  MOZ_COUNT_CTOR(CrossProcessMutex);
}

CrossProcessMutex::~CrossProcessMutex()
{
  int32_t count = --(*mCount);

  if (count == 0) {
    // Nothing can be done if the destroy fails so ignore return code.
    unused << pthread_mutex_destroy(mMutex);
  }

  MOZ_COUNT_DTOR(CrossProcessMutex);
}

void
CrossProcessMutex::Lock()
{
  MOZ_ASSERT(*mCount > 0, "Attempting to lock mutex with zero ref count");
  pthread_mutex_lock(mMutex);
}

void
CrossProcessMutex::Unlock()
{
  MOZ_ASSERT(*mCount > 0, "Attempting to unlock mutex with zero ref count");
  pthread_mutex_unlock(mMutex);
}

CrossProcessMutexHandle
CrossProcessMutex::ShareToProcess(base::ProcessHandle aHandle)
{
  CrossProcessMutexHandle result = ipc::SharedMemoryBasic::NULLHandle();

  if (mSharedBuffer && !mSharedBuffer->ShareToProcess(aHandle, &result)) {
    MOZ_CRASH();
  }

  return result;
}

}
