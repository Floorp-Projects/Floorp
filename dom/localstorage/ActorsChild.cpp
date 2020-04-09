/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ActorsChild.h"

#include "LocalStorageCommon.h"
#include "LSDatabase.h"
#include "LSObject.h"
#include "LSObserver.h"
#include "LSSnapshot.h"

namespace mozilla {
namespace dom {

/*******************************************************************************
 * LSDatabaseChild
 ******************************************************************************/

LSDatabaseChild::LSDatabaseChild(LSDatabase* aDatabase) : mDatabase(aDatabase) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabase);

  MOZ_COUNT_CTOR(LSDatabaseChild);
}

LSDatabaseChild::~LSDatabaseChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(LSDatabaseChild);
}

void LSDatabaseChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mDatabase) {
    mDatabase->ClearActor();
    mDatabase = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundLSDatabaseChild::SendDeleteMe());
  }
}

void LSDatabaseChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mDatabase) {
    mDatabase->ClearActor();
#ifdef DEBUG
    mDatabase = nullptr;
#endif
  }
}

mozilla::ipc::IPCResult LSDatabaseChild::RecvRequestAllowToClose() {
  AssertIsOnOwningThread();

  if (mDatabase) {
    mDatabase->RequestAllowToClose();

    // TODO: A new datastore will be prepared at first LocalStorage API
    //       synchronous call. It would be better to start preparing a new
    //       datastore right here, but asynchronously.
    //       However, we probably shouldn't do that if we are shutting down.
  }

  return IPC_OK();
}

PBackgroundLSSnapshotChild* LSDatabaseChild::AllocPBackgroundLSSnapshotChild(
    const nsString& aDocumentURI, const nsString& aKey,
    const bool& aIncreasePeakUsage, const int64_t& aRequestedSize,
    const int64_t& aMinSize, LSSnapshotInitInfo* aInitInfo) {
  MOZ_CRASH("PBackgroundLSSnapshotChild actor should be manually constructed!");
}

bool LSDatabaseChild::DeallocPBackgroundLSSnapshotChild(
    PBackgroundLSSnapshotChild* aActor) {
  MOZ_ASSERT(aActor);

  delete aActor;
  return true;
}

/*******************************************************************************
 * LSObserverChild
 ******************************************************************************/

LSObserverChild::LSObserverChild(LSObserver* aObserver) : mObserver(aObserver) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObserver);

  MOZ_COUNT_CTOR(LSObserverChild);
}

LSObserverChild::~LSObserverChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(LSObserverChild);
}

void LSObserverChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mObserver) {
    mObserver->ClearActor();
    mObserver = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundLSObserverChild::SendDeleteMe());
  }
}

void LSObserverChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mObserver) {
    mObserver->ClearActor();
#ifdef DEBUG
    mObserver = nullptr;
#endif
  }
}

mozilla::ipc::IPCResult LSObserverChild::RecvObserve(
    const PrincipalInfo& aPrincipalInfo, const uint32_t& aPrivateBrowsingId,
    const nsString& aDocumentURI, const nsString& aKey,
    const LSValue& aOldValue, const LSValue& aNewValue) {
  AssertIsOnOwningThread();

  if (!mObserver) {
    return IPC_OK();
  }

  nsresult rv;
  nsCOMPtr<nsIPrincipal> principal =
      PrincipalInfoToPrincipal(aPrincipalInfo, &rv);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return IPC_FAIL_NO_REASON(this);
  }

  Storage::NotifyChange(/* aStorage */ nullptr, principal, aKey,
                        aOldValue.AsString(), aNewValue.AsString(),
                        /* aStorageType */ kLocalStorageType, aDocumentURI,
                        /* aIsPrivate */ !!aPrivateBrowsingId,
                        /* aImmediateDispatch */ true);

  return IPC_OK();
}

/*******************************************************************************
 * LocalStorageRequestChild
 ******************************************************************************/

LSRequestChild::LSRequestChild() : mFinishing(false) {
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(LSRequestChild);
}

LSRequestChild::~LSRequestChild() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mCallback);

  MOZ_COUNT_DTOR(LSRequestChild);
}

void LSRequestChild::SetCallback(LSRequestChildCallback* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mCallback);
  MOZ_ASSERT(Manager());

  mCallback = aCallback;
}

bool LSRequestChild::Finishing() const {
  AssertIsOnOwningThread();

  return mFinishing;
}

void LSRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mCallback) {
    MOZ_ASSERT(aWhy != Deletion);

    mCallback->OnResponse(NS_ERROR_FAILURE);

    mCallback = nullptr;
  }
}

mozilla::ipc::IPCResult LSRequestChild::Recv__delete__(
    const LSRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCallback);

  mCallback->OnResponse(aResponse);

  mCallback = nullptr;

  return IPC_OK();
}

mozilla::ipc::IPCResult LSRequestChild::RecvReady() {
  AssertIsOnOwningThread();

  mFinishing = true;

  // We only expect this to return false if the channel has been closed, but
  // PBackground's channel never gets shutdown.
  MOZ_ALWAYS_TRUE(SendFinish());

  return IPC_OK();
}

/*******************************************************************************
 * LSSimpleRequestChild
 ******************************************************************************/

LSSimpleRequestChild::LSSimpleRequestChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_CTOR(LSSimpleRequestChild);
}

LSSimpleRequestChild::~LSSimpleRequestChild() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mCallback);

  MOZ_COUNT_DTOR(LSSimpleRequestChild);
}

void LSSimpleRequestChild::SetCallback(
    LSSimpleRequestChildCallback* aCallback) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aCallback);
  MOZ_ASSERT(!mCallback);
  MOZ_ASSERT(Manager());

  mCallback = aCallback;
}

void LSSimpleRequestChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mCallback) {
    MOZ_ASSERT(aWhy != Deletion);

    mCallback->OnResponse(NS_ERROR_FAILURE);

    mCallback = nullptr;
  }
}

mozilla::ipc::IPCResult LSSimpleRequestChild::Recv__delete__(
    const LSSimpleRequestResponse& aResponse) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mCallback);

  mCallback->OnResponse(aResponse);

  mCallback = nullptr;

  return IPC_OK();
}

/*******************************************************************************
 * LSSnapshotChild
 ******************************************************************************/

LSSnapshotChild::LSSnapshotChild(LSSnapshot* aSnapshot) : mSnapshot(aSnapshot) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aSnapshot);

  MOZ_COUNT_CTOR(LSSnapshotChild);
}

LSSnapshotChild::~LSSnapshotChild() {
  AssertIsOnOwningThread();

  MOZ_COUNT_DTOR(LSSnapshotChild);
}

void LSSnapshotChild::SendDeleteMeInternal() {
  AssertIsOnOwningThread();

  if (mSnapshot) {
    mSnapshot->ClearActor();
    mSnapshot = nullptr;

    MOZ_ALWAYS_TRUE(PBackgroundLSSnapshotChild::SendDeleteMe());
  }
}

void LSSnapshotChild::ActorDestroy(ActorDestroyReason aWhy) {
  AssertIsOnOwningThread();

  if (mSnapshot) {
    mSnapshot->ClearActor();
#ifdef DEBUG
    mSnapshot = nullptr;
#endif
  }
}

mozilla::ipc::IPCResult LSSnapshotChild::RecvMarkDirty() {
  AssertIsOnOwningThread();

  if (!mSnapshot) {
    return IPC_OK();
  }

  mSnapshot->MarkDirty();

  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
