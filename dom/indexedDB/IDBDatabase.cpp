/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBDatabase.h"

#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "IndexedDatabaseInlines.h"
#include "IndexedDatabaseManager.h"
#include "IndexedDBCommon.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventDispatcher.h"
#include "MainThreadUtils.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/Services.h"
#include "mozilla/storage.h"
#include "mozilla/Telemetry.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMStringListBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/IDBDatabaseBinding.h"
#include "mozilla/dom/IDBObjectStoreBinding.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseFileChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/dom/IPCBlobUtils.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/dom/quota/ResultExtensions.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/Document.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsISupportsPrimitives.h"
#include "nsThreadUtils.h"
#include "nsIWeakReferenceUtils.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"
#include "ScriptErrorHelper.h"
#include "nsQueryObject.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla::dom {

using namespace mozilla::dom::indexedDB;
using namespace mozilla::dom::quota;
using namespace mozilla::ipc;
using namespace mozilla::services;

namespace {

const char kCycleCollectionObserverTopic[] = "cycle-collector-end";
const char kMemoryPressureObserverTopic[] = "memory-pressure";
const char kWindowObserverTopic[] = "inner-window-destroyed";

class CancelableRunnableWrapper final : public CancelableRunnable {
  nsCOMPtr<nsIRunnable> mRunnable;

 public:
  explicit CancelableRunnableWrapper(nsCOMPtr<nsIRunnable> aRunnable)
      : CancelableRunnable("dom::CancelableRunnableWrapper"),
        mRunnable(std::move(aRunnable)) {
    MOZ_ASSERT(mRunnable);
  }

 private:
  ~CancelableRunnableWrapper() = default;

  NS_DECL_NSIRUNNABLE
  nsresult Cancel() override;
};

class DatabaseFile final : public PBackgroundIDBDatabaseFileChild {
  IDBDatabase* mDatabase;

 public:
  explicit DatabaseFile(IDBDatabase* aDatabase) : mDatabase(aDatabase) {
    MOZ_ASSERT(aDatabase);
    aDatabase->AssertIsOnOwningThread();

    MOZ_COUNT_CTOR(DatabaseFile);
  }

 private:
  ~DatabaseFile() {
    MOZ_ASSERT(!mDatabase);

    MOZ_COUNT_DTOR(DatabaseFile);
  }

  virtual void ActorDestroy(ActorDestroyReason aWhy) override {
    MOZ_ASSERT(mDatabase);
    mDatabase->AssertIsOnOwningThread();

    if (aWhy != Deletion) {
      RefPtr<IDBDatabase> database = mDatabase;
      database->NoteFinishedFileActor(this);
    }

#ifdef DEBUG
    mDatabase = nullptr;
#endif
  }
};

}  // namespace

class IDBDatabase::Observer final : public nsIObserver {
  IDBDatabase* mWeakDatabase;
  const uint64_t mWindowId;

 public:
  Observer(IDBDatabase* aDatabase, uint64_t aWindowId)
      : mWeakDatabase(aDatabase), mWindowId(aWindowId) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aDatabase);
  }

  void Revoke() {
    MOZ_ASSERT(NS_IsMainThread());

    mWeakDatabase = nullptr;
  }

  NS_DECL_ISUPPORTS

 private:
  ~Observer() {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mWeakDatabase);
  }

  NS_DECL_NSIOBSERVER
};

IDBDatabase::IDBDatabase(IDBOpenDBRequest* aRequest,
                         SafeRefPtr<IDBFactory> aFactory,
                         BackgroundDatabaseChild* aActor,
                         UniquePtr<DatabaseSpec> aSpec)
    : DOMEventTargetHelper(aRequest),
      mFactory(std::move(aFactory)),
      mSpec(std::move(aSpec)),
      mBackgroundActor(aActor),
      mClosed(false),
      mInvalidated(false),
      mQuotaExceeded(false),
      mIncreasedActiveDatabaseCount(false) {
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(mFactory);
  mFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(mSpec);
}

IDBDatabase::~IDBDatabase() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mBackgroundActor);
  MOZ_ASSERT(!mIncreasedActiveDatabaseCount);
}

