/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBDatabase.h"

#include "FileInfo.h"
#include "FileManager.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBMutableFile.h"
#include "IDBObjectStore.h"
#include "IDBRequest.h"
#include "IDBTransaction.h"
#include "IDBFactory.h"
#include "IndexedDatabaseManager.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/EventDispatcher.h"
#include "MainThreadUtils.h"
#include "mozilla/Services.h"
#include "mozilla/storage.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/DOMStringList.h"
#include "mozilla/dom/DOMStringListBinding.h"
#include "mozilla/dom/File.h"
#include "mozilla/dom/IDBDatabaseBinding.h"
#include "mozilla/dom/IDBObjectStoreBinding.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBDatabaseFileChild.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"
#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/dom/ipc/BlobChild.h"
#include "mozilla/dom/ipc/nsIRemoteBlob.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "mozilla/ipc/FileDescriptor.h"
#include "mozilla/ipc/InputStreamParams.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIConsoleService.h"
#include "nsIDocument.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsIScriptError.h"
#include "nsISupportsPrimitives.h"
#include "nsThreadUtils.h"
#include "ProfilerHelpers.h"
#include "ReportInternalError.h"

// Include this last to avoid path problems on Windows.
#include "ActorsChild.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

using namespace mozilla::dom::quota;
using namespace mozilla::ipc;
using namespace mozilla::services;

namespace {

const char kCycleCollectionObserverTopic[] = "cycle-collector-end";
const char kMemoryPressureObserverTopic[] = "memory-pressure";
const char kWindowObserverTopic[] = "inner-window-destroyed";

class CancelableRunnableWrapper MOZ_FINAL
  : public nsICancelableRunnable
{
  nsCOMPtr<nsIRunnable> mRunnable;

public:
  explicit
  CancelableRunnableWrapper(nsIRunnable* aRunnable)
    : mRunnable(aRunnable)
  {
    MOZ_ASSERT(aRunnable);
  }

  NS_DECL_ISUPPORTS

private:
  ~CancelableRunnableWrapper()
  { }

  NS_DECL_NSIRUNNABLE
  NS_DECL_NSICANCELABLERUNNABLE
};

// XXX This should either be ported to PBackground or removed someday.
class CreateFileHelper MOZ_FINAL
  : public nsRunnable
{
  nsRefPtr<IDBDatabase> mDatabase;
  nsRefPtr<IDBRequest> mRequest;
  nsRefPtr<FileInfo> mFileInfo;

  const nsString mName;
  const nsString mType;
  const nsString mDatabaseName;
  const nsCString mOrigin;

  const PersistenceType mPersistenceType;

  nsresult mResultCode;

public:
  static nsresult
  CreateAndDispatch(IDBDatabase* aDatabase,
                    IDBRequest* aRequest,
                    const nsAString& aName,
                    const nsAString& aType);

  NS_DECL_ISUPPORTS_INHERITED

private:
  CreateFileHelper(IDBDatabase* aDatabase,
                   IDBRequest* aRequest,
                   const nsAString& aName,
                   const nsAString& aType,
                   const nsACString& aOrigin);

  ~CreateFileHelper()
  {
    MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  }

  nsresult
  DoDatabaseWork();

  void
  DoMainThreadWork(nsresult aResultCode);

  NS_DECL_NSIRUNNABLE
};

class DatabaseFile MOZ_FINAL
  : public PBackgroundIDBDatabaseFileChild
{
  IDBDatabase* mDatabase;

public:
  explicit DatabaseFile(IDBDatabase* aDatabase)
    : mDatabase(aDatabase)
  {
    MOZ_ASSERT(aDatabase);
    aDatabase->AssertIsOnOwningThread();

    MOZ_COUNT_CTOR(DatabaseFile);
  }

private:
  ~DatabaseFile()
  {
    MOZ_ASSERT(!mDatabase);

    MOZ_COUNT_DTOR(DatabaseFile);
  }

  virtual void
  ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE
  {
    MOZ_ASSERT(mDatabase);
    mDatabase->AssertIsOnOwningThread();

    if (aWhy != Deletion) {
      nsRefPtr<IDBDatabase> database = mDatabase;
      database->NoteFinishedFileActor(this);
    }

#ifdef DEBUG
    mDatabase = nullptr;
#endif
  }
};

} // anonymous namespace

class IDBDatabase::LogWarningRunnable MOZ_FINAL
  : public nsRunnable
{
  nsCString mMessageName;
  nsString mFilename;
  uint32_t mLineNumber;
  uint64_t mInnerWindowID;
  bool mIsChrome;

public:
  LogWarningRunnable(const char* aMessageName,
                     const nsAString& aFilename,
                     uint32_t aLineNumber,
                     bool aIsChrome,
                     uint64_t aInnerWindowID)
    : mMessageName(aMessageName)
    , mFilename(aFilename)
    , mLineNumber(aLineNumber)
    , mInnerWindowID(aInnerWindowID)
    , mIsChrome(aIsChrome)
  {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  static void
  LogWarning(const char* aMessageName,
             const nsAString& aFilename,
             uint32_t aLineNumber,
             bool aIsChrome,
             uint64_t aInnerWindowID);

  NS_DECL_ISUPPORTS_INHERITED

private:
  ~LogWarningRunnable()
  { }

  NS_DECL_NSIRUNNABLE
};

class IDBDatabase::Observer MOZ_FINAL
  : public nsIObserver
{
  IDBDatabase* mWeakDatabase;
  const uint64_t mWindowId;

public:
  Observer(IDBDatabase* aDatabase, uint64_t aWindowId)
    : mWeakDatabase(aDatabase)
    , mWindowId(aWindowId)
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aDatabase);
  }

  void
  Revoke()
  {
    MOZ_ASSERT(NS_IsMainThread());

    mWeakDatabase = nullptr;
  }

  NS_DECL_ISUPPORTS

