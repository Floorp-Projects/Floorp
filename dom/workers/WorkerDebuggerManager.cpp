/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkerDebuggerManager.h"

#include "nsISimpleEnumerator.h"

#include "WorkerPrivate.h"

USING_WORKERS_NAMESPACE

class RegisterDebuggerRunnable MOZ_FINAL : public nsRunnable
{
  nsRefPtr<WorkerDebuggerManager> mManager;
  nsRefPtr<WorkerDebugger> mDebugger;
  bool mHasListeners;

public:
  RegisterDebuggerRunnable(WorkerDebuggerManager* aManager,
                           WorkerDebugger* aDebugger,
                           bool aHasListeners)
  : mManager(aManager), mDebugger(aDebugger), mHasListeners(aHasListeners)
  { }

  NS_DECL_THREADSAFE_ISUPPORTS

private:
  ~RegisterDebuggerRunnable()
  { }

  NS_IMETHOD
  Run() MOZ_OVERRIDE
  {
    mManager->RegisterDebuggerOnMainThread(mDebugger, mHasListeners);

    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(RegisterDebuggerRunnable, nsIRunnable);

BEGIN_WORKERS_NAMESPACE

class WorkerDebuggerEnumerator MOZ_FINAL : public nsISimpleEnumerator
{
  nsTArray<nsCOMPtr<nsISupports>> mDebuggers;
  uint32_t mIndex;

public:
  explicit WorkerDebuggerEnumerator(const nsTArray<WorkerDebugger*>& aDebuggers)
  : mIndex(0)
  {
    mDebuggers.AppendElements(aDebuggers);
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

  nsCOMPtr<nsISupports> element = mDebuggers.ElementAt(mIndex++);
  element.forget(aResult);
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

NS_IMPL_ISUPPORTS(WorkerDebuggerManager, nsIWorkerDebuggerManager);

NS_IMETHODIMP
WorkerDebuggerManager::GetWorkerDebuggerEnumerator(
                                                  nsISimpleEnumerator** aResult)
{
  AssertIsOnMainThread();

  MutexAutoLock lock(mMutex);

  nsRefPtr<WorkerDebuggerEnumerator> enumerator =
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

void
WorkerDebuggerManager::RegisterDebugger(WorkerDebugger* aDebugger)
{
  // May be called on any thread!

  bool hasListeners = false;

  {
    MutexAutoLock lock(mMutex);

    hasListeners = !mListeners.IsEmpty();
  }

  if (NS_IsMainThread()) {
    RegisterDebuggerOnMainThread(aDebugger, hasListeners);
  } else {
    nsCOMPtr<nsIRunnable> runnable =
      new RegisterDebuggerRunnable(this, aDebugger, hasListeners);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL)));

    if (hasListeners) {
      aDebugger->WaitIsEnabled(true);
    }
  }
}

void
WorkerDebuggerManager::UnregisterDebugger(WorkerDebugger* aDebugger)
{
  // May be called on any thread!

  if (NS_IsMainThread()) {
    UnregisterDebuggerOnMainThread(aDebugger);
  } else {
    nsCOMPtr<nsIRunnable> runnable =
      NS_NewRunnableMethodWithArg<nsRefPtr<WorkerDebugger>>(this,
        &WorkerDebuggerManager::UnregisterDebuggerOnMainThread, aDebugger);
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      NS_DispatchToMainThread(runnable, NS_DISPATCH_NORMAL)));

    aDebugger->WaitIsEnabled(false);
  }
}

void
WorkerDebuggerManager::RegisterDebuggerOnMainThread(WorkerDebugger* aDebugger,
                                                    bool aHasListeners)
{
  AssertIsOnMainThread();

  MOZ_ASSERT(!mDebuggers.Contains(aDebugger));
  mDebuggers.AppendElement(aDebugger);

  nsTArray<nsCOMPtr<nsIWorkerDebuggerManagerListener>> listeners;
  {
    MutexAutoLock lock(mMutex);

    listeners.AppendElements(mListeners);
  }

  if (aHasListeners) {
    for (size_t index = 0; index < listeners.Length(); ++index) {
      listeners[index]->OnRegister(aDebugger);
    }
  }

  aDebugger->Enable();
}

void
WorkerDebuggerManager::UnregisterDebuggerOnMainThread(WorkerDebugger* aDebugger)
{
  AssertIsOnMainThread();

  MOZ_ASSERT(mDebuggers.Contains(aDebugger));
  mDebuggers.RemoveElement(aDebugger);

  nsTArray<nsCOMPtr<nsIWorkerDebuggerManagerListener>> listeners;
  {
    MutexAutoLock lock(mMutex);

    listeners.AppendElements(mListeners);
  }

  for (size_t index = 0; index < listeners.Length(); ++index) {
    listeners[index]->OnUnregister(aDebugger);
  }

  aDebugger->Disable();
}

END_WORKERS_NAMESPACE