// static
RefPtr<IDBDatabase> IDBDatabase::Create(IDBOpenDBRequest* aRequest,
                                        SafeRefPtr<IDBFactory> aFactory,
                                        BackgroundDatabaseChild* aActor,
                                        UniquePtr<DatabaseSpec> aSpec) {
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(aFactory);
  aFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aSpec);

  RefPtr<IDBDatabase> db =
      new IDBDatabase(aRequest, aFactory.clonePtr(), aActor, std::move(aSpec));

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(aFactory->GetParentObject());
    if (window) {
      uint64_t windowId = window->WindowID();

      RefPtr<Observer> observer = new Observer(db, windowId);

      nsCOMPtr<nsIObserverService> obsSvc = GetObserverService();
      MOZ_ASSERT(obsSvc);

      // This topic must be successfully registered.
      MOZ_ALWAYS_SUCCEEDS(
          obsSvc->AddObserver(observer, kWindowObserverTopic, false));

      // These topics are not crucial.
      QM_WARNONLY_TRY(QM_TO_RESULT(
          obsSvc->AddObserver(observer, kCycleCollectionObserverTopic, false)));
      QM_WARNONLY_TRY(QM_TO_RESULT(
          obsSvc->AddObserver(observer, kMemoryPressureObserverTopic, false)));

      db->mObserver = std::move(observer);
    }
  }

  db->IncreaseActiveDatabaseCount();

  return db;
}

#ifdef DEBUG

void IDBDatabase::AssertIsOnOwningThread() const {
  MOZ_ASSERT(mFactory);
  mFactory->AssertIsOnOwningThread();
}

#endif  // DEBUG

nsIEventTarget* IDBDatabase::EventTarget() const {
  AssertIsOnOwningThread();
  return mFactory->EventTarget();
}

void IDBDatabase::CloseInternal() {
  AssertIsOnOwningThread();

  if (!mClosed) {
    mClosed = true;

    ExpireFileActors(/* aExpireAll */ true);

    if (mObserver) {
      mObserver->Revoke();

      nsCOMPtr<nsIObserverService> obsSvc = GetObserverService();
      if (obsSvc) {
        // These might not have been registered.
        obsSvc->RemoveObserver(mObserver, kCycleCollectionObserverTopic);
        obsSvc->RemoveObserver(mObserver, kMemoryPressureObserverTopic);

        MOZ_ALWAYS_SUCCEEDS(
            obsSvc->RemoveObserver(mObserver, kWindowObserverTopic));
      }

      mObserver = nullptr;
    }

    if (mBackgroundActor && !mInvalidated) {
      mBackgroundActor->SendClose();
    }

    // Decrease the number of active databases right after the database is
    // closed.
    MaybeDecreaseActiveDatabaseCount();
  }
}

void IDBDatabase::InvalidateInternal() {
  AssertIsOnOwningThread();

  AbortTransactions(/* aShouldWarn */ true);

  CloseInternal();
}

void IDBDatabase::EnterSetVersionTransaction(uint64_t aNewVersion) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aNewVersion);
  MOZ_ASSERT(!RunningVersionChangeTransaction());
  MOZ_ASSERT(mSpec);
  MOZ_ASSERT(!mPreviousSpec);

  mPreviousSpec = MakeUnique<DatabaseSpec>(*mSpec);

  mSpec->metadata().version() = aNewVersion;
}

void IDBDatabase::ExitSetVersionTransaction() {
  AssertIsOnOwningThread();

  if (mPreviousSpec) {
    mPreviousSpec = nullptr;
  }
}

void IDBDatabase::RevertToPreviousState() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(RunningVersionChangeTransaction());
  MOZ_ASSERT(mPreviousSpec);

  // Hold the current spec alive until RefreshTransactionsSpecEnumerator has
  // finished!
  auto currentSpec = std::move(mSpec);

  mSpec = std::move(mPreviousSpec);

  RefreshSpec(/* aMayDelete */ true);
}

void IDBDatabase::RefreshSpec(bool aMayDelete) {
  AssertIsOnOwningThread();

  for (auto* weakTransaction : mTransactions) {
    const auto transaction =
        SafeRefPtr{weakTransaction, AcquireStrongRefFromRawPtr{}};
    MOZ_ASSERT(transaction);
    transaction->AssertIsOnOwningThread();
    transaction->RefreshSpec(aMayDelete);
  }
}