private:
  ~Observer()
  {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(!mWeakDatabase);
  }

  NS_DECL_NSIOBSERVER
};

IDBDatabase::IDBDatabase(IDBWrapperCache* aOwnerCache,
                         IDBFactory* aFactory,
                         BackgroundDatabaseChild* aActor,
                         DatabaseSpec* aSpec)
  : IDBWrapperCache(aOwnerCache)
  , mFactory(aFactory)
  , mSpec(aSpec)
  , mBackgroundActor(aActor)
  , mClosed(false)
  , mInvalidated(false)
{
  MOZ_ASSERT(aOwnerCache);
  MOZ_ASSERT(aFactory);
  aFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aSpec);
}

IDBDatabase::~IDBDatabase()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(!mBackgroundActor);
}

// static
already_AddRefed<IDBDatabase>
IDBDatabase::Create(IDBWrapperCache* aOwnerCache,
                    IDBFactory* aFactory,
                    BackgroundDatabaseChild* aActor,
                    DatabaseSpec* aSpec)
{
  MOZ_ASSERT(aOwnerCache);
  MOZ_ASSERT(aFactory);
  aFactory->AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(aSpec);

  nsRefPtr<IDBDatabase> db =
    new IDBDatabase(aOwnerCache, aFactory, aActor, aSpec);

  db->SetScriptOwner(aOwnerCache->GetScriptOwner());

  if (NS_IsMainThread()) {
    if (nsPIDOMWindow* window = aFactory->GetParentObject()) {
      MOZ_ASSERT(window->IsInnerWindow());

      uint64_t windowId = window->WindowID();

      nsRefPtr<Observer> observer = new Observer(db, windowId);

      nsCOMPtr<nsIObserverService> obsSvc = GetObserverService();
      MOZ_ASSERT(obsSvc);

      // This topic must be successfully registered.
      if (NS_WARN_IF(NS_FAILED(
            obsSvc->AddObserver(observer, kWindowObserverTopic, false)))) {
        observer->Revoke();
        return nullptr;
      }

      // These topics are not crucial.
      if (NS_FAILED(obsSvc->AddObserver(observer,
                                        kCycleCollectionObserverTopic,
                                        false)) ||
          NS_FAILED(obsSvc->AddObserver(observer,
                                        kMemoryPressureObserverTopic,
                                        false))) {
        NS_WARNING("Failed to add additional memory observers!");
      }

      db->mObserver.swap(observer);
    }
  }

  return db.forget();
}

#ifdef DEBUG

void
IDBDatabase::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mFactory);
  mFactory->AssertIsOnOwningThread();
}

#endif // DEBUG

void
IDBDatabase::CloseInternal()
{
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

        MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
          obsSvc->RemoveObserver(mObserver, kWindowObserverTopic)));
      }

      mObserver = nullptr;
    }

    if (mBackgroundActor && !mInvalidated) {
      mBackgroundActor->SendClose();
    }
  }
}

void
IDBDatabase::InvalidateInternal()
{
  AssertIsOnOwningThread();

  InvalidateMutableFiles();
  AbortTransactions(/* aShouldWarn */ true);

  CloseInternal();
}

void
IDBDatabase::EnterSetVersionTransaction(uint64_t aNewVersion)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aNewVersion);
  MOZ_ASSERT(!RunningVersionChangeTransaction());
  MOZ_ASSERT(mSpec);
  MOZ_ASSERT(!mPreviousSpec);

  mPreviousSpec = new DatabaseSpec(*mSpec);

  mSpec->metadata().version() = aNewVersion;
}

void
IDBDatabase::ExitSetVersionTransaction()
{
  AssertIsOnOwningThread();

  if (mPreviousSpec) {
    mPreviousSpec = nullptr;
  }
}

void
IDBDatabase::RevertToPreviousState()
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(RunningVersionChangeTransaction());
  MOZ_ASSERT(mPreviousSpec);

  // Hold the current spec alive until RefreshTransactionsSpecEnumerator has
  // finished!
  nsAutoPtr<DatabaseSpec> currentSpec(mSpec.forget());

  mSpec = mPreviousSpec.forget();

  RefreshSpec(/* aMayDelete */ true);
}

void
IDBDatabase::RefreshSpec(bool aMayDelete)
{
  AssertIsOnOwningThread();

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static PLDHashOperator
    RefreshTransactionsSpec(nsPtrHashKey<IDBTransaction>* aTransaction,
                            void* aClosure)
    {
      MOZ_ASSERT(aTransaction);
      aTransaction->GetKey()->AssertIsOnOwningThread();
      MOZ_ASSERT(aClosure);

      bool mayDelete = *static_cast<bool*>(aClosure);

      nsRefPtr<IDBTransaction> transaction = aTransaction->GetKey();
      transaction->RefreshSpec(mayDelete);

      return PL_DHASH_NEXT;
    }
  };

  mTransactions.EnumerateEntries(Helper::RefreshTransactionsSpec, &aMayDelete);
}

nsPIDOMWindow*
IDBDatabase::GetParentObject() const
{
  return mFactory->GetParentObject();
}

const nsString&
IDBDatabase::Name() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mSpec);

  return mSpec->metadata().name();
}

