/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/MainThreadInvoker.h"

#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/BackgroundHangMonitor.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/mscom/SpinEvent.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "private/prpriv.h"  // For PR_GetThreadID
#include <winternl.h>        // For NTSTATUS and NTAPI

namespace {

typedef NTSTATUS(NTAPI* NtTestAlertPtr)(VOID);

/**
 * SyncRunnable implements different code paths depending on whether or not
 * we are running on a multiprocessor system. In the multiprocessor case, we
 * leave the thread in a spin loop while waiting for the main thread to execute
 * our runnable. Since spinning is pointless in the uniprocessor case, we block
 * on an event that is set by the main thread once it has finished the runnable.
 */
class SyncRunnable : public mozilla::Runnable {
 public:
  explicit SyncRunnable(already_AddRefed<nsIRunnable> aRunnable)
      : mozilla::Runnable("MainThreadInvoker"), mRunnable(aRunnable) {
    static const bool gotStatics = InitStatics();
    MOZ_ASSERT(gotStatics);
    Unused << gotStatics;
  }

  ~SyncRunnable() = default;

  NS_IMETHOD Run() override {
    if (mHasRun) {
      // The APC already ran, so we have nothing to do.
      return NS_OK;
    }

    // Run the pending APC in the queue.
    MOZ_ASSERT(sNtTestAlert);
    sNtTestAlert();
    return NS_OK;
  }

  // This is called by MainThreadInvoker::MainThreadAPC.
  void APCRun() {
    mHasRun = true;

    TimeStamp runStart(TimeStamp::Now());
    mRunnable->Run();
    TimeStamp runEnd(TimeStamp::Now());

    mDuration = runEnd - runStart;

    mEvent.Signal();
  }

  bool WaitUntilComplete() {
    return mEvent.Wait(mozilla::mscom::MainThreadInvoker::GetTargetThread());
  }

  const mozilla::TimeDuration& GetDuration() const { return mDuration; }

 private:
  bool mHasRun = false;
  nsCOMPtr<nsIRunnable> mRunnable;
  mozilla::mscom::SpinEvent mEvent;
  mozilla::TimeDuration mDuration;

  static NtTestAlertPtr sNtTestAlert;

  static bool InitStatics() {
    sNtTestAlert = reinterpret_cast<NtTestAlertPtr>(
        ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "NtTestAlert"));
    MOZ_ASSERT(sNtTestAlert);
    return sNtTestAlert;
  }
};

NtTestAlertPtr SyncRunnable::sNtTestAlert = nullptr;

}  // anonymous namespace

namespace mozilla {
namespace mscom {

HANDLE MainThreadInvoker::sMainThread = nullptr;

/* static */
bool MainThreadInvoker::InitStatics() {
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

MainThreadInvoker::MainThreadInvoker() {
  static const bool gotStatics = InitStatics();
  MOZ_ASSERT(gotStatics);
  Unused << gotStatics;
}

bool MainThreadInvoker::Invoke(already_AddRefed<nsIRunnable>&& aRunnable) {
  nsCOMPtr<nsIRunnable> runnable(std::move(aRunnable));
  if (!runnable) {
    return false;
  }

  if (NS_IsMainThread()) {
    runnable->Run();
    return true;
  }

  RefPtr<SyncRunnable> syncRunnable = new SyncRunnable(runnable.forget());

  // The main thread could be either blocked on a condition variable waiting
  // for a Gecko event, or it could be blocked waiting on a Windows HANDLE in
  // IPC code (doing a sync message send). In the former case, we wake it by
  // posting a Gecko runnable to the main thread. In the latter case, we wake
  // it using an APC. However, the latter case doesn't happen very often now
  // and APCs aren't otherwise run by the main thread. To ensure the
  // SyncRunnable is cleaned up, we need both to run consistently.
  // To do this, we:
  // 1. Queue an APC which does the actual work.
  // This ref gets released in MainThreadAPC when it runs.
  SyncRunnable* syncRunnableRef = syncRunnable.get();
  NS_ADDREF(syncRunnableRef);
  if (!::QueueUserAPC(&MainThreadAPC, sMainThread,
                      reinterpret_cast<UINT_PTR>(syncRunnableRef))) {
    return false;
  }

  // 2. Post a Gecko runnable (which always runs). If the APC hasn't run, the
  // Gecko runnable runs it. Otherwise, it does nothing.
  if (NS_FAILED(SchedulerGroup::Dispatch(TaskCategory::Other,
                                         do_AddRef(syncRunnable)))) {
    return false;
  }

  bool result = syncRunnable->WaitUntilComplete();
  mDuration = syncRunnable->GetDuration();
  return result;
}

/* static */ VOID CALLBACK MainThreadInvoker::MainThreadAPC(ULONG_PTR aParam) {
  AUTO_PROFILER_THREAD_WAKE;
  mozilla::BackgroundHangMonitor().NotifyActivity();
  MOZ_ASSERT(NS_IsMainThread());
  auto runnable = reinterpret_cast<SyncRunnable*>(aParam);
  runnable->APCRun();
  NS_RELEASE(runnable);
}

}  // namespace mscom
}  // namespace mozilla
