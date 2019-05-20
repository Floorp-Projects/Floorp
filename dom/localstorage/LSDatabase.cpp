/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSDatabase.h"

namespace mozilla {
namespace dom {

using namespace mozilla::services;

namespace {

#define XPCOM_SHUTDOWN_OBSERVER_TOPIC "xpcom-shutdown"

typedef nsDataHashtable<nsCStringHashKey, LSDatabase*> LSDatabaseHashtable;

StaticAutoPtr<LSDatabaseHashtable> gLSDatabases;

}  // namespace

StaticRefPtr<LSDatabase::Observer> LSDatabase::sObserver;

class LSDatabase::Observer final : public nsIObserver {
  bool mInvalidated;

 public:
  Observer() : mInvalidated(false) { MOZ_ASSERT(NS_IsMainThread()); }

  void Invalidate() { mInvalidated = true; }

 private:
  ~Observer() { MOZ_ASSERT(NS_IsMainThread()); }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

LSDatabase::LSDatabase(const nsACString& aOrigin)
    : mActor(nullptr),
      mSnapshot(nullptr),
      mOrigin(aOrigin),
      mAllowedToClose(false),
      mRequestedAllowToClose(false) {
  AssertIsOnOwningThread();

  if (!gLSDatabases) {
    gLSDatabases = new LSDatabaseHashtable();

    MOZ_ASSERT(!sObserver);

    sObserver = new Observer();

    nsCOMPtr<nsIObserverService> obsSvc = GetObserverService();
    MOZ_ASSERT(obsSvc);

    MOZ_ALWAYS_SUCCEEDS(
        obsSvc->AddObserver(sObserver, XPCOM_SHUTDOWN_OBSERVER_TOPIC, false));
  }

  MOZ_ASSERT(!gLSDatabases->Get(mOrigin));
  gLSDatabases->Put(mOrigin, this);
}

LSDatabase::~LSDatabase() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mSnapshot);

  if (!mAllowedToClose) {
    AllowToClose();
  }

  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }
}

// static
LSDatabase* LSDatabase::Get(const nsACString& aOrigin) {
  return gLSDatabases ? gLSDatabases->Get(aOrigin) : nullptr;
}

void LSDatabase::SetActor(LSDatabaseChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

void LSDatabase::RequestAllowToClose() {
  AssertIsOnOwningThread();

  if (mRequestedAllowToClose) {
    return;
  }

  mRequestedAllowToClose = true;

  if (mSnapshot) {
    mSnapshot->MarkDirty();
  } else {
    AllowToClose();
  }
}

void LSDatabase::NoteFinishedSnapshot(LSSnapshot* aSnapshot) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aSnapshot == mSnapshot);

  mSnapshot = nullptr;

  if (mRequestedAllowToClose) {
    AllowToClose();
  }
}

