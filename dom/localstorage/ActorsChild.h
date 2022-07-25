/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_localstorage_ActorsChild_h
#define mozilla_dom_localstorage_ActorsChild_h

#include <cstdint>
#include "mozilla/RefPtr.h"
#include "mozilla/dom/PBackgroundLSDatabaseChild.h"
#include "mozilla/dom/PBackgroundLSObserverChild.h"
#include "mozilla/dom/PBackgroundLSRequest.h"
#include "mozilla/dom/PBackgroundLSRequestChild.h"
#include "mozilla/dom/PBackgroundLSSimpleRequest.h"
#include "mozilla/dom/PBackgroundLSSimpleRequestChild.h"
#include "mozilla/dom/PBackgroundLSSnapshotChild.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "nsISupports.h"
#include "nsStringFwd.h"
#include "nscore.h"

namespace mozilla {

namespace ipc {

class BackgroundChildImpl;

}  // namespace ipc

namespace dom {

class LocalStorageManager2;
class LSDatabase;
class LSObject;
class LSObserver;
class LSRequestChildCallback;
class LSSimpleRequestChildCallback;
class LSSnapshot;

/**
 * Minimal glue actor with standard IPC-managed new/delete existence that exists
 * primarily to track the continued existence of the LSDatabase in the child.
 * Most of the interesting bits happen via PBackgroundLSSnapshot.
 *
 * Mutual raw pointers are maintained between LSDatabase and this class that are
 * cleared at either (expected) when the child starts the deletion process
 * (SendDeleteMeInternal) or unexpected actor death (ActorDestroy).
 *
 * See `PBackgroundLSDatabase.ipdl` for more information.
 *
 *
 * ## Low-Level Lifecycle ##
 * - Created by LSObject::EnsureDatabase if it had to create a database.
 * - Deletion begun by LSDatabase's destructor invoking SendDeleteMeInternal
 *   which will result in the parent sending __delete__ which destroys the
 *   actor.
 */
class LSDatabaseChild final : public PBackgroundLSDatabaseChild {
  friend class mozilla::ipc::BackgroundChildImpl;
  friend class LSDatabase;
  friend class LSObject;

  LSDatabase* mDatabase;

  NS_DECL_OWNINGTHREAD

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(LSDatabaseChild);
  }

 private:
  // Only created by LSObject.
  explicit LSDatabaseChild(LSDatabase* aDatabase);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSDatabaseChild();

  void SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvRequestAllowToClose() override;

  PBackgroundLSSnapshotChild* AllocPBackgroundLSSnapshotChild(
      const nsAString& aDocumentURI, const nsAString& aKey,
      const bool& aIncreasePeakUsage, const int64_t& aMinSize,
      LSSnapshotInitInfo* aInitInfo) override;

  bool DeallocPBackgroundLSSnapshotChild(
      PBackgroundLSSnapshotChild* aActor) override;
};

/**
 * Minimal IPC-managed (new/delete) actor that exists to receive and relay
 * "storage" events from changes to LocalStorage that take place in other
 * processes as their Snapshots are checkpointed to the canonical Datastore in
 * the parent process.
 *
 * See `PBackgroundLSObserver.ipdl` for more info.
 */
class LSObserverChild final : public PBackgroundLSObserverChild {
  friend class mozilla::ipc::BackgroundChildImpl;
  friend class LSObserver;
  friend class LSObject;

  LSObserver* mObserver;

  NS_DECL_OWNINGTHREAD

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(LSObserverChild);
  }

 private:
  // Only created by LSObject.
  explicit LSObserverChild(LSObserver* aObserver);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSObserverChild();

  void SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvObserve(const PrincipalInfo& aPrinciplaInfo,
                                      const uint32_t& aPrivateBrowsingId,
                                      const nsAString& aDocumentURI,
                                      const nsAString& aKey,
                                      const LSValue& aOldValue,
                                      const LSValue& aNewValue) override;
};

/**
 * Minimal glue IPC-managed (new/delete) actor that is used by LSObject and its
 * RequestHelper to perform synchronous requests on top of an asynchronous
 * protocol.
 *
 * Takes an `LSReuestChildCallback` to be invoked when a response is received
 * via __delete__.
 *
 * See `PBackgroundLSRequest.ipdl`, `LSObject`, and `RequestHelper` for more
 * info.
 */
class LSRequestChild final : public PBackgroundLSRequestChild {
  friend class LSObject;
  friend class LocalStorageManager2;

  RefPtr<LSRequestChildCallback> mCallback;

  bool mFinishing;

  NS_DECL_OWNINGTHREAD

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(LSReqeustChild);
  }

  bool Finishing() const;

 private:
  // Only created by LSObject.
  LSRequestChild();

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSRequestChild();

  void SetCallback(LSRequestChildCallback* aCallback);

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult Recv__delete__(
      const LSRequestResponse& aResponse) override;

  mozilla::ipc::IPCResult RecvReady() override;
};

class NS_NO_VTABLE LSRequestChildCallback {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void OnResponse(const LSRequestResponse& aResponse) = 0;

 protected:
  virtual ~LSRequestChildCallback() = default;
};

/**
 * Minimal glue IPC-managed (new/delete) actor used by `LocalStorageManager2` to
 * issue asynchronous requests in an asynchronous fashion.
 *
 * Takes an `LSSimpleRequestChildCallback` to be invoked when a response is
 * received via __delete__.
 *
 * See `PBackgroundLSSimpleRequest.ipdl` for more info.
 */
class LSSimpleRequestChild final : public PBackgroundLSSimpleRequestChild {
  friend class LocalStorageManager2;

  RefPtr<LSSimpleRequestChildCallback> mCallback;

  NS_DECL_OWNINGTHREAD

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(LSSimpleReqeustChild);
  }

 private:
  // Only created by LocalStorageManager2.
  LSSimpleRequestChild();

  void SetCallback(LSSimpleRequestChildCallback* aCallback);

  // Only destroyed by mozilla::ipc::BackgroundChildImpl.
  ~LSSimpleRequestChild();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult Recv__delete__(
      const LSSimpleRequestResponse& aResponse) override;
};

class NS_NO_VTABLE LSSimpleRequestChildCallback {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  virtual void OnResponse(const LSSimpleRequestResponse& aResponse) = 0;

 protected:
  virtual ~LSSimpleRequestChildCallback() = default;
};

/**
 * Minimal IPC-managed (new/delete) actor that lasts as long as its owning
 * LSSnapshot.
 *
 * Mutual raw pointers are maintained between LSSnapshot and this class that are
 * cleared at either (expected) when the child starts the deletion process
 * (SendDeleteMeInternal) or unexpected actor death (ActorDestroy).
 *
 * See `PBackgroundLSSnapshot.ipdl` and `LSSnapshot` for more info.
 */
class LSSnapshotChild final : public PBackgroundLSSnapshotChild {
  friend class LSDatabase;
  friend class LSSnapshot;

  LSSnapshot* mSnapshot;

  NS_DECL_OWNINGTHREAD

 public:
  void AssertIsOnOwningThread() const {
    NS_ASSERT_OWNINGTHREAD(LSSnapshotChild);
  }

 private:
  // Only created by LSDatabase.
  explicit LSSnapshotChild(LSSnapshot* aSnapshot);

  // Only destroyed by LSDatabaseChild.
  ~LSSnapshotChild();

  void SendDeleteMeInternal();

  // IPDL methods are only called by IPDL.
  void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvMarkDirty() override;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_localstorage_ActorsChild_h