uint64_t
IDBDatabase::Version() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mSpec);

  return mSpec->metadata().version();
}

already_AddRefed<DOMStringList>
IDBDatabase::ObjectStoreNames() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mSpec);

  const nsTArray<ObjectStoreSpec>& objectStores = mSpec->objectStores();

  nsRefPtr<DOMStringList> list = new DOMStringList();

  if (!objectStores.IsEmpty()) {
    nsTArray<nsString>& listNames = list->StringArray();
    listNames.SetCapacity(objectStores.Length());

    for (uint32_t index = 0; index < objectStores.Length(); index++) {
      listNames.InsertElementSorted(objectStores[index].metadata().name());
    }
  }

  return list.forget();
}

already_AddRefed<nsIDocument>
IDBDatabase::GetOwnerDocument() const
{
  if (nsPIDOMWindow* window = GetOwner()) {
    nsCOMPtr<nsIDocument> doc = window->GetExtantDoc();
    return doc.forget();
  }
  return nullptr;
}

already_AddRefed<IDBObjectStore>
IDBDatabase::CreateObjectStore(
                            const nsAString& aName,
                            const IDBObjectStoreParameters& aOptionalParameters,
                            ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  IDBTransaction* transaction = IDBTransaction::GetCurrent();

  if (!transaction ||
      transaction->Database() != this ||
      transaction->GetMode() != IDBTransaction::VERSION_CHANGE) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  MOZ_ASSERT(transaction->IsOpen());

  KeyPath keyPath(0);
  if (NS_FAILED(KeyPath::Parse(aOptionalParameters.mKeyPath, &keyPath))) {
    aRv.Throw(NS_ERROR_DOM_SYNTAX_ERR);
    return nullptr;
  }

  nsTArray<ObjectStoreSpec>& objectStores = mSpec->objectStores();
  for (uint32_t count = objectStores.Length(), index = 0;
       index < count;
       index++) {
    if (aName == objectStores[index].metadata().name()) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR);
      return nullptr;
    }
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

  if (oldSpecElements &&
      oldSpecElements != objectStores.Elements()) {
    MOZ_ASSERT(objectStores.Length() > 1);

    // Array got moved, update the spec pointers for all live objectStores and
    // indexes.
    RefreshSpec(/* aMayDelete */ false);
  }

  nsRefPtr<IDBObjectStore> objectStore =
    transaction->CreateObjectStore(*newSpec);
  MOZ_ASSERT(objectStore);

  // Don't do this in the macro because we always need to increment the serial
  // number to keep in sync with the parent.
  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();

  IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                 "database(%s).transaction(%s).createObjectStore(%s)",
               "IndexedDB %s: C T[%lld] R[%llu]: "
                 "IDBDatabase.createObjectStore()",
               IDB_LOG_ID_STRING(),
               transaction->LoggingSerialNumber(),
               requestSerialNumber,
               IDB_LOG_STRINGIFY(this),
               IDB_LOG_STRINGIFY(transaction),
               IDB_LOG_STRINGIFY(objectStore));

  return objectStore.forget();
}

void
IDBDatabase::DeleteObjectStore(const nsAString& aName, ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  IDBTransaction* transaction = IDBTransaction::GetCurrent();

  if (!transaction ||
      transaction->Database() != this ||
      transaction->GetMode() != IDBTransaction::VERSION_CHANGE) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return;
  }

  MOZ_ASSERT(transaction->IsOpen());

  nsTArray<ObjectStoreSpec>& specArray = mSpec->objectStores();

  int64_t objectStoreId = 0;

  for (uint32_t specCount = specArray.Length(), specIndex = 0;
       specIndex < specCount;
       specIndex++) {
    const ObjectStoreMetadata& metadata = specArray[specIndex].metadata();
    MOZ_ASSERT(metadata.id());

    if (aName == metadata.name()) {
      objectStoreId = metadata.id();

      // Must do this before altering the metadata array!
      transaction->DeleteObjectStore(objectStoreId);

      specArray.RemoveElementAt(specIndex);

      RefreshSpec(/* aMayDelete */ false);
      break;
    }
  }

  if (!objectStoreId) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
    return;
  }

  // Don't do this in the macro because we always need to increment the serial
  // number to keep in sync with the parent.
  const uint64_t requestSerialNumber = IDBRequest::NextSerialNumber();

  IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld] Request[%llu]: "
                 "database(%s).transaction(%s).deleteObjectStore(\"%s\")",
               "IndexedDB %s: C T[%lld] R[%llu]: "
                 "IDBDatabase.deleteObjectStore()",
               IDB_LOG_ID_STRING(),
               transaction->LoggingSerialNumber(),
               requestSerialNumber,
               IDB_LOG_STRINGIFY(this),
               IDB_LOG_STRINGIFY(transaction),
               NS_ConvertUTF16toUTF8(aName).get());
}