const nsString& IDBDatabase::Name() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mSpec);

  return mSpec->metadata().name();
}

uint64_t IDBDatabase::Version() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mSpec);

  return mSpec->metadata().version();
}

RefPtr<DOMStringList> IDBDatabase::ObjectStoreNames() const {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mSpec);

  return CreateSortedDOMStringList(
      mSpec->objectStores(),
      [](const auto& objectStore) { return objectStore.metadata().name(); });
}

RefPtr<Document> IDBDatabase::GetOwnerDocument() const {
  if (nsPIDOMWindowInner* window = GetOwner()) {
    return window->GetExtantDoc();
  }
  return nullptr;
}

RefPtr<IDBObjectStore> IDBDatabase::CreateObjectStore(
    const nsAString& aName, const IDBObjectStoreParameters& aOptionalParameters,
    ErrorResult& aRv) {
  AssertIsOnOwningThread();

  const auto transaction = IDBTransaction::MaybeCurrent();
  if (!transaction || transaction->Database() != this ||
      transaction->GetMode() != IDBTransaction::Mode::VersionChange) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  if (!transaction->IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return nullptr;
  }

  QM_INFOONLY_TRY_UNWRAP(const auto maybeKeyPath,
                         KeyPath::Parse(aOptionalParameters.mKeyPath));
  if (!maybeKeyPath) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  const auto& keyPath = maybeKeyPath.ref();

  auto& objectStores = mSpec->objectStores();
  const auto end = objectStores.cend();
  const auto foundIt = std::find_if(
      objectStores.cbegin(), end, [&aName](const auto& objectStore) {
        return aName == objectStore.metadata().name();
      });
  if (foundIt != end) {
    aRv.ThrowConstraintError(nsPrintfCString(
        "Object store named '%s' already exists at index '%zu'",
        NS_ConvertUTF16toUTF8(aName).get(), foundIt.GetIndex()));
    return nullptr;
  }

  if (!keyPath.IsAllowedForObjectStore(aOptionalParameters.mAutoIncrement)) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  const ObjectStoreSpec* oldSpecElements =
      objectStores.IsEmpty() ? nullptr : objectStores.Elements();

  ObjectStoreSpec* newSpec = objectStores.AppendElement();
  newSpec->metadata() =
      ObjectStoreMetadata(transaction->NextObjectStoreId(), nsString(aName),
                          keyPath, aOptionalParameters.mAutoIncrement);

  if (oldSpecElements && oldSpecElements != objectStores.Elements()) {
    MOZ_ASSERT(objectStores.Length() > 1);

    // Array got moved, update the spec pointers for all live objectStores and
    // indexes.
    RefreshSpec(/* aMayDelete */ false);
  }

  auto objectStore = transaction->CreateObjectStore(*newSpec);
  MOZ_ASSERT(objectStore);

  // Don't do this in the macro because we always need to increment the serial
  // number to keep in sync with the parent.
  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();

  IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
      "database(%s).transaction(%s).createObjectStore(%s)",
      "IDBDatabase.createObjectStore(%.0s%.0s%.0s)",
      transaction->LoggingSerialNumber(), requestSerialNumber,
      IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(*transaction),
      IDB_LOG_STRINGIFY(objectStore));

  return objectStore;
}

void IDBDatabase::DeleteObjectStore(const nsAString& aName, ErrorResult& aRv) {
  AssertIsOnOwningThread();

  const auto transaction = IDBTransaction::MaybeCurrent();
  if (!transaction || transaction->Database() != this ||
      transaction->GetMode() != IDBTransaction::Mode::VersionChange) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  if (!transaction->IsActive()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR);
    return;
  }

  auto& specArray = mSpec->objectStores();
  const auto end = specArray.end();
  const auto foundIt =
      std::find_if(specArray.begin(), end, [&aName](const auto& objectStore) {
        const ObjectStoreMetadata& metadata = objectStore.metadata();
        MOZ_ASSERT(metadata.id());

        return aName == metadata.name();
      });

  if (foundIt == end) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
    return;
  }

  // Must do this before altering the metadata array!
  transaction->DeleteObjectStore(foundIt->metadata().id());

  specArray.RemoveElementAt(foundIt);
  RefreshSpec(/* aMayDelete */ false);

  // Don't do this in the macro because we always need to increment the serial
  // number to keep in sync with the parent.
  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();

  IDB_LOG_MARK_CHILD_TRANSACTION_REQUEST(
      "database(%s).transaction(%s).deleteObjectStore(\"%s\")",
      "IDBDatabase.deleteObjectStore(%.0s%.0s%.0s)",
      transaction->LoggingSerialNumber(), requestSerialNumber,
      IDB_LOG_STRINGIFY(this), IDB_LOG_STRINGIFY(*transaction),
      NS_ConvertUTF16toUTF8(aName).get());
}

