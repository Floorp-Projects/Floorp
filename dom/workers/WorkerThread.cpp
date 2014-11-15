/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerThread.h"

#include "mozilla/Assertions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "nsIThreadInternal.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"

#ifdef DEBUG
#include "nsThreadManager.h"
#endif

namespace mozilla {
namespace dom {
namespace workers {

using namespace mozilla::ipc;

namespace {

// The C stack size. We use the same stack size on all platforms for
// consistency.
const uint32_t kWorkerStackSize = 256 * sizeof(size_t) * 1024;

} // anonymous namespace

class WorkerThread::Observer MOZ_FINAL
  : public nsIThreadObserver
{
  WorkerPrivate* mWorkerPrivate;

public:
  explicit Observer(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  ~Observer()
  {
    mWorkerPrivate->AssertIsOnWorkerThread();
  }

  NS_DECL_NSITHREADOBSERVER
};

WorkerThread::WorkerThread()
: nsThread(nsThread::NOT_MAIN_THREAD, kWorkerStackSize),
  mWorkerPrivate(nullptr)
#ifdef DEBUG
  , mAcceptingNonWorkerRunnables(true)
#endif
{
}

WorkerThread::~WorkerThread()
{
}

// static
already_AddRefed<WorkerThread>
WorkerThread::Create()
{
  MOZ_ASSERT(nsThreadManager::get());

  nsRefPtr<WorkerThread> thread = new WorkerThread();
  if (NS_FAILED(thread->Init())) {
    NS_WARNING("Failed to create new thread!");
    return nullptr;
  }

  NS_SetThreadName(thread, "DOM Worker");

  return thread.forget();
}

void
WorkerThread::SetWorker(WorkerPrivate* aWorkerPrivate)
{
  MOZ_ASSERT(PR_GetCurrentThread() == mThread);
  MOZ_ASSERT_IF(aWorkerPrivate, !mWorkerPrivate);
  MOZ_ASSERT_IF(!aWorkerPrivate, mWorkerPrivate);

  // No need to lock here because mWorkerPrivate is only modified on mThread.

  if (mWorkerPrivate) {
    MOZ_ASSERT(mObserver);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(RemoveObserver(mObserver)));

    mObserver = nullptr;
    mWorkerPrivate->SetThread(nullptr);
  }

  mWorkerPrivate = aWorkerPrivate;

  if (mWorkerPrivate) {
    mWorkerPrivate->SetThread(this);

    nsRefPtr<Observer> observer = new Observer(mWorkerPrivate);

    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(AddObserver(observer)));

    mObserver.swap(observer);
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(WorkerThread, nsThread)

NS_IMETHODIMP
WorkerThread::Dispatch(nsIRunnable* aRunnable, uint32_t aFlags)
{
  // May be called on any thread!

#ifdef DEBUG
  if (PR_GetCurrentThread() == mThread) {
    MOZ_ASSERT(mWorkerPrivate);
    mWorkerPrivate->AssertIsOnWorkerThread();
  }
  else if (aRunnable && !IsAcceptingNonWorkerRunnables()) {
    // Only enforce cancelable runnables after we've started the worker loop.
    nsCOMPtr<nsICancelableRunnable> cancelable = do_QueryInterface(aRunnable);
    MOZ_ASSERT(cancelable,
               "Should have been wrapped by the worker's event target!");
  }
#endif

  // Workers only support asynchronous dispatch for now.
  if (NS_WARN_IF(aFlags != NS_DISPATCH_NORMAL)) {
    return NS_ERROR_UNEXPECTED;
  }

  nsIRunnable* runnableToDispatch;
  nsRefPtr<WorkerRunnable> workerRunnable;

  if (aRunnable && PR_GetCurrentThread() == mThread) {
    // No need to lock here because mWorkerPrivate is only modified on mThread.
    workerRunnable = mWorkerPrivate->MaybeWrapAsWorkerRunnable(aRunnable);
    runnableToDispatch = workerRunnable;
  }
  else {
    runnableToDispatch = aRunnable;
  }

  nsresult rv = nsThread::Dispatch(runnableToDispatch, NS_DISPATCH_NORMAL);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

NS_IMPL_ISUPPORTS(WorkerThread::Observer, nsIThreadObserver)

NS_IMETHODIMP
WorkerThread::Observer::OnDispatchedEvent(nsIThreadInternal* /* aThread */)
{
  MOZ_CRASH("OnDispatchedEvent() should never be called!");
}

NS_IMETHODIMP
WorkerThread::Observer::OnProcessNextEvent(nsIThreadInternal* /* aThread */,
                                           bool aMayWait,
                                           uint32_t aRecursionDepth)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  // If the PBackground child is not created yet, then we must permit
  // blocking event processing to support SynchronouslyCreatePBackground().
  // If this occurs then we are spinning on the event queue at the start of
  // PrimaryWorkerRunnable::Run() and don't want to process the event in
  // mWorkerPrivate yet.
  if (aMayWait) {
    MOZ_ASSERT(aRecursionDepth == 2);
    MOZ_ASSERT(!BackgroundChild::GetForCurrentThread());
    return NS_OK;
  }

  mWorkerPrivate->OnProcessNextEvent(aRecursionDepth);
  return NS_OK;
}

NS_IMETHODIMP
WorkerThread::Observer::AfterProcessNextEvent(nsIThreadInternal* /* aThread */,
                                              uint32_t aRecursionDepth,
                                              bool /* aEventWasProcessed */)
{
  mWorkerPrivate->AssertIsOnWorkerThread();

  mWorkerPrivate->AfterProcessNextEvent(aRecursionDepth);
  return NS_OK;
}

} // namespace workers
} // namespace dom
} // namespace mozilla
