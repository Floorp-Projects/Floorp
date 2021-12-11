/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SharedWorkerService_h
#define mozilla_dom_SharedWorkerService_h

#include "mozilla/dom/quota/CheckedUnsafePtr.h"
#include "nsISupportsImpl.h"
#include "nsTArray.h"

class nsIEventTarget;

namespace mozilla {

namespace ipc {
class PrincipalInfo;
}

namespace dom {

class MessagePortIdentifier;
class RemoteWorkerData;
class SharedWorkerManager;
class SharedWorkerParent;
class UniqueMessagePortId;

/**
 * PBackground service that creates and tracks the per-worker
 * SharedWorkerManager instances, allowing rendezvous between SharedWorkerParent
 * instances and the SharedWorkerManagers they want to talk to (1:1).
 */
class SharedWorkerService final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedWorkerService);

  // This can be called on PBackground thread only.
  static already_AddRefed<SharedWorkerService> GetOrCreate();

  // The service, if already created, is available on any thread using this
  // method.
  static SharedWorkerService* Get();

  // PBackground method only.
  void GetOrCreateWorkerManager(SharedWorkerParent* aActor,
                                const RemoteWorkerData& aData,
                                uint64_t aWindowID,
                                const MessagePortIdentifier& aPortIdentifier);

  void GetOrCreateWorkerManagerOnMainThread(
      nsIEventTarget* aBackgroundEventTarget, SharedWorkerParent* aActor,
      const RemoteWorkerData& aData, uint64_t aWindowID,
      UniqueMessagePortId& aPortIdentifier);

  void RemoveWorkerManagerOnMainThread(SharedWorkerManager* aManager);

 private:
  SharedWorkerService() = default;
  ~SharedWorkerService() = default;

  void ErrorPropagationOnMainThread(nsIEventTarget* aBackgroundEventTarget,
                                    SharedWorkerParent* aActor,
                                    nsresult aError);

  // Touched on main-thread only.
  nsTArray<RefPtr<SharedWorkerManager>> mWorkerManagers;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SharedWorkerService_h