RefPtr<IDBTransaction> IDBDatabase::Transaction(
    JSContext* aCx, const StringOrStringSequence& aStoreNames,
    IDBTransactionMode aMode, ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if ((aMode == IDBTransactionMode::Readwriteflush ||
       aMode == IDBTransactionMode::Cleanup) &&
      !StaticPrefs::dom_indexedDB_experimental()) {
    // Pretend that this mode doesn't exist. We don't have a way to annotate
    // certain enum values as depending on preferences so we just duplicate the
    // normal exception generation here.
    aRv.ThrowTypeError<MSG_INVALID_ENUM_VALUE>("argument 2", "readwriteflush",
                                               "IDBTransactionMode");
    return nullptr;
  }

  if (QuotaManager::IsShuttingDown()) {
    IDB_REPORT_INTERNAL_ERR();
    aRv.ThrowUnknownError("Can't start IndexedDB transaction during shutdown");
    return nullptr;
  }

  // https://w3c.github.io/IndexedDB/#dom-idbdatabase-transaction
  // Step 1.
  if (RunningVersionChangeTransaction()) {
    aRv.ThrowInvalidStateError(
        "Can't start a transaction while running an upgrade transaction");
    return nullptr;
  }

  // Step 2.
  if (mClosed) {
    aRv.ThrowInvalidStateError(
        "Can't start a transaction on a closed database");
    return nullptr;
  }

  // Step 3.
  AutoTArray<nsString, 1> stackSequence;

  if (aStoreNames.IsString()) {
    stackSequence.AppendElement(aStoreNames.GetAsString());
  } else {
    MOZ_ASSERT(aStoreNames.IsStringSequence());
    // Step 5, but it can be done before step 4 because those steps
    // can't both throw.
    if (aStoreNames.GetAsStringSequence().IsEmpty()) {
      aRv.ThrowInvalidAccessError("Empty scope passed in");
      return nullptr;
    }
  }

  // Step 4.
  const nsTArray<nsString>& storeNames =
      aStoreNames.IsString() ? stackSequence
                             : static_cast<const nsTArray<nsString>&>(
                                   aStoreNames.GetAsStringSequence());
  MOZ_ASSERT(!storeNames.IsEmpty());

  const nsTArray<ObjectStoreSpec>& objectStores = mSpec->objectStores();
  const uint32_t nameCount = storeNames.Length();

  nsTArray<nsString> sortedStoreNames;
  sortedStoreNames.SetCapacity(nameCount);

  // While collecting object store names, check if the corresponding object
  // stores actually exist.
  const auto begin = objectStores.cbegin();
  const auto end = objectStores.cend();
  for (const auto& name : storeNames) {
    const auto foundIt =
        std::find_if(begin, end, [&name](const auto& objectStore) {
          return objectStore.metadata().name() == name;
        });
    if (foundIt == end) {
      Telemetry::ScalarAdd(
          storeNames.IsEmpty()
              ? Telemetry::ScalarID::
                    IDB_FAILURE_UNKNOWN_OBJECTSTORE_EMPTY_DATABASE
              : Telemetry::ScalarID::
                    IDB_FAILURE_UNKNOWN_OBJECTSTORE_NON_EMPTY_DATABASE,
          1);

      // Not using nsPrintfCString in case "name" has embedded nulls.
      aRv.ThrowNotFoundError("'"_ns + NS_ConvertUTF16toUTF8(name) +
                             "' is not a known object store name"_ns);
      return nullptr;
    }

    sortedStoreNames.EmplaceBack(name);
  }
  sortedStoreNames.Sort();

  // Remove any duplicates.
  sortedStoreNames.SetLength(
      std::unique(sortedStoreNames.begin(), sortedStoreNames.end()).GetIndex());

  IDBTransaction::Mode mode;
  switch (aMode) {
    case IDBTransactionMode::Readonly:
      mode = IDBTransaction::Mode::ReadOnly;
      break;
    case IDBTransactionMode::Readwrite:
      if (mQuotaExceeded) {
        mode = IDBTransaction::Mode::Cleanup;
        mQuotaExceeded = false;
      } else {
        mode = IDBTransaction::Mode::ReadWrite;
      }
      break;
    case IDBTransactionMode::Readwriteflush:
      mode = IDBTransaction::Mode::ReadWriteFlush;
      break;
    case IDBTransactionMode::Cleanup:
      mode = IDBTransaction::Mode::Cleanup;
      mQuotaExceeded = false;
      break;
    case IDBTransactionMode::Versionchange:
      // Step 6.
      aRv.ThrowTypeError("Invalid transaction mode");
      return nullptr;

    default:
      MOZ_CRASH("Unknown mode!");
  }

  SafeRefPtr<IDBTransaction> transaction =
      IDBTransaction::Create(aCx, this, sortedStoreNames, mode);
  if (NS_WARN_IF(!transaction)) {
    IDB_REPORT_INTERNAL_ERR();
    MOZ_ASSERT(!NS_IsMainThread(),
               "Transaction creation can only fail on workers");
    aRv.ThrowUnknownError("Failed to create IndexedDB transaction on worker");
    return nullptr;
  }

  RefPtr<BackgroundTransactionChild> actor =
      new BackgroundTransactionChild(transaction.clonePtr());

  IDB_LOG_MARK_CHILD_TRANSACTION(
      "database(%s).transaction(%s)", "IDBDatabase.transaction(%.0s%.0s)",
      transaction->LoggingSerialNumber(), IDB_LOG_STRINGIFY(this),
      IDB_LOG_STRINGIFY(*transaction));

  if (!mBackgroundActor->SendPBackgroundIDBTransactionConstructor(
          actor, sortedStoreNames, mode)) {
    IDB_REPORT_INTERNAL_ERR();
    aRv.ThrowUnknownError("Failed to create IndexedDB transaction");
    return nullptr;
  }

  transaction->SetBackgroundActor(actor);

  if (mode == IDBTransaction::Mode::Cleanup) {
    ExpireFileActors(/* aExpireAll */ true);
  }

  return AsRefPtr(std::move(transaction));
}