already_AddRefed<IDBTransaction>
IDBDatabase::Transaction(const nsAString& aStoreName,
                         IDBTransactionMode aMode,
                         ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  Sequence<nsString> storeNames;
  if (!storeNames.AppendElement(aStoreName)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  return Transaction(storeNames, aMode, aRv);
}

already_AddRefed<IDBTransaction>
IDBDatabase::Transaction(const Sequence<nsString>& aStoreNames,
                         IDBTransactionMode aMode,
                         ErrorResult& aRv)
{
  AssertIsOnOwningThread();

  if (QuotaManager::IsShuttingDown()) {
    IDB_REPORT_INTERNAL_ERR();
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  if (mClosed) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  if (RunningVersionChangeTransaction()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  if (aStoreNames.IsEmpty()) {
    aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
    return nullptr;
  }

  IDBTransaction::Mode mode;
  switch (aMode) {
    case IDBTransactionMode::Readonly:
      mode = IDBTransaction::READ_ONLY;
      break;
    case IDBTransactionMode::Readwrite:
      mode = IDBTransaction::READ_WRITE;
      break;
    case IDBTransactionMode::Versionchange:
      aRv.Throw(NS_ERROR_DOM_INVALID_ACCESS_ERR);
      return nullptr;
    default:
      MOZ_CRASH("Unknown mode!");
  }

  const nsTArray<ObjectStoreSpec>& objectStores = mSpec->objectStores();
  const uint32_t nameCount = aStoreNames.Length();

  nsTArray<nsString> sortedStoreNames;
  sortedStoreNames.SetCapacity(nameCount);

  // Check to make sure the object store names we collected actually exist.
  for (uint32_t nameIndex = 0; nameIndex < nameCount; nameIndex++) {
    const nsString& name = aStoreNames[nameIndex];

    bool found = false;

    for (uint32_t objCount = objectStores.Length(), objIndex = 0;
         objIndex < objCount;
         objIndex++) {
      if (objectStores[objIndex].metadata().name() == name) {
        found = true;
        break;
      }
    }

    if (!found) {
      aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR);
      return nullptr;
    }

    sortedStoreNames.InsertElementSorted(name);
  }

  // Remove any duplicates.
  for (uint32_t nameIndex = nameCount - 1; nameIndex > 0; nameIndex--) {
    if (sortedStoreNames[nameIndex] == sortedStoreNames[nameIndex - 1]) {
      sortedStoreNames.RemoveElementAt(nameIndex);
    }
  }

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, sortedStoreNames, mode);
  if (NS_WARN_IF(!transaction)) {
    IDB_REPORT_INTERNAL_ERR();
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  BackgroundTransactionChild* actor =
    new BackgroundTransactionChild(transaction);

  IDB_LOG_MARK("IndexedDB %s: Child  Transaction[%lld]: "
                 "database(%s).transaction(%s)",
               "IndexedDB %s: C T[%lld]: IDBDatabase.transaction()",
               IDB_LOG_ID_STRING(),
               transaction->LoggingSerialNumber(),
               IDB_LOG_STRINGIFY(this),
               IDB_LOG_STRINGIFY(transaction));

  MOZ_ALWAYS_TRUE(
    mBackgroundActor->SendPBackgroundIDBTransactionConstructor(actor,
                                                               sortedStoreNames,
                                                               mode));

  transaction->SetBackgroundActor(actor);

  return transaction.forget();
}

StorageType
IDBDatabase::Storage() const
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(mSpec);

  return PersistenceTypeToStorage(mSpec->metadata().persistenceType());
}

already_AddRefed<IDBRequest>
IDBDatabase::CreateMutableFile(const nsAString& aName,
                               const Optional<nsAString>& aType,
                               ErrorResult& aRv)
{
  if (!IndexedDatabaseManager::IsMainProcess() || !NS_IsMainThread()) {
    IDB_WARNING("Not supported!");
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  if (QuotaManager::IsShuttingDown()) {
    IDB_REPORT_INTERNAL_ERR();
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
    return nullptr;
  }

  if (mClosed) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR);
    return nullptr;
  }

  nsRefPtr<IDBRequest> request = IDBRequest::Create(this, nullptr);

  nsString type;
  if (aType.WasPassed()) {
    type = aType.Value();
  }

  mFactory->IncrementParentLoggingRequestSerialNumber();

  aRv = CreateFileHelper::CreateAndDispatch(this, request, aName, type);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return request.forget();
}

void
IDBDatabase::RegisterTransaction(IDBTransaction* aTransaction)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnOwningThread();
  MOZ_ASSERT(!mTransactions.Contains(aTransaction));

  mTransactions.PutEntry(aTransaction);
}

void
IDBDatabase::UnregisterTransaction(IDBTransaction* aTransaction)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aTransaction);
  aTransaction->AssertIsOnOwningThread();
  MOZ_ASSERT(mTransactions.Contains(aTransaction));

  mTransactions.RemoveEntry(aTransaction);
}

void
IDBDatabase::AbortTransactions(bool aShouldWarn)
{
  AssertIsOnOwningThread();

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static void
    AbortTransactions(nsTHashtable<nsPtrHashKey<IDBTransaction>>& aTable,
                      nsTArray<nsRefPtr<IDBTransaction>>& aAbortedTransactions)
    {
      const uint32_t count = aTable.Count();
      if (!count) {
        return;
      }

      nsAutoTArray<nsRefPtr<IDBTransaction>, 20> transactions;
      transactions.SetCapacity(count);

      aTable.EnumerateEntries(Collect, &transactions);

      MOZ_ASSERT(transactions.Length() == count);

      for (uint32_t index = 0; index < count; index++) {
        nsRefPtr<IDBTransaction> transaction = Move(transactions[index]);
        MOZ_ASSERT(transaction);

        transaction->Abort(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

        // We only care about warning for write transactions.
        if (transaction->GetMode() != IDBTransaction::READ_ONLY) {
          aAbortedTransactions.AppendElement(Move(transaction));
        }
      }
    }

  private:
    static PLDHashOperator
    Collect(nsPtrHashKey<IDBTransaction>* aTransaction, void* aClosure)
    {
      MOZ_ASSERT(aTransaction);
      aTransaction->GetKey()->AssertIsOnOwningThread();
      MOZ_ASSERT(aClosure);

      auto* array = static_cast<nsTArray<nsRefPtr<IDBTransaction>>*>(aClosure);
      array->AppendElement(aTransaction->GetKey());

      return PL_DHASH_NEXT;
    }
  };

  nsAutoTArray<nsRefPtr<IDBTransaction>, 5> abortedTransactions;
  Helper::AbortTransactions(mTransactions, abortedTransactions);

  if (aShouldWarn && !abortedTransactions.IsEmpty()) {
    static const char kWarningMessage[] = "IndexedDBTransactionAbortNavigation";

    for (uint32_t count = abortedTransactions.Length(), index = 0;
         index < count;
         index++) {
      nsRefPtr<IDBTransaction>& transaction = abortedTransactions[index];
      MOZ_ASSERT(transaction);

      nsString filename;
      uint32_t lineNo;
      transaction->GetCallerLocation(filename, &lineNo);

      LogWarning(kWarningMessage, filename, lineNo);
    }
  }
}