nsresult LSDatabase::GetLength(LSObject* aObject, uint32_t* aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsresult rv = EnsureSnapshot(aObject, VoidString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mSnapshot->GetLength(aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::GetKey(LSObject* aObject, uint32_t aIndex,
                            nsAString& aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsresult rv = EnsureSnapshot(aObject, VoidString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mSnapshot->GetKey(aIndex, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::GetItem(LSObject* aObject, const nsAString& aKey,
                             nsAString& aResult) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsresult rv = EnsureSnapshot(aObject, aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mSnapshot->GetItem(aKey, aResult);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::GetKeys(LSObject* aObject, nsTArray<nsString>& aKeys) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsresult rv = EnsureSnapshot(aObject, VoidString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mSnapshot->GetKeys(aKeys);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::SetItem(LSObject* aObject, const nsAString& aKey,
                             const nsAString& aValue,
                             LSNotifyInfo& aNotifyInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsresult rv = EnsureSnapshot(aObject, aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mSnapshot->SetItem(aKey, aValue, aNotifyInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::RemoveItem(LSObject* aObject, const nsAString& aKey,
                                LSNotifyInfo& aNotifyInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsresult rv = EnsureSnapshot(aObject, aKey);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mSnapshot->RemoveItem(aKey, aNotifyInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::Clear(LSObject* aObject, LSNotifyInfo& aNotifyInfo) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  nsresult rv = EnsureSnapshot(aObject, VoidString());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mSnapshot->Clear(aNotifyInfo);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::BeginExplicitSnapshot(LSObject* aObject) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  if (mSnapshot) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsresult rv = EnsureSnapshot(aObject, VoidString(), /* aExplicit */ true);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::EndExplicitSnapshot(LSObject* aObject) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT(!mAllowedToClose);

  if (!mSnapshot) {
    return NS_ERROR_NOT_INITIALIZED;
  }

  MOZ_ASSERT(mSnapshot->Explicit());

  nsresult rv = mSnapshot->End();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult LSDatabase::EnsureSnapshot(LSObject* aObject, const nsAString& aKey,
                                    bool aExplicit) {
  MOZ_ASSERT(aObject);
  MOZ_ASSERT(mActor);
  MOZ_ASSERT_IF(mSnapshot, !aExplicit);
  MOZ_ASSERT(!mAllowedToClose);

  if (mSnapshot) {
    return NS_OK;
  }

  RefPtr<LSSnapshot> snapshot = new LSSnapshot(this);

  LSSnapshotChild* actor = new LSSnapshotChild(snapshot);

  LSSnapshotInitInfo initInfo;
  bool ok = mActor->SendPBackgroundLSSnapshotConstructor(
      actor, aObject->DocumentURI(), nsString(aKey),
      /* increasePeakUsage */ true,
      /* requestedSize */ 131072,
      /* minSize */ 4096, &initInfo);
  if (NS_WARN_IF(!ok)) {
    return NS_ERROR_FAILURE;
  }

  snapshot->SetActor(actor);

  // This add refs snapshot.
  nsresult rv = snapshot->Init(aKey, initInfo, aExplicit);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  // This is cleared in LSSnapshot::Run() before the snapshot is destroyed.
  mSnapshot = snapshot;

  return NS_OK;
}

void LSDatabase::AllowToClose() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mAllowedToClose);
  MOZ_ASSERT(!mSnapshot);

  mAllowedToClose = true;

  if (mActor) {
    mActor->SendAllowToClose();
  }

  MOZ_ASSERT(gLSDatabases);
  MOZ_ASSERT(gLSDatabases->Get(mOrigin));
  gLSDatabases->Remove(mOrigin);

  if (!gLSDatabases->Count()) {
    gLSDatabases = nullptr;

    MOZ_ASSERT(sObserver);

    nsCOMPtr<nsIObserverService> obsSvc = GetObserverService();
    MOZ_ASSERT(obsSvc);

    MOZ_ALWAYS_SUCCEEDS(
        obsSvc->RemoveObserver(sObserver, XPCOM_SHUTDOWN_OBSERVER_TOPIC));

    // We also need to invalidate the observer because AllowToClose can be
    // triggered by an indirectly related observer, so the observer service
    // may still keep our observer alive and call Observe on it. This is
    // possible because observer service snapshots the observer list for given
    // subject before looping over the list.
    sObserver->Invalidate();

    sObserver = nullptr;
  }
}

NS_IMPL_ISUPPORTS(LSDatabase::Observer, nsIObserver)

NS_IMETHODIMP
LSDatabase::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                              const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!strcmp(aTopic, XPCOM_SHUTDOWN_OBSERVER_TOPIC));

  if (mInvalidated) {
    return NS_OK;
  }

  MOZ_ASSERT(gLSDatabases);

  nsTArray<RefPtr<LSDatabase>> databases;

  for (auto iter = gLSDatabases->ConstIter(); !iter.Done(); iter.Next()) {
    LSDatabase* database = iter.Data();
    MOZ_ASSERT(database);

    databases.AppendElement(database);
  }

  for (RefPtr<LSDatabase>& database : databases) {
    database->RequestAllowToClose();
  }

  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