void IDBDatabase::RegisterTransaction(IDBTransaction& aTransaction) {
  AssertIsOnOwningThread();
  aTransaction.AssertIsOnOwningThread();
  MOZ_ASSERT(!mTransactions.Contains(&aTransaction));

  mTransactions.Insert(&aTransaction);
}

void IDBDatabase::UnregisterTransaction(IDBTransaction& aTransaction) {
  AssertIsOnOwningThread();
  aTransaction.AssertIsOnOwningThread();
  MOZ_ASSERT(mTransactions.Contains(&aTransaction));

  mTransactions.Remove(&aTransaction);
}

void IDBDatabase::AbortTransactions(bool aShouldWarn) {
  AssertIsOnOwningThread();

  constexpr size_t StackExceptionLimit = 20;
  using StrongTransactionArray =
      AutoTArray<SafeRefPtr<IDBTransaction>, StackExceptionLimit>;
  using WeakTransactionArray = AutoTArray<IDBTransaction*, StackExceptionLimit>;

  if (!mTransactions.Count()) {
    // Return early as an optimization, the remainder is a no-op in this
    // case.
    return;
  }

  // XXX TransformIfIntoNewArray might be generalized to allow specifying the
  // type of nsTArray to create, so that it can create an AutoTArray as well; an
  // TransformIf (without AbortOnErr) might be added, which could be used here.
  StrongTransactionArray transactionsToAbort;
  transactionsToAbort.SetCapacity(mTransactions.Count());

  for (IDBTransaction* const transaction : mTransactions) {
    MOZ_ASSERT(transaction);

    transaction->AssertIsOnOwningThread();

    // Transactions that are already done can simply be ignored. Otherwise
    // there is a race here and it's possible that the transaction has not
    // been successfully committed yet so we will warn the user.
    if (!transaction->IsFinished()) {
      transactionsToAbort.EmplaceBack(transaction,
                                      AcquireStrongRefFromRawPtr{});
    }
  }
  MOZ_ASSERT(transactionsToAbort.Length() <= mTransactions.Count());

  if (transactionsToAbort.IsEmpty()) {
    // Return early as an optimization, the remainder is a no-op in this
    // case.
    return;
  }

  // We want to abort transactions as soon as possible so we iterate the
  // transactions once and abort them all first, collecting the transactions
  // that need to have a warning issued along the way. Those that need a
  // warning will be a subset of those that are aborted, so we don't need
  // additional strong references here.
  WeakTransactionArray transactionsThatNeedWarning;

  for (const auto& transaction : transactionsToAbort) {
    MOZ_ASSERT(transaction);
    MOZ_ASSERT(!transaction->IsFinished());

    // We warn for any transactions that could have written data, but
    // ignore read-only transactions.
    if (aShouldWarn && transaction->IsWriteAllowed()) {
      transactionsThatNeedWarning.AppendElement(transaction.unsafeGetRawPtr());
    }

    transaction->Abort(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR);
  }

  static const char kWarningMessage[] = "IndexedDBTransactionAbortNavigation";

  for (IDBTransaction* transaction : transactionsThatNeedWarning) {
    MOZ_ASSERT(transaction);

    nsString filename;
    uint32_t lineNo, column;
    transaction->GetCallerLocation(filename, &lineNo, &column);

    LogWarning(kWarningMessage, filename, lineNo, column);
  }
}

