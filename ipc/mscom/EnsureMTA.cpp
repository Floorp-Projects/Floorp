/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/EnsureMTA.h"

#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/mscom/COMWrappers.h"
#include "mozilla/mscom/ProcessRuntime.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StaticLocalPtr.h"
#include "nsThreadManager.h"
#include "nsThreadUtils.h"

#include "private/pprthred.h"

namespace {

class EnterMTARunnable : public mozilla::Runnable {
 public:
  EnterMTARunnable() : mozilla::Runnable("EnterMTARunnable") {}
  NS_IMETHOD Run() override {
    mozilla::DebugOnly<HRESULT> hr =
        mozilla::mscom::wrapped::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    MOZ_ASSERT(SUCCEEDED(hr));
    return NS_OK;
  }
};

class BackgroundMTAData {
 public:
  BackgroundMTAData() {
    nsCOMPtr<nsIRunnable> runnable = new EnterMTARunnable();
    mozilla::DebugOnly<nsresult> rv = NS_NewNamedThread(
        "COM MTA", getter_AddRefs(mThread), runnable.forget());
    NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "NS_NewNamedThread failed");
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  ~BackgroundMTAData() {
    if (mThread) {
      mThread->Dispatch(
          NS_NewRunnableFunction("BackgroundMTAData::~BackgroundMTAData",
                                 &mozilla::mscom::wrapped::CoUninitialize),
          NS_DISPATCH_NORMAL);
      mThread->Shutdown();
    }
  }

  nsCOMPtr<nsIThread> GetThread() const { return mThread; }

 private:
  nsCOMPtr<nsIThread> mThread;
};

}  // anonymous namespace

namespace mozilla {
namespace mscom {

EnsureMTA::EnsureMTA() {
  MOZ_ASSERT(NS_IsMainThread());

  // It is possible that we're running so early that we might need to start
  // the thread manager ourselves. We do this here to guarantee that we have
  // the ability to start the persistent MTA thread at any moment beyond this
  // point.
  nsresult rv = nsThreadManager::get().Init();
  // We intentionally don't check rv unless we need it when
  // CoIncremementMTAUsage is unavailable.

  // Calling this function initializes the MTA without needing to explicitly
  // create a thread and call CoInitializeEx to do it.
  // We don't retain the cookie because once we've incremented the MTA, we
  // leave it that way for the lifetime of the process.
  CO_MTA_USAGE_COOKIE mtaCookie = nullptr;
  HRESULT hr = wrapped::CoIncrementMTAUsage(&mtaCookie);
  if (SUCCEEDED(hr)) {
    if (NS_SUCCEEDED(rv)) {
      // Start the persistent MTA thread (mostly) asynchronously.
      Unused << GetPersistentMTAThread();
    }

    return;
  }

  // In the fallback case, we simply initialize our persistent MTA thread.

  // Make sure thread manager init succeeded before trying to initialize the
  // persistent MTA thread.
  MOZ_DIAGNOSTIC_ASSERT(NS_SUCCEEDED(rv));
  if (NS_FAILED(rv)) {
    return;
  }

  // Before proceeding any further, pump a runnable through the persistent MTA
  // thread to ensure that it is up and running and has finished initializing
  // the multi-threaded apartment.
  nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
      "EnsureMTA::EnsureMTA()",
      []() { MOZ_RELEASE_ASSERT(IsCurrentThreadExplicitMTA()); }));
  SyncDispatchToPersistentThread(runnable);
}

/* static */
RefPtr<EnsureMTA::CreateInstanceAgileRefPromise>
EnsureMTA::CreateInstanceInternal(REFCLSID aClsid, REFIID aIid) {
  MOZ_ASSERT(IsCurrentThreadExplicitMTA());

  RefPtr<IUnknown> iface;
  HRESULT hr = wrapped::CoCreateInstance(aClsid, nullptr, CLSCTX_INPROC_SERVER,
                                         aIid, getter_AddRefs(iface));
  if (FAILED(hr)) {
    return CreateInstanceAgileRefPromise::CreateAndReject(hr, __func__);
  }

  // We need to use the two argument constructor for AgileReference because our
  // RefPtr is not parameterized on the specific interface being requested.
  AgileReference agileRef(aIid, iface);
  if (!agileRef) {
    return CreateInstanceAgileRefPromise::CreateAndReject(agileRef.GetHResult(),
                                                          __func__);
  }

  return CreateInstanceAgileRefPromise::CreateAndResolve(std::move(agileRef),
                                                         __func__);
}