PBackgroundIDBDatabaseFileChild*
IDBDatabase::GetOrCreateFileActorForBlob(File* aBlob)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBlob);
  MOZ_ASSERT(mBackgroundActor);

  // We use the File's nsIWeakReference as the key to the table because
  // a) it is unique per blob, b) it is reference-counted so that we can
  // guarantee that it stays alive, and c) it doesn't hold the actual File
  // alive.
  nsCOMPtr<nsIDOMBlob> blob = aBlob;
  nsCOMPtr<nsIWeakReference> weakRef = do_GetWeakReference(blob);
  MOZ_ASSERT(weakRef);

  PBackgroundIDBDatabaseFileChild* actor = nullptr;

  if (!mFileActors.Get(weakRef, &actor)) {
    FileImpl* blobImpl = aBlob->Impl();
    MOZ_ASSERT(blobImpl);

    if (mReceivedBlobs.GetEntry(weakRef)) {
      // This blob was previously retrieved from the database.
      nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryObject(blobImpl);
      MOZ_ASSERT(remoteBlob);

      BlobChild* blobChild = remoteBlob->GetBlobChild();
      MOZ_ASSERT(blobChild);

#ifdef DEBUG
      {
        PBackgroundChild* backgroundManager = blobChild->GetBackgroundManager();
        MOZ_ASSERT(backgroundManager);

        PBackgroundChild* thisManager = mBackgroundActor->Manager()->Manager();
        MOZ_ASSERT(thisManager);

        MOZ_ASSERT(thisManager == backgroundManager);
      }
#endif
      auto* dbFile = new DatabaseFile(this);

      actor =
        mBackgroundActor->SendPBackgroundIDBDatabaseFileConstructor(dbFile,
                                                                    blobChild);
      if (NS_WARN_IF(!actor)) {
        return nullptr;
      }
    } else {
      // Make sure that the input stream we get here is one that can actually be
      // serialized to PBackground.
      PBackgroundChild* backgroundManager =
        mBackgroundActor->Manager()->Manager();
      MOZ_ASSERT(backgroundManager);

      auto* blobChild =
        static_cast<BlobChild*>(
          BackgroundChild::GetOrCreateActorForBlob(backgroundManager, aBlob));
      MOZ_ASSERT(blobChild);

      auto* dbFile = new DatabaseFile(this);

      actor =
        mBackgroundActor->SendPBackgroundIDBDatabaseFileConstructor(dbFile,
                                                                    blobChild);
      if (NS_WARN_IF(!actor)) {
        return nullptr;
      }
    }

    MOZ_ASSERT(actor);

    mFileActors.Put(weakRef, actor);
  }

  MOZ_ASSERT(actor);

  return actor;
}

void
IDBDatabase::NoteFinishedFileActor(PBackgroundIDBDatabaseFileChild* aFileActor)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aFileActor);

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static PLDHashOperator
    Remove(nsISupports* aKey,
           PBackgroundIDBDatabaseFileChild*& aValue,
           void* aClosure)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);
      MOZ_ASSERT(aClosure);

      if (aValue == static_cast<PBackgroundIDBDatabaseFileChild*>(aClosure)) {
        return PL_DHASH_REMOVE;
      }

      return PL_DHASH_NEXT;
    }
  };

  mFileActors.Enumerate(&Helper::Remove, aFileActor);
}

void
IDBDatabase::NoteReceivedBlob(File* aBlob)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aBlob);
  MOZ_ASSERT(mBackgroundActor);

#ifdef DEBUG
  {
    nsRefPtr<FileImpl> blobImpl = aBlob->Impl();
    MOZ_ASSERT(blobImpl);

    nsCOMPtr<nsIRemoteBlob> remoteBlob = do_QueryObject(blobImpl);
    MOZ_ASSERT(remoteBlob);

    BlobChild* blobChild = remoteBlob->GetBlobChild();
    MOZ_ASSERT(blobChild);

    PBackgroundChild* backgroundManager = blobChild->GetBackgroundManager();
    MOZ_ASSERT(backgroundManager);

    PBackgroundChild* thisManager = mBackgroundActor->Manager()->Manager();
    MOZ_ASSERT(thisManager);

    MOZ_ASSERT(thisManager == backgroundManager);
  }