PBackgroundIDBDatabaseFileChild* IDBDatabase::GetOrCreateFileActorForBlob(
    Blob& aBlob) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mBackgroundActor);

  // We use the File's nsIWeakReference as the key to the table because
  // a) it is unique per blob, b) it is reference-counted so that we can
  // guarantee that it stays alive, and c) it doesn't hold the actual File
  // alive.
  nsWeakPtr weakRef = do_GetWeakReference(&aBlob);
  MOZ_ASSERT(weakRef);

  PBackgroundIDBDatabaseFileChild* actor = nullptr;

  if (!mFileActors.Get(weakRef, &actor)) {
    BlobImpl* blobImpl = aBlob.Impl();
    MOZ_ASSERT(blobImpl);

    IPCBlob ipcBlob;
    nsresult rv = IPCBlobUtils::Serialize(blobImpl, ipcBlob);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return nullptr;
    }

    auto* dbFile = new DatabaseFile(this);

    actor = mBackgroundActor->SendPBackgroundIDBDatabaseFileConstructor(
        dbFile, ipcBlob);
    if (NS_WARN_IF(!actor)) {
      return nullptr;
    }

    mFileActors.InsertOrUpdate(weakRef, actor);
  }

  MOZ_ASSERT(actor);

  return actor;
}

void IDBDatabase::NoteFinishedFileActor(
    PBackgroundIDBDatabaseFileChild* aFileActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileActor);

  mFileActors.RemoveIf([aFileActor](const auto& iter) {
    MOZ_ASSERT(iter.Key());
    PBackgroundIDBDatabaseFileChild* actor = iter.Data();
    MOZ_ASSERT(actor);

    return actor == aFileActor;
  });
}

void IDBDatabase::NoteActiveTransaction() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFactory);

  // Increase the number of active transactions.
  mFactory->UpdateActiveTransactionCount(1);
}

void IDBDatabase::NoteInactiveTransaction() {
  AssertIsOnOwningThread();

  if (!mBackgroundActor || !mFileActors.Count()) {
    MOZ_ASSERT(mFactory);
    mFactory->UpdateActiveTransactionCount(-1);
    return;
  }

  RefPtr<Runnable> runnable =
      NewRunnableMethod("IDBDatabase::NoteInactiveTransactionDelayed", this,
                        &IDBDatabase::NoteInactiveTransactionDelayed);
  MOZ_ASSERT(runnable);

  if (!NS_IsMainThread()) {
    // Wrap as a nsICancelableRunnable to make workers happy.
    runnable = MakeRefPtr<CancelableRunnableWrapper>(runnable.forget());
  }

  MOZ_ALWAYS_SUCCEEDS(
      EventTarget()->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL));
}

