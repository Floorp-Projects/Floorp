/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerService_h
#define mozilla_dom_RemoteWorkerService_h

#include "nsCOMPtr.h"
#include "nsIObserver.h"

class nsIThread;

namespace mozilla {
namespace dom {

class RemoteWorkerServiceChild;

/**
 * Every process has a RemoteWorkerService which does the actual spawning of
 * RemoteWorkerChild instances. The RemoteWorkerService creates a "Worker
 * Launcher" thread at initialization on which it creates a
 * RemoteWorkerServiceChild to service spawn requests. The thread is exposed as
 * RemoteWorkerService::Thread(). A new/distinct thread is used because we
 * (eventually) don't want to deal with main-thread contention, content
 * processes have no equivalent of a PBackground thread, and actors are bound to
 * specific threads.
 *
 * (Disclaimer: currently most RemoteWorkerOps need to happen on the main thread
 * because the main-thread ends up as the owner of the worker and all
 * manipulation of the worker must happen from the owning thread.)
 */
class RemoteWorkerService final : public nsIObserver {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOBSERVER

  // To be called when a process is initialized on main-thread.
  static void Initialize();

  static nsIThread* Thread();

 private:
  RemoteWorkerService();
  ~RemoteWorkerService();

  nsresult InitializeOnMainThread();

  void InitializeOnTargetThread();

  void ShutdownOnTargetThread();

  nsCOMPtr<nsIThread> mThread;
  RefPtr<RemoteWorkerServiceChild> mActor;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWorkerService_h
