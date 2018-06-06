/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkletThread.h"
#include "prthread.h"
#include "nsContentUtils.h"
#include "nsCycleCollector.h"
#include "mozilla/dom/AtomList.h"
#include "mozilla/EventQueue.h"
#include "mozilla/ThreadEventQueue.h"

namespace mozilla {
namespace dom {

namespace {

// The size of the worklet runtime heaps in bytes.
#define WORKLET_DEFAULT_RUNTIME_HEAPSIZE 32 * 1024 * 1024

// The size of the generational GC nursery for worklet, in bytes.
#define WORKLET_DEFAULT_NURSERY_SIZE 1 * 1024 * 1024

// The C stack size. We use the same stack size on all platforms for
// consistency.
const uint32_t kWorkletStackSize = 256 * sizeof(size_t) * 1024;

// Helper functions

bool
PreserveWrapper(JSContext* aCx, JSObject* aObj)
{
  MOZ_ASSERT(aCx);
  MOZ_ASSERT(aObj);
  MOZ_ASSERT(mozilla::dom::IsDOMObject(aObj));
  return mozilla::dom::TryPreserveWrapper(aObj);
}

void
DestroyWorkletPrincipals(JSPrincipals* aPrincipals)
{
  MOZ_ASSERT_UNREACHABLE("Worklet principals refcount should never fall below one");
}

JSObject*
Wrap(JSContext* aCx, JS::HandleObject aExisting, JS::HandleObject aObj)
{
  if (aExisting) {
    js::Wrapper::Renew(aExisting, aObj,
                       &js::OpaqueCrossCompartmentWrapper::singleton);
  }

  return js::Wrapper::New(aCx, aObj,
                          &js::OpaqueCrossCompartmentWrapper::singleton);
}

const JSWrapObjectCallbacks WrapObjectCallbacks =
{
  Wrap,
  nullptr,
};

} // namespace

// This classes control CC in the worklet thread.

class WorkletJSRuntime final : public mozilla::CycleCollectedJSRuntime
{
public:
  explicit WorkletJSRuntime(JSContext* aCx)
    : CycleCollectedJSRuntime(aCx)
  {
  }

  ~WorkletJSRuntime() override = default;

  virtual void
  PrepareForForgetSkippable() override
  {
  }

  virtual void
  BeginCycleCollectionCallback() override
  {
  }

  virtual void
  EndCycleCollectionCallback(CycleCollectorResults& aResults) override
  {
  }

  virtual void
  DispatchDeferredDeletion(bool aContinuation, bool aPurge) override
  {
    MOZ_ASSERT(!aContinuation);
    nsCycleCollector_doDeferredDeletion();
  }

  virtual void
  CustomGCCallback(JSGCStatus aStatus) override
  {
    // nsCycleCollector_collect() requires a cycle collector but
    // ~WorkletJSContext calls nsCycleCollector_shutdown() and the base class
    // destructor will trigger a final GC.  The nsCycleCollector_collect()
    // call can be skipped in this GC as ~CycleCollectedJSContext removes the
    // context from |this|.
    if (aStatus == JSGC_END && !Contexts().isEmpty()) {
      nsCycleCollector_collect(nullptr);
    }
  }
};

class WorkletJSContext final : public CycleCollectedJSContext
{
public:
  explicit WorkletJSContext(WorkletThread* aWorkletThread)
    : mWorkletThread(aWorkletThread)
  {
    MOZ_ASSERT(aWorkletThread);
    MOZ_ASSERT(!NS_IsMainThread());

    nsCycleCollector_startup();
  }

  ~WorkletJSContext() override
  {
    MOZ_ASSERT(!NS_IsMainThread());

    JSContext* cx = MaybeContext();
    if (!cx) {
      return;   // Initialize() must have failed
    }

    nsCycleCollector_shutdown();
  }

  WorkletJSContext* GetAsWorkletJSContext() override { return this; }

  CycleCollectedJSRuntime* CreateRuntime(JSContext* aCx) override
  {
    return new WorkletJSRuntime(aCx);
  }