void IDBDatabase::ExpireFileActors(bool aExpireAll) {
  AssertIsOnOwningThread();

  if (mBackgroundActor && mFileActors.Count()) {
    for (auto iter = mFileActors.Iter(); !iter.Done(); iter.Next()) {
      nsISupports* key = iter.Key();
      PBackgroundIDBDatabaseFileChild* actor = iter.Data();
      MOZ_ASSERT(key);
      MOZ_ASSERT(actor);

      bool shouldExpire = aExpireAll;
      if (!shouldExpire) {
        nsWeakPtr weakRef = do_QueryInterface(key);
        MOZ_ASSERT(weakRef);

        nsCOMPtr<nsISupports> referent = do_QueryReferent(weakRef);
        shouldExpire = !referent;
      }

      if (shouldExpire) {
        PBackgroundIDBDatabaseFileChild::Send__delete__(actor);

        if (!aExpireAll) {
          iter.Remove();
        }
      }
    }
    if (aExpireAll) {
      mFileActors.Clear();
    }
  } else {
    MOZ_ASSERT(!mFileActors.Count());
  }
}

void IDBDatabase::Invalidate() {
  AssertIsOnOwningThread();

  if (!mInvalidated) {
    mInvalidated = true;

    InvalidateInternal();
  }
}

void IDBDatabase::NoteInactiveTransactionDelayed() {
  ExpireFileActors(/* aExpireAll */ false);

  MOZ_ASSERT(mFactory);
  mFactory->UpdateActiveTransactionCount(-1);
}

void IDBDatabase::LogWarning(const char* aMessageName,
                             const nsAString& aFilename, uint32_t aLineNumber,
                             uint32_t aColumnNumber) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aMessageName);

  ScriptErrorHelper::DumpLocalizedMessage(
      nsDependentCString(aMessageName), aFilename, aLineNumber, aColumnNumber,
      nsIScriptError::warningFlag, mFactory->IsChrome(),
      mFactory->InnerWindowID());
}

NS_IMPL_ADDREF_INHERITED(IDBDatabase, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBDatabase, DOMEventTargetHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBDatabase)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBDatabase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBDatabase,
                                                  DOMEventTargetHelper)
  tmp->AssertIsOnOwningThread();
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFactory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBDatabase,
                                                DOMEventTargetHelper)
  tmp->AssertIsOnOwningThread();

  // Don't unlink mFactory!

  // We've been unlinked, at the very least we should be able to prevent further
  // transactions from starting and unblock any other SetVersion callers.
  tmp->CloseInternal();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

void IDBDatabase::DisconnectFromOwner() {
  InvalidateInternal();
  DOMEventTargetHelper::DisconnectFromOwner();
}

