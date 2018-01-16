/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/MainThreadInvoker.h"

#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/HangMonitor.h"
#include "mozilla/mscom/SpinEvent.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SystemGroup.h"
#include "private/prpriv.h" // For PR_GetThreadID
#include "WinUtils.h"

namespace {

/**
 * SyncRunnable implements different code paths depending on whether or not
 * we are running on a multiprocessor system. In the multiprocessor case, we
 * leave the thread in a spin loop while waiting for the main thread to execute
 * our runnable. Since spinning is pointless in the uniprocessor case, we block
 * on an event that is set by the main thread once it has finished the runnable.
 */
class SyncRunnable : public mozilla::Runnable
{
public:
  explicit SyncRunnable(already_AddRefed<nsIRunnable> aRunnable)
    : mozilla::Runnable("MainThreadInvoker")
    , mRunnable(aRunnable)
  {}

  ~SyncRunnable() = default;

  NS_IMETHOD Run() override
  {
    if (mHasRun) {
      return NS_OK;
    }
    mHasRun = true;

    TimeStamp runStart(TimeStamp::Now());
    mRunnable->Run();
    TimeStamp runEnd(TimeStamp::Now());

    mDuration = runEnd - runStart;

    mEvent.Signal();
    return NS_OK;
  }

  bool WaitUntilComplete()
  {
    return mEvent.Wait(mozilla::mscom::MainThreadInvoker::GetTargetThread());
  }

  const mozilla::TimeDuration& GetDuration() const
  {
    return mDuration;
  }

private:
  bool                      mHasRun = false;
  nsCOMPtr<nsIRunnable>     mRunnable;
  mozilla::mscom::SpinEvent mEvent;
  mozilla::TimeDuration     mDuration;
};

} // anonymous namespace

namespace mozilla {
namespace mscom {

HANDLE MainThreadInvoker::sMainThread = nullptr;

/* static */ bool
MainThreadInvoker::InitStatics()
{
  nsCOMPtr<nsIThread> mainThread;
  nsresult rv = ::NS_GetMainThread(getter_AddRefs(mainThread));
  if (NS_FAILED(rv)) {
    return false;
  }

  PRThread* mainPrThread = nullptr;
  rv = mainThread->GetPRThread(&mainPrThread);
  if (NS_FAILED(rv)) {
    return false;
  }

  PRUint32 tid = ::PR_GetThreadID(mainPrThread);
  sMainThread = ::OpenThread(SYNCHRONIZE | THREAD_SET_CONTEXT, FALSE, tid);

  return !!sMainThread;
}

MainThreadInvoker::MainThreadInvoker()
{
  static const bool gotStatics = InitStatics();
  MOZ_ASSERT(gotStatics);
}

bool
MainThreadInvoker::Invoke(already_AddRefed<nsIRunnable>&& aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable(Move(aRunnable));
  if (!runnable) {
    return false;
  }

  if (NS_IsMainThread()) {
    runnable->Run();
    return true;
  }

  RefPtr<SyncRunnable> syncRunnable = new SyncRunnable(runnable.forget());

  if (NS_FAILED(SystemGroup::Dispatch(
                  TaskCategory::Other, do_AddRef(syncRunnable)))) {
    return false;
  }

  // This ref gets released in MainThreadAPC when it runs.
  SyncRunnable* syncRunnableRef = syncRunnable.get();
  NS_ADDREF(syncRunnableRef);
  if (!::QueueUserAPC(&MainThreadAPC, sMainThread,
                      reinterpret_cast<UINT_PTR>(syncRunnableRef))) {
    return false;
  }

  bool result = syncRunnable->WaitUntilComplete();
  mDuration = syncRunnable->GetDuration();
  return result;
}

/* static */ VOID CALLBACK
MainThreadInvoker::MainThreadAPC(ULONG_PTR aParam)
{
  AUTO_PROFILER_THREAD_WAKE;
  mozilla::HangMonitor::NotifyActivity(mozilla::HangMonitor::kGeneralActivity);
  MOZ_ASSERT(NS_IsMainThread());
  auto runnable = reinterpret_cast<SyncRunnable*>(aParam);
  runnable->Run();
  NS_RELEASE(runnable);
}

} // namespace mscom
} // namespace mozilla