  nsresult
  Initialize(JSRuntime* aParentRuntime)
  {
    MOZ_ASSERT(!NS_IsMainThread());

    nsresult rv =
      CycleCollectedJSContext::Initialize(aParentRuntime,
                                          WORKLET_DEFAULT_RUNTIME_HEAPSIZE,
                                          WORKLET_DEFAULT_NURSERY_SIZE);
     if (NS_WARN_IF(NS_FAILED(rv))) {
       return rv;
     }

    JSContext* cx = Context();

    js::SetPreserveWrapperCallback(cx, PreserveWrapper);
    JS_InitDestroyPrincipalsCallback(cx, DestroyWorkletPrincipals);
    JS_SetWrapObjectCallbacks(cx, &WrapObjectCallbacks);
    JS_SetFutexCanWait(cx);

    return NS_OK;
  }

  void
  DispatchToMicroTask(already_AddRefed<MicroTaskRunnable> aRunnable) override
  {
    RefPtr<MicroTaskRunnable> runnable(aRunnable);

#ifdef DEBUG
    MOZ_ASSERT(!NS_IsMainThread());
    MOZ_ASSERT(runnable);

    WorkletThread* workletThread = WorkletThread::Get();
    MOZ_ASSERT(workletThread);

    JSContext* cx = workletThread->GetJSContext();
    MOZ_ASSERT(cx);

    JS::Rooted<JSObject*> global(cx, JS::CurrentGlobalOrNull(cx));
    MOZ_ASSERT(global);
#endif

    GetMicroTaskQueue().push(runnable.forget());
  }

  WorkletThread* GetWorkletThread() const
  {
    return mWorkletThread;
  }

  bool IsSystemCaller() const override
  {
    // Currently no support for special system worklet privileges.
    return false;
  }

private:
  RefPtr<WorkletThread> mWorkletThread;
};

// This is the first runnable to be dispatched. It calls the RunEventLoop() so
// basically everything happens into this runnable. The reason behind this
// approach is that, when the Worklet is terminated, it must not have any JS in
// stack, but, because we have CC, nsIThread creates an AutoNoJSAPI object by
// default. Using this runnable, CC exists only into it.
class WorkletThread::PrimaryRunnable final : public Runnable
{
public:
  explicit PrimaryRunnable(WorkletThread* aWorkletThread)
    : Runnable("WorkletThread::PrimaryRunnable")
    , mWorkletThread(aWorkletThread)
  {
    MOZ_ASSERT(aWorkletThread);
    MOZ_ASSERT(NS_IsMainThread());

    mParentRuntime =
      JS_GetParentRuntime(CycleCollectedJSContext::Get()->Context());
    MOZ_ASSERT(mParentRuntime);
  }

  NS_IMETHOD
  Run() override
  {
    mWorkletThread->RunEventLoop(mParentRuntime);
    return NS_OK;
  }

private:
  RefPtr<WorkletThread> mWorkletThread;
  JSRuntime* mParentRuntime;
};

// This is the last runnable to be dispatched. It calls the TerminateInternal()
class WorkletThread::TerminateRunnable final : public Runnable
{
public:
  explicit TerminateRunnable(WorkletThread* aWorkletThread)
    : Runnable("WorkletThread::TerminateRunnable")
    , mWorkletThread(aWorkletThread)
  {
    MOZ_ASSERT(aWorkletThread);
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD
  Run() override
  {
    mWorkletThread->TerminateInternal();
    return NS_OK;
  }

private:
  RefPtr<WorkletThread> mWorkletThread;
};

WorkletThread::WorkletThread(const WorkletLoadInfo& aWorkletLoadInfo)
  : nsThread(MakeNotNull<ThreadEventQueue<mozilla::EventQueue>*>(
               MakeUnique<mozilla::EventQueue>()),
             nsThread::NOT_MAIN_THREAD, kWorkletStackSize)
  , mWorkletLoadInfo(aWorkletLoadInfo)
  , mCreationTimeStamp(TimeStamp::Now())
  , mJSContext(nullptr)
  , mIsTerminating(false)
{
  MOZ_ASSERT(NS_IsMainThread());
  nsContentUtils::RegisterShutdownObserver(this);
}

WorkletThread::~WorkletThread()
{
  // This should be gone during the termination step.
  MOZ_ASSERT(!mJSContext);
}

// static
already_AddRefed<WorkletThread>
WorkletThread::Create(const WorkletLoadInfo& aWorkletLoadInfo)
{
  RefPtr<WorkletThread> thread =
    new WorkletThread(aWorkletLoadInfo);
  if (NS_WARN_IF(NS_FAILED(thread->Init()))) {
    return nullptr;
  }

  RefPtr<PrimaryRunnable> runnable = new PrimaryRunnable(thread);
  if (NS_WARN_IF(NS_FAILED(thread->DispatchRunnable(runnable.forget())))) {
    return nullptr;
  }

  return thread.forget();
}

nsresult
WorkletThread::DispatchRunnable(already_AddRefed<nsIRunnable> aRunnable)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return nsThread::Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
WorkletThread::DispatchFromScript(nsIRunnable* aRunnable, uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);
  return Dispatch(runnable.forget(), aFlags);
}

NS_IMETHODIMP
WorkletThread::Dispatch(already_AddRefed<nsIRunnable> aRunnable,
                        uint32_t aFlags)
{
  nsCOMPtr<nsIRunnable> runnable(aRunnable);

  // Worklet only supports asynchronous dispatch.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  return nsThread::Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
}

NS_IMETHODIMP
WorkletThread::DelayedDispatch(already_AddRefed<nsIRunnable>, uint32_t aFlags)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
WorkletThread::RunEventLoop(JSRuntime* aParentRuntime)
{
  MOZ_ASSERT(!NS_IsMainThread());

  PR_SetCurrentThreadName("worklet");

  auto context = MakeUnique<WorkletJSContext>(this);
  nsresult rv = context->Initialize(aParentRuntime);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    // TODO: error propagation
    return;
  }

