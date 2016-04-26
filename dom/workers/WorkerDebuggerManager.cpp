/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerDebuggerManager.h"

#include "nsISimpleEnumerator.h"

#include "mozilla/ClearOnShutdown.h"

#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE

namespace {

class RegisterDebuggerMainThreadRunnable final : public mozilla::Runnable
{
  WorkerPrivate* mWorkerPrivate;
  bool mNotifyListeners;

public:
  RegisterDebuggerMainThreadRunnable(WorkerPrivate* aWorkerPrivate,
                                     bool aNotifyListeners)
  : mWorkerPrivate(aWorkerPrivate),
    mNotifyListeners(aNotifyListeners)
  { }

private:
  ~RegisterDebuggerMainThreadRunnable()
  { }

  NS_IMETHOD
  Run() override
  {
    WorkerDebuggerManager* manager = WorkerDebuggerManager::Get();
    MOZ_ASSERT(manager);

    manager->RegisterDebuggerMainThread(mWorkerPrivate, mNotifyListeners);
    return NS_OK;
  }
};

class UnregisterDebuggerMainThreadRunnable final : public mozilla::Runnable
{
  WorkerPrivate* mWorkerPrivate;

public:
  explicit UnregisterDebuggerMainThreadRunnable(WorkerPrivate* aWorkerPrivate)
  : mWorkerPrivate(aWorkerPrivate)
  { }

private:
  ~UnregisterDebuggerMainThreadRunnable()
  { }

  NS_IMETHOD
  Run() override
  {
    WorkerDebuggerManager* manager = WorkerDebuggerManager::Get();
    MOZ_ASSERT(manager);

    manager->UnregisterDebuggerMainThread(mWorkerPrivate);
    return NS_OK;
  }
};

// Does not hold an owning reference.
static WorkerDebuggerManager* gWorkerDebuggerManager;

} /* anonymous namespace */

BEGIN_WORKERS_NAMESPACE

class WorkerDebuggerEnumerator final : public nsISimpleEnumerator
{
  nsTArray<RefPtr<WorkerDebugger>> mDebuggers;
  uint32_t mIndex;

public:
  explicit WorkerDebuggerEnumerator(
                             const nsTArray<RefPtr<WorkerDebugger>>& aDebuggers)
  : mDebuggers(aDebuggers), mIndex(0)
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSISIMPLEENUMERATOR

private:
  ~WorkerDebuggerEnumerator() {}
};

NS_IMPL_ISUPPORTS(WorkerDebuggerEnumerator, nsISimpleEnumerator);

NS_IMETHODIMP
WorkerDebuggerEnumerator::HasMoreElements(bool* aResult)
{
  *aResult = mIndex < mDebuggers.Length();
  return NS_OK;
};

NS_IMETHODIMP
WorkerDebuggerEnumerator::GetNext(nsISupports** aResult)
{
  if (mIndex == mDebuggers.Length()) {
    return NS_ERROR_FAILURE;
  }

  mDebuggers.ElementAt(mIndex++).forget(aResult);
  return NS_OK;
};

WorkerDebuggerManager::WorkerDebuggerManager()
: mMutex("WorkerDebuggerManager::mMutex")
{
  AssertIsOnMainThread();
}

WorkerDebuggerManager::~WorkerDebuggerManager()
{
  AssertIsOnMainThread();
}

// static
already_AddRefed<WorkerDebuggerManager>
WorkerDebuggerManager::GetInstance()
{
  RefPtr<WorkerDebuggerManager> manager = WorkerDebuggerManager::GetOrCreate();
  return manager.forget();
}

// static
WorkerDebuggerManager*
WorkerDebuggerManager::GetOrCreate()
{
  AssertIsOnMainThread();

  if (!gWorkerDebuggerManager) {
    // The observer service now owns us until shutdown.
    gWorkerDebuggerManager = new WorkerDebuggerManager();
    if (NS_FAILED(gWorkerDebuggerManager->Init())) {
      NS_WARNING("Failed to initialize worker debugger manager!");
      gWorkerDebuggerManager = nullptr;
      return nullptr;
    }
  }

  return gWorkerDebuggerManager;
}

WorkerDebuggerManager*
WorkerDebuggerManager::Get()
{
  MOZ_ASSERT(gWorkerDebuggerManager);
  return gWorkerDebuggerManager;
}

NS_IMPL_ISUPPORTS(WorkerDebuggerManager, nsIObserver, nsIWorkerDebuggerManager);