#endif

  nsCOMPtr<nsIDOMBlob> blob = aBlob;
  nsCOMPtr<nsIWeakReference> weakRef = do_GetWeakReference(blob);
  MOZ_ASSERT(weakRef);

  // It's ok if this entry already exists in the table.
  mReceivedBlobs.PutEntry(weakRef);
}

void
IDBDatabase::DelayedMaybeExpireFileActors()
{
  AssertIsOnOwningThread();

  if (!mBackgroundActor || !mFileActors.Count()) {
    return;
  }

  nsCOMPtr<nsIRunnable> runnable =
    NS_NewRunnableMethodWithArg<bool>(this,
                                      &IDBDatabase::ExpireFileActors,
                                      /* aExpireAll */ false);
  MOZ_ASSERT(runnable);

  if (!NS_IsMainThread()) {
    // Wrap as a nsICancelableRunnable to make workers happy.
    nsCOMPtr<nsIRunnable> cancelable = new CancelableRunnableWrapper(runnable);
    cancelable.swap(runnable);
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToCurrentThread(runnable)));
}

nsresult
IDBDatabase::GetQuotaInfo(nsACString& aOrigin,
                          PersistenceType* aPersistenceType)
{
  using mozilla::dom::quota::QuotaManager;

  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (aPersistenceType) {
    *aPersistenceType = mSpec->metadata().persistenceType();
    MOZ_ASSERT(*aPersistenceType != PERSISTENCE_TYPE_INVALID);
  }

  PrincipalInfo* principalInfo = mFactory->GetPrincipalInfo();
  MOZ_ASSERT(principalInfo);

  switch (principalInfo->type()) {
    case PrincipalInfo::TNullPrincipalInfo:
      MOZ_CRASH("Is this needed?!");

    case PrincipalInfo::TSystemPrincipalInfo:
      QuotaManager::GetInfoForChrome(nullptr, &aOrigin, nullptr);
      return NS_OK;

    case PrincipalInfo::TContentPrincipalInfo: {
      nsresult rv;
      nsCOMPtr<nsIPrincipal> principal =
        PrincipalInfoToPrincipal(*principalInfo, &rv);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      rv = QuotaManager::GetInfoFromPrincipal(principal,
                                              nullptr,
                                              &aOrigin,
                                              nullptr);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }

      return NS_OK;
    }

    default:
      MOZ_CRASH("Unknown PrincipalInfo type!");
  }

  MOZ_CRASH("Should never get here!");
}

void
IDBDatabase::ExpireFileActors(bool aExpireAll)
{
  AssertIsOnOwningThread();

  class MOZ_STACK_CLASS Helper MOZ_FINAL
  {
  public:
    static PLDHashOperator
    MaybeExpireFileActors(nsISupports* aKey,
                          PBackgroundIDBDatabaseFileChild*& aValue,
                          void* aClosure)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(aValue);
      MOZ_ASSERT(aClosure);

      const bool expiringAll = *static_cast<bool*>(aClosure);

      bool shouldExpire = expiringAll;
      if (!shouldExpire) {
        nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(aKey);
        MOZ_ASSERT(weakRef);

        nsCOMPtr<nsISupports> referent = do_QueryReferent(weakRef);
        shouldExpire = !referent;
      }

      if (shouldExpire) {
        PBackgroundIDBDatabaseFileChild::Send__delete__(aValue);

        if (!expiringAll) {
          return PL_DHASH_REMOVE;
        }
      }

      return PL_DHASH_NEXT;
    }

    static PLDHashOperator
    MaybeExpireReceivedBlobs(nsISupportsHashKey* aKey,
                             void* aClosure)
    {
      MOZ_ASSERT(aKey);
      MOZ_ASSERT(!aClosure);

      nsISupports* key = aKey->GetKey();
      MOZ_ASSERT(key);

      nsCOMPtr<nsIWeakReference> weakRef = do_QueryInterface(key);
      MOZ_ASSERT(weakRef);

      nsCOMPtr<nsISupports> referent = do_QueryReferent(weakRef);
      if (!referent) {
        return PL_DHASH_REMOVE;
      }

      return PL_DHASH_NEXT;
    }
  };

  if (mBackgroundActor && mFileActors.Count()) {
    mFileActors.Enumerate(&Helper::MaybeExpireFileActors, &aExpireAll);
    if (aExpireAll) {
      mFileActors.Clear();
    }
  } else {
    MOZ_ASSERT(!mFileActors.Count());
  }

  if (mReceivedBlobs.Count()) {
    if (aExpireAll) {
      mReceivedBlobs.Clear();
    } else {
      mReceivedBlobs.EnumerateEntries(&Helper::MaybeExpireReceivedBlobs,
                                      nullptr);
    }
  }
}

void
IDBDatabase::NoteLiveMutableFile(IDBMutableFile* aMutableFile)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aMutableFile);
  MOZ_ASSERT(!mLiveMutableFiles.Contains(aMutableFile));

  mLiveMutableFiles.AppendElement(aMutableFile);
}

void
IDBDatabase::NoteFinishedMutableFile(IDBMutableFile* aMutableFile)
{
  // This should always happen in the main process but occasionally it is called
  // after the IndexedDatabaseManager has already shut down.
  //   MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aMutableFile);

  // It's ok if this is called more than once, so don't assert that aMutableFile
  // is in the list already.

  mLiveMutableFiles.RemoveElement(aMutableFile);
}

void
IDBDatabase::InvalidateMutableFiles()
{
  if (!mLiveMutableFiles.IsEmpty()) {
    MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
    MOZ_ASSERT(NS_IsMainThread());

    for (uint32_t count = mLiveMutableFiles.Length(), index = 0;
         index < count;
         index++) {
      mLiveMutableFiles[index]->Invalidate();
    }

    mLiveMutableFiles.Clear();
  }
}