/* static */
RefPtr<EnsureMTA::CreateInstanceAgileRefPromise> EnsureMTA::CreateInstance(
    REFCLSID aClsid, REFIID aIid) {
  MOZ_ASSERT(IsCOMInitializedOnCurrentThread());

  const bool isClassOk = IsClassThreadAwareInprocServer(aClsid);
  MOZ_ASSERT(isClassOk,
             "mozilla::mscom::EnsureMTA::CreateInstance is not "
             "safe/performant/necessary to use with this CLSID. This CLSID "
             "either does not support creation from within a multithreaded "
             "apartment, or it is not an in-process server.");
  if (!isClassOk) {
    return CreateInstanceAgileRefPromise::CreateAndReject(CO_E_NOT_SUPPORTED,
                                                          __func__);
  }

  if (IsCurrentThreadExplicitMTA()) {
    // It's safe to immediately call CreateInstanceInternal
    return CreateInstanceInternal(aClsid, aIid);
  }

  // aClsid and aIid are references. Make local copies that we can put into the
  // lambda in case the sources of aClsid or aIid are not static data
  CLSID localClsid = aClsid;
  IID localIid = aIid;

  auto invoker = [localClsid,
                  localIid]() -> RefPtr<CreateInstanceAgileRefPromise> {
    return CreateInstanceInternal(localClsid, localIid);
  };

  nsCOMPtr<nsIThread> mtaThread(GetPersistentMTAThread());

  return InvokeAsync(mtaThread, __func__, std::move(invoker));
}

/* static */
nsCOMPtr<nsIThread> EnsureMTA::GetPersistentMTAThread() {
  static StaticLocalAutoPtr<BackgroundMTAData> sMTAData(
      []() -> BackgroundMTAData* {
        BackgroundMTAData* bgData = new BackgroundMTAData();

        auto setClearOnShutdown = [ptr = &sMTAData]() -> void {
          ClearOnShutdown(ptr, ShutdownPhase::XPCOMShutdownThreads);
        };

        if (NS_IsMainThread()) {
          setClearOnShutdown();
          return bgData;
        }

        SchedulerGroup::Dispatch(
            NS_NewRunnableFunction("mscom::EnsureMTA::GetPersistentMTAThread",
                                   std::move(setClearOnShutdown)));

        return bgData;
      }());

  MOZ_ASSERT(sMTAData);

  return sMTAData->GetThread();
}

/* static */
void EnsureMTA::SyncDispatchToPersistentThread(nsIRunnable* aRunnable) {
  nsCOMPtr<nsIThread> thread(GetPersistentMTAThread());
  MOZ_ASSERT(thread);
  if (!thread) {
    return;
  }

  // Note that, due to APC dispatch, we might reenter this function while we
  // wait on this event. We therefore need a unique event object for each
  // entry into this function. If perf becomes an issue then we will want to
  // maintain an array of events where the Nth event is unique to the Nth
  // reentry.
  nsAutoHandle event(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
  if (!event) {
    return;
  }

  HANDLE eventHandle = event.get();
  auto eventSetter = [&aRunnable, eventHandle]() -> void {
    aRunnable->Run();
    ::SetEvent(eventHandle);
  };

  nsresult rv = thread->Dispatch(
      NS_NewRunnableFunction("mscom::EnsureMTA::SyncDispatchToPersistentThread",
                             std::move(eventSetter)),
      NS_DISPATCH_NORMAL);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  if (NS_FAILED(rv)) {
    return;
  }

  AUTO_PROFILER_THREAD_SLEEP;
  DWORD waitResult;
  while ((waitResult = ::WaitForSingleObjectEx(event, INFINITE, FALSE)) ==
         WAIT_IO_COMPLETION) {
  }
  MOZ_ASSERT(waitResult == WAIT_OBJECT_0);
}

/**
 * While this function currently appears to be redundant, it may become more
 * sophisticated in the future. For example, we could optionally dispatch to an
 * MTA context if we wanted to utilize the MTA thread pool.
 */
/* static */
void EnsureMTA::SyncDispatch(nsCOMPtr<nsIRunnable>&& aRunnable, Option aOpt) {
  SyncDispatchToPersistentThread(aRunnable);
}

}  // namespace mscom
}  // namespace mozilla