NS_IMETHODIMP
WorkerDebuggerManager::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)) {
    Shutdown();
    return NS_OK;
  }

  NS_NOTREACHED("Unknown observer topic!");
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebuggerManager::GetWorkerDebuggerEnumerator(
                                                  nsISimpleEnumerator** aResult)
{
  AssertIsOnMainThread();

  RefPtr<WorkerDebuggerEnumerator> enumerator =
    new WorkerDebuggerEnumerator(mDebuggers);
  enumerator.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebuggerManager::AddListener(nsIWorkerDebuggerManagerListener* aListener)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (mListeners.Contains(aListener)) {
    return NS_ERROR_INVALID_ARG;
  }

  mListeners.AppendElement(aListener);
  return NS_OK;
}

NS_IMETHODIMP
WorkerDebuggerManager::RemoveListener(
                                    nsIWorkerDebuggerManagerListener* aListener)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  if (!mListeners.Contains(aListener)) {
    return NS_OK;
  }

  mListeners.RemoveElement(aListener);
  return NS_OK;
}

nsresult
WorkerDebuggerManager::Init()
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  nsresult rv = obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void
WorkerDebuggerManager::Shutdown()
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  mListeners.Clear();
}

void
WorkerDebuggerManager::RegisterDebugger(WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnParentThread();

  if (NS_IsMainThread()) {
    // When the parent thread is the main thread, it will always block until all
    // register liseners have been called, since it cannot continue until the
    // call to RegisterDebuggerMainThread returns.
    //
    // In this case, it is always safe to notify all listeners on the main
    // thread, even if there were no listeners at the time this method was
    // called, so we can always pass true for the value of aNotifyListeners.
    // This avoids having to lock mMutex to check whether mListeners is empty.
    RegisterDebuggerMainThread(aWorkerPrivate, true);
  } else {
    // We guarantee that if any register listeners are called, the worker does
    // not start running until all register listeners have been called. To
    // guarantee this, the parent thread should block until all register
    // listeners have been called.
    //
    // However, to avoid overhead when the debugger is not being used, the
    // parent thread will only block if there were any listeners at the time
    // this method was called. As a result, we should not notify any listeners
    // on the main thread if there were no listeners at the time this method was
    // called, because the parent will not be blocking in that case.
    bool hasListeners = false;
    {
      MutexAutoLock lock(mMutex);

      hasListeners = !mListeners.IsEmpty();
    }

    nsCOMPtr<nsIRunnable> runnable =
      new RegisterDebuggerMainThreadRunnable(aWorkerPrivate, hasListeners);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL));

    if (hasListeners) {
      aWorkerPrivate->WaitForIsDebuggerRegistered(true);
    }
  }
}

void
WorkerDebuggerManager::UnregisterDebugger(WorkerPrivate* aWorkerPrivate)
{
  aWorkerPrivate->AssertIsOnParentThread();

  if (NS_IsMainThread()) {
    UnregisterDebuggerMainThread(aWorkerPrivate);
  } else {
    nsCOMPtr<nsIRunnable> runnable =
      new UnregisterDebuggerMainThreadRunnable(aWorkerPrivate);
    MOZ_ALWAYS_SUCCEEDS(NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL));

    aWorkerPrivate->WaitForIsDebuggerRegistered(false);
  }
}

void
WorkerDebuggerManager::RegisterDebuggerMainThread(WorkerPrivate* aWorkerPrivate,
                                                  bool aNotifyListeners)
{
  AssertIsOnMainThread();

  RefPtr<WorkerDebugger> debugger = new WorkerDebugger(aWorkerPrivate);
  mDebuggers.AppendElement(debugger);

  aWorkerPrivate->SetDebugger(debugger);

  if (aNotifyListeners) {
    nsTArray<nsCOMPtr<nsIWorkerDebuggerManagerListener>> listeners;
    {
      MutexAutoLock lock(mMutex);

      listeners = mListeners;
    }

    for (size_t index = 0; index < listeners.Length(); ++index) {
      listeners[index]->OnRegister(debugger);
    }
  }

  aWorkerPrivate->SetIsDebuggerRegistered(true);
}

void
WorkerDebuggerManager::UnregisterDebuggerMainThread(
                                                  WorkerPrivate* aWorkerPrivate)
{
  AssertIsOnMainThread();

  // There is nothing to do here if the debugger was never succesfully
  // registered. We need to check this on the main thread because the worker
  // does not wait for the registration to complete if there were no listeners
  // installed when it started.
  if (!aWorkerPrivate->IsDebuggerRegistered()) {
    return;
  }

  RefPtr<WorkerDebugger> debugger = aWorkerPrivate->Debugger();
  mDebuggers.RemoveElement(debugger);

  aWorkerPrivate->SetDebugger(nullptr);

  nsTArray<nsCOMPtr<nsIWorkerDebuggerManagerListener>> listeners;
  {
    MutexAutoLock lock(mMutex);

    listeners = mListeners;
  }

  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnUnregister(debugger);
  }

  debugger->Close();
  aWorkerPrivate->SetIsDebuggerRegistered(false);
}

END_WORKERS_NAMESPACE