void
IDBDatabase::Invalidate()
{
  AssertIsOnOwningThread();

  if (!mInvalidated) {
    mInvalidated = true;

    InvalidateInternal();
  }
}

void
IDBDatabase::LogWarning(const char* aMessageName,
                        const nsAString& aFilename,
                        uint32_t aLineNumber)
{
  AssertIsOnOwningThread();
  MOZ_ASSERT(aMessageName);

  if (NS_IsMainThread()) {
    LogWarningRunnable::LogWarning(aMessageName,
                                   aFilename,
                                   aLineNumber,
                                   mFactory->IsChrome(),
                                   mFactory->InnerWindowID());
  } else {
    nsRefPtr<LogWarningRunnable> runnable =
      new LogWarningRunnable(aMessageName,
                             aFilename,
                             aLineNumber,
                             mFactory->IsChrome(),
                             mFactory->InnerWindowID());
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(runnable)));
  }
}

NS_IMPL_ADDREF_INHERITED(IDBDatabase, IDBWrapperCache)
NS_IMPL_RELEASE_INHERITED(IDBDatabase, IDBWrapperCache)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBDatabase)
NS_INTERFACE_MAP_END_INHERITING(IDBWrapperCache)

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBDatabase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBDatabase, IDBWrapperCache)
  tmp->AssertIsOnOwningThread();
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFactory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBDatabase, IDBWrapperCache)
  tmp->AssertIsOnOwningThread();

  // Don't unlink mFactory!

  // We've been unlinked, at the very least we should be able to prevent further
  // transactions from starting and unblock any other SetVersion callers.
  tmp->CloseInternal();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

void
IDBDatabase::LastRelease()
{
  AssertIsOnOwningThread();

  CloseInternal();

  if (mBackgroundActor) {
    mBackgroundActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mBackgroundActor, "SendDeleteMeInternal should have cleared!");
  }
}

nsresult
IDBDatabase::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  nsresult rv =
    IndexedDatabaseManager::CommonPostHandleEvent(this, mFactory, aVisitor);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

JSObject*
IDBDatabase::WrapObject(JSContext* aCx)
{
  return IDBDatabaseBinding::Wrap(aCx, this);
}

NS_IMPL_ISUPPORTS(CancelableRunnableWrapper, nsIRunnable, nsICancelableRunnable)

NS_IMETHODIMP
CancelableRunnableWrapper::Run()
{
  nsCOMPtr<nsIRunnable> runnable;
  mRunnable.swap(runnable);

  if (runnable) {
    return runnable->Run();
  }

  return NS_OK;
}

NS_IMETHODIMP
CancelableRunnableWrapper::Cancel()
{
  if (mRunnable) {
    mRunnable = nullptr;
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

CreateFileHelper::CreateFileHelper(IDBDatabase* aDatabase,
                                   IDBRequest* aRequest,
                                   const nsAString& aName,
                                   const nsAString& aType,
                                   const nsACString& aOrigin)
  : mDatabase(aDatabase)
  , mRequest(aRequest)
  , mName(aName)
  , mType(aType)
  , mDatabaseName(aDatabase->Name())
  , mOrigin(aOrigin)
  , mPersistenceType(aDatabase->Spec()->metadata().persistenceType())
  , mResultCode(NS_OK)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDatabase);
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(mPersistenceType != PERSISTENCE_TYPE_INVALID);
}

// static
nsresult
CreateFileHelper::CreateAndDispatch(IDBDatabase* aDatabase,
                                    IDBRequest* aRequest,
                                    const nsAString& aName,
                                    const nsAString& aType)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aDatabase);
  aDatabase->AssertIsOnOwningThread();
  MOZ_ASSERT(aDatabase->Factory());
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(!QuotaManager::IsShuttingDown());

  nsCString origin;
  nsresult rv = aDatabase->GetQuotaInfo(origin, nullptr);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(!origin.IsEmpty());

  nsRefPtr<CreateFileHelper> helper =
    new CreateFileHelper(aDatabase, aRequest, aName, aType, origin);

  QuotaManager* quotaManager = QuotaManager::Get();
  MOZ_ASSERT(quotaManager);

  nsCOMPtr<nsIEventTarget> ioThread = quotaManager->IOThread();
  MOZ_ASSERT(ioThread);

  if (NS_WARN_IF(NS_FAILED(ioThread->Dispatch(helper, NS_DISPATCH_NORMAL)))) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  return NS_OK;
}

