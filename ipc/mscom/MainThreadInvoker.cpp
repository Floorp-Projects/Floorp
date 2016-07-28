/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/MainThreadInvoker.h"

#include "MainThreadUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/RefPtr.h"
#include "private/prpriv.h" // For PR_GetThreadID

#include <winternl.h> // For NTSTATUS and NTAPI

namespace {

class SyncRunnable : public mozilla::Runnable
{
public:
  SyncRunnable(HANDLE aEvent, already_AddRefed<nsIRunnable>&& aRunnable)
    : mDoneEvent(aEvent)
    , mRunnable(aRunnable)
  {
    MOZ_ASSERT(aEvent);
    MOZ_ASSERT(mRunnable);
  }

  NS_IMETHOD Run() override
  {
    mRunnable->Run();
    ::SetEvent(mDoneEvent);
    return NS_OK;
  }

private:
  HANDLE                mDoneEvent;
  nsCOMPtr<nsIRunnable> mRunnable;
};

typedef NTSTATUS (NTAPI* NtTestAlertPtr)(VOID);

} // anonymous namespace

namespace mozilla {
namespace mscom {

HANDLE MainThreadInvoker::sMainThread = nullptr;
StaticRefPtr<nsIRunnable> MainThreadInvoker::sAlertRunnable;

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
  if (!sMainThread) {
    return false;
  }
  NtTestAlertPtr NtTestAlert =
    reinterpret_cast<NtTestAlertPtr>(
        ::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "NtTestAlert"));
  sAlertRunnable = ::NS_NewRunnableFunction([NtTestAlert]() -> void {
    // We're using NtTestAlert() instead of SleepEx() so that the main thread
    // never gives up its quantum if there are no APCs pending.
    NtTestAlert();
  }).take();
  if (sAlertRunnable) {
    ClearOnShutdown(&sAlertRunnable);
  }
  return !!sAlertRunnable;
}

MainThreadInvoker::MainThreadInvoker()
  : mDoneEvent(::CreateEvent(nullptr, FALSE, FALSE, nullptr))
{
  static const bool gotStatics = InitStatics();
  MOZ_ASSERT(gotStatics);
}

MainThreadInvoker::~MainThreadInvoker()
{
  if (mDoneEvent) {
    ::CloseHandle(mDoneEvent);
  }
}

bool
MainThreadInvoker::WaitForCompletion(DWORD aTimeout)
{
  HANDLE handles[] = {mDoneEvent, sMainThread};
  DWORD waitResult = ::WaitForMultipleObjects(ArrayLength(handles), handles,
                                              FALSE, aTimeout);
  return waitResult == WAIT_OBJECT_0;
}

bool
MainThreadInvoker::Invoke(already_AddRefed<nsIRunnable>&& aRunnable,
                          DWORD aTimeout)
{
  nsCOMPtr<nsIRunnable> runnable(Move(aRunnable));
  if (!runnable) {
    return false;
  }
  if (NS_IsMainThread()) {
    runnable->Run();
    return true;
  }
  RefPtr<SyncRunnable> wrappedRunnable(new SyncRunnable(mDoneEvent,
                                                        runnable.forget()));
  // Make sure that wrappedRunnable remains valid while sitting in the APC queue
  wrappedRunnable->AddRef();
  if (!::QueueUserAPC(&MainThreadAPC, sMainThread,
                      reinterpret_cast<UINT_PTR>(wrappedRunnable.get()))) {
    // Enqueue failed so cancel the above AddRef
    wrappedRunnable->Release();
    return false;
  }
  // We should enqueue a call to NtTestAlert() so that the main thread will
  // check for APCs during event processing. If we omit this then the main
  // thread will not check its APC queue until it is idle. Note that failing to
  // dispatch this event is non-fatal, but it will delay execution of the APC.
  NS_WARN_IF(NS_FAILED(NS_DispatchToMainThread(sAlertRunnable)));
  return WaitForCompletion(aTimeout);
}

/* static */ VOID CALLBACK
MainThreadInvoker::MainThreadAPC(ULONG_PTR aParam)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<SyncRunnable> runnable(already_AddRefed<SyncRunnable>(
                                  reinterpret_cast<SyncRunnable*>(aParam)));
  runnable->Run();
}

} // namespace mscom
} // namespace mozilla
