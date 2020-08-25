/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_RemoteWorkerManager_h
#define mozilla_dom_RemoteWorkerManager_h

#include "base/process.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/RemoteWorkerTypes.h"
#include "mozilla/dom/WorkerPrivate.h"  // WorkerType enum
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

  static bool MatchRemoteType(const nsACString& processRemoteType,
                              const nsACString& workerRemoteType);

  /**
   * Get the child process RemoteType where a RemoteWorker should be
   * launched.
   */
  static Result<nsCString, nsresult> GetRemoteType(
      const nsCOMPtr<nsIPrincipal>& aPrincipal, WorkerType aWorkerType);

  /**
   * Verify if a remote worker should be allowed to run in the current
   * child process remoteType.
   */
  static bool IsRemoteTypeAllowed(const RemoteWorkerData& aData);

 private:
  RemoteWorkerManager();
  ~RemoteWorkerManager();

  RemoteWorkerServiceParent* SelectTargetActor(const RemoteWorkerData& aData,
                                               base::ProcessId aProcessId);

  RemoteWorkerServiceParent* SelectTargetActorForServiceWorker(
      const RemoteWorkerData& aData) const;

  RemoteWorkerServiceParent* SelectTargetActorForSharedWorker(
      base::ProcessId aProcessId, const RemoteWorkerData& aData) const;

  void LaunchInternal(RemoteWorkerController* aController,
                      RemoteWorkerServiceParent* aTargetActor,
                      const RemoteWorkerData& aData,
                      bool aRemoteWorkerAlreadyRegistered = false);

  void LaunchNewContentProcess(const RemoteWorkerData& aData);

  void AsyncCreationFailed(RemoteWorkerController* aController);

  static nsCString GetRemoteTypeForActor(
      const RemoteWorkerServiceParent* aActor);

  // Iterate through all RemoteWorkerServiceParent actors, starting from a
  // random index (as if iterating through a circular array).
  //
  // aCallback should be a invokable object with a function signature of
  //   bool (RemoteWorkerServiceParent*, RefPtr<ContentParent>&&)
  //
  // aCallback is called with the actor and corresponding ContentParent, should
  // return false to abort iteration before all actors have been traversed (e.g.
  // if the desired actor is found), and must not mutate mChildActors (which
  // shouldn't be an issue because this function is const). aCallback also
  // doesn't need to worry about proxy-releasing the ContentParent if it isn't
  // moved out of the parameter.
  template <typename Callback>
  void ForEachActor(Callback&& aCallback) const;

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
