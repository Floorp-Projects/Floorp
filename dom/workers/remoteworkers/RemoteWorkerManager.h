/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerManager_h
#define mozilla_dom_RemoteWorkerManager_h

#include "base/process.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {

class RemoteWorkerController;
class RemoteWorkerServiceParent;

// This class is used on PBackground thread, on the parent process only.
class RemoteWorkerManager final {
 public:
  NS_INLINE_DECL_REFCOUNTING(RemoteWorkerManager)

  static already_AddRefed<RemoteWorkerManager> GetOrCreate();

  void RegisterActor(RemoteWorkerServiceParent* aActor);

  void UnregisterActor(RemoteWorkerServiceParent* aActor);

  void Launch(RemoteWorkerController* aController,
              const RemoteWorkerData& aData, base::ProcessId aProcessId);

 private:
  RemoteWorkerManager();
  ~RemoteWorkerManager();

  RemoteWorkerServiceParent* SelectTargetActor(const RemoteWorkerData& aData,
                                               base::ProcessId aProcessId);

  RemoteWorkerServiceParent* SelectTargetActorForServiceWorker() const;

  void LaunchInternal(RemoteWorkerController* aController,
                      RemoteWorkerServiceParent* aTargetActor,
                      const RemoteWorkerData& aData,
                      bool aRemoteWorkerAlreadyRegistered = false);

  void LaunchNewContentProcess();

  void AsyncCreationFailed(RemoteWorkerController* aController);

  // The list of existing RemoteWorkerServiceParent actors for child processes.
  // Raw pointers because RemoteWorkerServiceParent actors unregister themselves
  // when destroyed.
  // XXX For Fission, where we could have a lot of child actors, should we maybe
  // instead keep either a hash table (PID->actor) or perhaps store the actors
  // in order, sorted by PID, to avoid linear lookup times?
  nsTArray<RemoteWorkerServiceParent*> mChildActors;
  RemoteWorkerServiceParent* mParentActor;

  struct Pending {
    RefPtr<RemoteWorkerController> mController;
    RemoteWorkerData mData;
  };

  nsTArray<Pending> mPendings;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_RemoteWorkerManager_h
