/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedWorkerManager_h
#define mozilla_dom_SharedWorkerManager_h

#include "mozilla/dom/RemoteWorkerController.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class MessagePortIdentifier;
class RemoteWorkerData;
class SharedWorkerManager;
class SharedWorkerService;
class SharedWorkerParent;

// Main-thread only object that keeps a manager and the service alive.
// When the last SharedWorkerManagerHolder is released, the corresponding
// manager unregisters itself from the service and terminates the worker.
class SharedWorkerManagerHolder final {
 public:
  NS_INLINE_DECL_REFCOUNTING(SharedWorkerManagerHolder);

  SharedWorkerManagerHolder(SharedWorkerManager* aManager,
                            SharedWorkerService* aService);

  SharedWorkerManager* Manager() const { return mManager; }

  SharedWorkerService* Service() const { return mService; }

 private:
  ~SharedWorkerManagerHolder();

  RefPtr<SharedWorkerManager> mManager;
  RefPtr<SharedWorkerService> mService;
};

// Thread-safe wrapper for SharedWorkerManagerHolder.
class SharedWorkerManagerWrapper final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedWorkerManagerWrapper);

  explicit SharedWorkerManagerWrapper(
      already_AddRefed<SharedWorkerManagerHolder> aHolder);

  SharedWorkerManager* Manager() const { return mHolder->Manager(); }

 private:
  ~SharedWorkerManagerWrapper();

  RefPtr<SharedWorkerManagerHolder> mHolder;
};

class SharedWorkerManager final : public RemoteWorkerObserver {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedWorkerManager, override);

  // Called on main-thread thread methods

  static already_AddRefed<SharedWorkerManagerHolder> Create(
      SharedWorkerService* aService, nsIEventTarget* aPBackgroundEventTarget,
      const RemoteWorkerData& aData, nsIPrincipal* aLoadingPrincipal,
      const OriginAttributes& aStoragePrincipalAttrs);

  // Returns a holder if this manager matches. The holder blocks the shutdown of
  // the manager.
  already_AddRefed<SharedWorkerManagerHolder> MatchOnMainThread(
      SharedWorkerService* aService, const nsACString& aDomain,
      nsIURI* aScriptURL, const nsAString& aName,
      nsIPrincipal* aLoadingPrincipal,
      const OriginAttributes& aStoragePrincipalAttrs);

  // RemoteWorkerObserver

  void CreationFailed() override;

  void CreationSucceeded() override;

  void ErrorReceived(const ErrorValue& aValue) override;

  void Terminated() override;

  // Called on PBackground thread methods

  bool MaybeCreateRemoteWorker(const RemoteWorkerData& aData,
                               uint64_t aWindowID,
                               const MessagePortIdentifier& aPortIdentifier,
                               base::ProcessId aProcessId);

  void AddActor(SharedWorkerParent* aParent);

  void RemoveActor(SharedWorkerParent* aParent);

  void UpdateSuspend();

  void UpdateFrozen();

  bool IsSecureContext() const;

  void Terminate();

  // Called on main-thread only.

  void RegisterHolder(SharedWorkerManagerHolder* aHolder);

  void UnregisterHolder(SharedWorkerManagerHolder* aHolder);

 private:
  SharedWorkerManager(nsIEventTarget* aPBackgroundEventTarget,
                      const RemoteWorkerData& aData,
                      nsIPrincipal* aLoadingPrincipal,
                      const OriginAttributes& aStoragePrincipalAttrs);

  ~SharedWorkerManager();

  nsCOMPtr<nsIEventTarget> mPBackgroundEventTarget;

  nsCOMPtr<nsIPrincipal> mLoadingPrincipal;
  nsCString mDomain;
  OriginAttributes mStoragePrincipalAttrs;
  nsCOMPtr<nsIURI> mResolvedScriptURL;
  nsString mName;
  bool mIsSecureContext;
  bool mSuspended;
  bool mFrozen;

  // Raw pointers because SharedWorkerParent unregisters itself in
  // ActorDestroy().
  nsTArray<SharedWorkerParent*> mActors;

  RefPtr<RemoteWorkerController> mRemoteWorkerController;

  // Main-thread only. Raw Pointers because holders keep the manager alive and
  // they unregister themselves in their DTOR.
  nsTArray<SharedWorkerManagerHolder*> mHolders;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SharedWorkerManager_h
