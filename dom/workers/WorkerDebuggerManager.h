/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_workerdebuggermanager_h
#define mozilla_dom_workers_workerdebuggermanager_h

#include "Workers.h"

#include "nsIWorkerDebuggerManager.h"

#include "nsServiceManagerUtils.h"
#include "nsTArray.h"

#define WORKERDEBUGGERMANAGER_CID \
  { 0x62ec8731, 0x55ad, 0x4246, \
    { 0xb2, 0xea, 0xf2, 0x6c, 0x1f, 0xe1, 0x9d, 0x2d } }
#define WORKERDEBUGGERMANAGER_CONTRACTID \
  "@mozilla.org/dom/workers/workerdebuggermanager;1"

class RegisterDebuggerRunnable;

BEGIN_WORKERS_NAMESPACE

class WorkerDebugger;

class WorkerDebuggerManager final : public nsIWorkerDebuggerManager
{
  friend class ::RegisterDebuggerRunnable;

  mozilla::Mutex mMutex;

  // Protected by mMutex.
  nsTArray<nsCOMPtr<nsIWorkerDebuggerManagerListener>> mListeners;

  // Only touched on the main thread.
  nsTArray<WorkerDebugger*> mDebuggers;

public:
  static WorkerDebuggerManager*
  GetOrCreateService()
  {
    nsCOMPtr<nsIWorkerDebuggerManager> manager =
      do_GetService(WORKERDEBUGGERMANAGER_CONTRACTID);
    return static_cast<WorkerDebuggerManager*>(manager.get());
  }

  WorkerDebuggerManager();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWORKERDEBUGGERMANAGER

  void ClearListeners();

  void RegisterDebugger(WorkerDebugger* aDebugger);

  void UnregisterDebugger(WorkerDebugger* aDebugger);

private:
  virtual ~WorkerDebuggerManager();

  void RegisterDebuggerOnMainThread(WorkerDebugger* aDebugger,
                                    bool aHasListeners);

  void UnregisterDebuggerOnMainThread(WorkerDebugger* aDebugger);
};

inline nsresult
ClearWorkerDebuggerManagerListeners()
{
  nsRefPtr<WorkerDebuggerManager> manager =
    WorkerDebuggerManager::GetOrCreateService();
  if (!manager) {
    return NS_ERROR_FAILURE;
  }

  manager->ClearListeners();
  return NS_OK;
}

inline nsresult
RegisterWorkerDebugger(WorkerDebugger* aDebugger)
{
  nsRefPtr<WorkerDebuggerManager> manager =
    WorkerDebuggerManager::GetOrCreateService();
  if (!manager) {
    return NS_ERROR_FAILURE;
  }

  manager->RegisterDebugger(aDebugger);
  return NS_OK;
}

inline nsresult
UnregisterWorkerDebugger(WorkerDebugger* aDebugger)
{
  nsRefPtr<WorkerDebuggerManager> manager =
    WorkerDebuggerManager::GetOrCreateService();
  if (!manager) {
    return NS_ERROR_FAILURE;
  }

  manager->UnregisterDebugger(aDebugger);
  return NS_OK;
}

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_workerdebuggermanager_h