nsresult
CreateFileHelper::DoDatabaseWork()
{
  AssertIsOnIOThread();
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(!mFileInfo);

  PROFILER_LABEL("IndexedDB",
                 "CreateFileHelper::DoDatabaseWork",
                 js::ProfileEntry::Category::STORAGE);

  if (IndexedDatabaseManager::InLowDiskSpaceMode()) {
    NS_WARNING("Refusing to create file because disk space is low!");
    return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
  }

  IndexedDatabaseManager* idbManager = IndexedDatabaseManager::Get();
  MOZ_ASSERT(idbManager);

  nsRefPtr<FileManager> fileManager =
    idbManager->GetFileManager(mPersistenceType, mOrigin, mDatabaseName);
  MOZ_ASSERT(fileManager);

  nsRefPtr<FileInfo> fileInfo = fileManager->GetNewFileInfo();
  if (NS_WARN_IF(!fileInfo)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  const int64_t fileId = fileInfo->Id();

  nsCOMPtr<nsIFile> journalDirectory = fileManager->EnsureJournalDirectory();
  if (NS_WARN_IF(!journalDirectory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFile> journalFile =
    fileManager->GetFileForId(journalDirectory, fileId);
  if (NS_WARN_IF(!journalFile)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsresult rv = journalFile->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIFile> fileDirectory = fileManager->GetDirectory();
  if (NS_WARN_IF(!fileDirectory)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsCOMPtr<nsIFile> file = fileManager->GetFileForId(fileDirectory, fileId);
  if (NS_WARN_IF(!file)) {
    IDB_REPORT_INTERNAL_ERR();
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mFileInfo.swap(fileInfo);
  return NS_OK;
}

void
CreateFileHelper::DoMainThreadWork(nsresult aResultCode)
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());
  MOZ_ASSERT(NS_IsMainThread());

  if (mDatabase->IsInvalidated()) {
    IDB_REPORT_INTERNAL_ERR();
    aResultCode = NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsRefPtr<IDBMutableFile> mutableFile;
  if (NS_SUCCEEDED(aResultCode)) {
    mutableFile =
      IDBMutableFile::Create(mDatabase, mName, mType, mFileInfo.forget());
    MOZ_ASSERT(mutableFile);
  }

  DispatchMutableFileResult(mRequest, aResultCode, mutableFile);
}

NS_IMPL_ISUPPORTS_INHERITED0(CreateFileHelper, nsRunnable)

NS_IMETHODIMP
CreateFileHelper::Run()
{
  MOZ_ASSERT(IndexedDatabaseManager::IsMainProcess());

  if (NS_IsMainThread()) {
    DoMainThreadWork(mResultCode);

    mDatabase = nullptr;
    mRequest = nullptr;
    mFileInfo = nullptr;

    return NS_OK;
  }

  AssertIsOnIOThread();
  MOZ_ASSERT(NS_SUCCEEDED(mResultCode));

  nsresult rv = DoDatabaseWork();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    mResultCode = rv;
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(NS_DispatchToMainThread(this)));

  return NS_OK;
}


// static
void
IDBDatabase::
LogWarningRunnable::LogWarning(const char* aMessageName,
                               const nsAString& aFilename,
                               uint32_t aLineNumber,
                               bool aIsChrome,
                               uint64_t aInnerWindowID)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aMessageName);

  nsXPIDLString localizedMessage;
  if (NS_WARN_IF(NS_FAILED(
    nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                       aMessageName,
                                       localizedMessage)))) {
    return;
  }

  nsAutoCString category;
  if (aIsChrome) {
    category.AssignLiteral("chrome ");
  } else {
    category.AssignLiteral("content ");
  }
  category.AppendLiteral("javascript");

  nsCOMPtr<nsIConsoleService> consoleService =
    do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  MOZ_ASSERT(consoleService);

  nsCOMPtr<nsIScriptError> scriptError =
    do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  MOZ_ASSERT(consoleService);

  if (aInnerWindowID) {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      scriptError->InitWithWindowID(localizedMessage,
                                    aFilename,
                                    /* aSourceLine */ EmptyString(),
                                    aLineNumber,
                                    /* aColumnNumber */ 0,
                                    nsIScriptError::warningFlag,
                                    category,
                                    aInnerWindowID)));
  } else {
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
      scriptError->Init(localizedMessage,
                        aFilename,
                        /* aSourceLine */ EmptyString(),
                        aLineNumber,
                        /* aColumnNumber */ 0,
                        nsIScriptError::warningFlag,
                        category.get())));
  }

  MOZ_ALWAYS_TRUE(NS_SUCCEEDED(consoleService->LogMessage(scriptError)));
}

NS_IMPL_ISUPPORTS_INHERITED0(IDBDatabase::LogWarningRunnable, nsRunnable)

NS_IMETHODIMP
IDBDatabase::
LogWarningRunnable::Run()
{
  MOZ_ASSERT(NS_IsMainThread());

  LogWarning(mMessageName.get(),
             mFilename,
             mLineNumber,
             mIsChrome,
             mInnerWindowID);

  return NS_OK;
}

NS_IMPL_ISUPPORTS(IDBDatabase::Observer, nsIObserver)

NS_IMETHODIMP
IDBDatabase::
Observer::Observe(nsISupports* aSubject,
                  const char* aTopic,
                  const char16_t* aData)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTopic);

  if (!strcmp(aTopic, kWindowObserverTopic)) {
    if (mWeakDatabase) {
      nsCOMPtr<nsISupportsPRUint64> supportsInt = do_QueryInterface(aSubject);
      MOZ_ASSERT(supportsInt);

      uint64_t windowId;
      MOZ_ALWAYS_TRUE(NS_SUCCEEDED(supportsInt->GetData(&windowId)));

      if (windowId == mWindowId) {
        nsRefPtr<IDBDatabase> database = mWeakDatabase;
        mWeakDatabase = nullptr;

        database->InvalidateInternal();
      }
    }

    return NS_OK;
  }

  if (!strcmp(aTopic, kCycleCollectionObserverTopic) ||
      !strcmp(aTopic, kMemoryPressureObserverTopic)) {
    if (mWeakDatabase) {
      nsRefPtr<IDBDatabase> database = mWeakDatabase;

      database->ExpireFileActors(/* aExpireAll */ false);
    }

    return NS_OK;
  }

  NS_WARNING("Unknown observer topic!");
  return NS_OK;
}

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