void IDBDatabase::LastRelease() {
  AssertIsOnOwningThread();

  CloseInternal();

  ExpireFileActors(/* aExpireAll */ true);

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

JSObject* IDBDatabase::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto) {
  return IDBDatabase_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
CancelableRunnableWrapper::Run() {
  const nsCOMPtr<nsIRunnable> runnable = std::move(mRunnable);

  if (runnable) {
    return runnable->Run();
  }

  return NS_OK;
}

nsresult CancelableRunnableWrapper::Cancel() {
  if (mRunnable) {
    mRunnable = nullptr;
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

NS_IMPL_ISUPPORTS(IDBDatabase::Observer, nsIObserver)

NS_IMETHODIMP
IDBDatabase::Observer::Observe(nsISupports* aSubject, const char* aTopic,
                               const char16_t* aData) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTopic);

  if (!strcmp(aTopic, kWindowObserverTopic)) {
    if (mWeakDatabase) {
      nsCOMPtr<nsISupportsPRUint64> supportsInt = do_QueryInterface(aSubject);
      MOZ_ASSERT(supportsInt);

      uint64_t windowId;
      MOZ_ALWAYS_SUCCEEDS(supportsInt->GetData(&windowId));

      if (windowId == mWindowId) {
        RefPtr<IDBDatabase> database = mWeakDatabase;
        mWeakDatabase = nullptr;

        database->InvalidateInternal();
      }
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, kCycleCollectionObserverTopic) ||
      !strcmp(aTopic, kMemoryPressureObserverTopic)) {
    if (mWeakDatabase) {
      RefPtr<IDBDatabase> database = mWeakDatabase;

      database->ExpireFileActors(/* aExpireAll */ false);
    }

    return NS_OK;
  }

  NS_WARNING("Unknown observer topic!");
  return NS_OK;
}

nsresult IDBDatabase::RenameObjectStore(int64_t aObjectStoreId,
                                        const nsAString& aName) {
  MOZ_ASSERT(mSpec);

  nsTArray<ObjectStoreSpec>& objectStores = mSpec->objectStores();
  ObjectStoreSpec* foundObjectStoreSpec = nullptr;

  // Find the matched object store spec and check if 'aName' is already used by
  // another object store.

  for (auto& objSpec : objectStores) {
    const bool idIsCurrent = objSpec.metadata().id() == aObjectStoreId;

    if (idIsCurrent) {
      MOZ_ASSERT(!foundObjectStoreSpec);
      foundObjectStoreSpec = &objSpec;
    }

    if (objSpec.metadata().name() == aName) {
      if (idIsCurrent) {
        return NS_OK;
      }
      return NS_ERROR_DOM_INDEXEDDB_RENAME_OBJECT_STORE_ERR;
    }
  }

  MOZ_ASSERT(foundObjectStoreSpec);

  // Update the name of the matched object store.
  foundObjectStoreSpec->metadata().name().Assign(aName);

  return NS_OK;
}

nsresult IDBDatabase::RenameIndex(int64_t aObjectStoreId, int64_t aIndexId,
                                  const nsAString& aName) {
  MOZ_ASSERT(mSpec);

  nsTArray<ObjectStoreSpec>& objectStores = mSpec->objectStores();

  ObjectStoreSpec* foundObjectStoreSpec = nullptr;
  // Find the matched index metadata and check if 'aName' is already used by
  // another index.
  for (uint32_t objCount = objectStores.Length(), objIndex = 0;
       objIndex < objCount; objIndex++) {
    const ObjectStoreSpec& objSpec = objectStores[objIndex];
    if (objSpec.metadata().id() == aObjectStoreId) {
      foundObjectStoreSpec = &objectStores[objIndex];
      break;
    }
  }

  MOZ_ASSERT(foundObjectStoreSpec);

  nsTArray<IndexMetadata>& indexes = foundObjectStoreSpec->indexes();
  IndexMetadata* foundIndexMetadata = nullptr;
  for (uint32_t idxCount = indexes.Length(), idxIndex = 0; idxIndex < idxCount;
       idxIndex++) {
    const IndexMetadata& metadata = indexes[idxIndex];
    if (metadata.id() == aIndexId) {
      MOZ_ASSERT(!foundIndexMetadata);
      foundIndexMetadata = &indexes[idxIndex];
      continue;
    }
    if (aName == metadata.name()) {
      return NS_ERROR_DOM_INDEXEDDB_RENAME_INDEX_ERR;
    }
  }

  MOZ_ASSERT(foundIndexMetadata);

  // Update the name of the matched object store.
  foundIndexMetadata->name() = nsString(aName);

  return NS_OK;
}

void IDBDatabase::IncreaseActiveDatabaseCount() {
  AssertIsOnOwningThread();
  MOZ_ASSERT(mFactory);
  MOZ_ASSERT(!mIncreasedActiveDatabaseCount);

  mFactory->UpdateActiveDatabaseCount(1);
  mIncreasedActiveDatabaseCount = true;
}

void IDBDatabase::MaybeDecreaseActiveDatabaseCount() {
  AssertIsOnOwningThread();

  if (mIncreasedActiveDatabaseCount) {
    // Decrease the number of active databases.
    MOZ_ASSERT(mFactory);
    mFactory->UpdateActiveDatabaseCount(-1);
    mIncreasedActiveDatabaseCount = false;
  }
}

}  // namespace mozilla::dom