  // FIXME: JS_SetDefaultLocale
  // FIXME: JSSettings
  // FIXME: JS_SetNativeStackQuota
  // FIXME: JS_SetSecurityCallbacks
  // FIXME: JS::SetAsmJSCacheOps
  // FIXME: JS::SetAsyncTaskCallbacks
  // FIXME: JS_AddInterruptCallback
  // FIXME: JS::SetCTypesActivityCallback
  // FIXME: JS_SetGCZeal

  if (!JS::InitSelfHostedCode(context->Context())) {
    // TODO: error propagation
    return;
  }

  mJSContext = context->Context();

  while (mJSContext) {
    MOZ_ALWAYS_TRUE(NS_ProcessNextEvent(this, /* wait: */ true));
  }

  MOZ_ASSERT(mJSContext == nullptr);
}

void
WorkletThread::Terminate()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (mIsTerminating) {
    // nsThread::Dispatch() would leak the runnable if the event queue is no
    // longer accepting runnables.
    return;
  }

  mIsTerminating = true;

  nsContentUtils::UnregisterShutdownObserver(this);

  RefPtr<TerminateRunnable> runnable = new TerminateRunnable(this);
  DispatchRunnable(runnable.forget());
}

void
WorkletThread::TerminateInternal()
{
  AssertIsOnWorkletThread();

  mJSContext = nullptr;

  nsCOMPtr<nsIRunnable> runnable =
    NewRunnableMethod("WorkletThread::Shutdown", this,
                      &WorkletThread::Shutdown);
  NS_DispatchToMainThread(runnable);
}

JSContext*
WorkletThread::GetJSContext() const
{
  AssertIsOnWorkletThread();
  MOZ_ASSERT(mJSContext);
  return mJSContext;
}

const WorkletLoadInfo&
WorkletThread::GetWorkletLoadInfo() const
{
  return mWorkletLoadInfo;
}

/* static */ bool
WorkletThread::IsOnWorkletThread()
{
  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();
  return ccjscx && ccjscx->GetAsWorkletJSContext();
}

/* static */ void
WorkletThread::AssertIsOnWorkletThread()
{
  MOZ_ASSERT(IsOnWorkletThread());
}

/* static */ WorkletThread*
WorkletThread::Get()
{
  AssertIsOnWorkletThread();

  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();
  MOZ_ASSERT(ccjscx);

  WorkletJSContext* workletjscx = ccjscx->GetAsWorkletJSContext();
  MOZ_ASSERT(workletjscx);
  return workletjscx->GetWorkletThread();
}

// nsIObserver
NS_IMETHODIMP
WorkletThread::Observe(nsISupports* aSubject, const char* aTopic,
                       const char16_t*)
{
  MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

  Terminate();
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED(WorkletThread, nsThread, nsIObserver)

} // namespace dom
} // namespace mozilla
