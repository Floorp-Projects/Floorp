/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/MainThreadInvoker.h"

#include "GeckoProfiler.h"
#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/HangMonitor.h"
#include "mozilla/RefPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsSystemInfo.h"
#include "private/prpriv.h" // For PR_GetThreadID
#include "WinUtils.h"

// This gives us compiler intrinsics for the x86 PAUSE instruction
#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_mm_pause)
#define CPU_PAUSE() _mm_pause()
#elif defined(__GNUC__) || defined(__clang__)
#define CPU_PAUSE() __builtin_ia32_pause()
#endif

static bool sIsMulticore;

namespace {

/**
 * SyncRunnable implements different code paths depending on whether or not
 * we are running on a multiprocessor system. In the multiprocessor case, we
 * leave the thread in a spin loop while waiting for the main thread to execute
 * our runnable. Since spinning is pointless in the uniprocessor case, we block
 * on an event that is set by the main thread once it has finished the runnable.
 */
class MOZ_RAII SyncRunnable
{
public:
  explicit SyncRunnable(already_AddRefed<nsIRunnable>&& aRunnable)
    : mDoneEvent(sIsMulticore ? nullptr :
                 ::CreateEventW(nullptr, FALSE, FALSE, nullptr))
    , mDone(false)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(sIsMulticore || mDoneEvent);
    MOZ_ASSERT(mRunnable);
  }

  ~SyncRunnable()
  {
    if (mDoneEvent) {
      ::CloseHandle(mDoneEvent);
    }
  }

  void Run()
  {
    mRunnable->Run();

    if (mDoneEvent) {
      ::SetEvent(mDoneEvent);
    } else {
      mDone = true;
    }
  }

  bool WaitUntilComplete()
  {
    if (mDoneEvent) {
      HANDLE handles[] = {mDoneEvent,
                          mozilla::mscom::MainThreadInvoker::GetTargetThread()};
      DWORD waitResult = ::WaitForMultipleObjects(mozilla::ArrayLength(handles),
                                                  handles, FALSE, INFINITE);
      return waitResult == WAIT_OBJECT_0;
    }

    while (!mDone) {
      // The PAUSE instruction is a hint to the CPU that we're doing a spin
      // loop. It is a no-op on older processors that don't support it, so
      // it is safe to use here without any CPUID checks.
      CPU_PAUSE();
    }
    return true;
  }

private:
  HANDLE                mDoneEvent;
  mozilla::Atomic<bool> mDone;
  nsCOMPtr<nsIRunnable> mRunnable;
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

  nsCOMPtr<nsIPropertyBag2> infoService = do_GetService(NS_SYSTEMINFO_CONTRACTID);
  if (infoService) {
    uint32_t cpuCount;
    nsresult rv = infoService->GetPropertyAsUint32(NS_LITERAL_STRING("cpucount"),
                                                   &cpuCount);
    sIsMulticore = NS_SUCCEEDED(rv) && cpuCount > 1;
  }

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

  SyncRunnable syncRunnable(runnable.forget());

  if (!::QueueUserAPC(&MainThreadAPC, sMainThread,
                      reinterpret_cast<UINT_PTR>(&syncRunnable))) {
    return false;
  }

  // We should ensure a call to NtTestAlert() is made on the main thread so
  // that the main thread will check for APCs during event processing. If we
  // omit this then the main thread will not check its APC queue until it is
  // idle.
  widget::WinUtils::SetAPCPending();

  return syncRunnable.WaitUntilComplete();
}

/* static */ VOID CALLBACK
MainThreadInvoker::MainThreadAPC(ULONG_PTR aParam)
{
  GeckoProfilerWakeRAII wakeProfiler;
  mozilla::HangMonitor::NotifyActivity(mozilla::HangMonitor::kGeneralActivity);
  MOZ_ASSERT(NS_IsMainThread());
  auto runnable = reinterpret_cast<SyncRunnable*>(aParam);
  runnable->Run();
}

} // namespace mscom
} // namespace mozilla
