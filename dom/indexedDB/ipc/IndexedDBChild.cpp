/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IndexedDBChild.h"

#include "nsIInputStream.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/QuotaManager.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"

USING_INDEXEDDB_NAMESPACE

using namespace mozilla::dom;
using mozilla::dom::quota::Client;
using mozilla::dom::quota::QuotaManager;

namespace {

class IPCOpenDatabaseHelper : public AsyncConnectionHelper
{
public:
  IPCOpenDatabaseHelper(IDBDatabase* aDatabase, IDBOpenDBRequest* aRequest)
  : AsyncConnectionHelper(aDatabase, aRequest)
  {
    MOZ_ASSERT(aRequest);
  }

  virtual nsresult UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
                                            MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  GetSuccessResult(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual nsresult
  OnSuccess() MOZ_OVERRIDE
  {
    static_cast<IDBOpenDBRequest*>(mRequest.get())->SetTransaction(nullptr);
    return AsyncConnectionHelper::OnSuccess();
  }

  virtual void
  OnError() MOZ_OVERRIDE
  {
    static_cast<IDBOpenDBRequest*>(mRequest.get())->SetTransaction(nullptr);
    AsyncConnectionHelper::OnError();
  }

  virtual nsresult
  DoDatabaseWork(mozIStorageConnection* aConnection) MOZ_OVERRIDE;
};

class IPCSetVersionHelper : public AsyncConnectionHelper
{
  nsRefPtr<IDBOpenDBRequest> mOpenRequest;
  uint64_t mOldVersion;
  uint64_t mRequestedVersion;

public:
  IPCSetVersionHelper(IDBTransaction* aTransaction, IDBOpenDBRequest* aRequest,
                      uint64_t aOldVersion, uint64_t aRequestedVersion)
  : AsyncConnectionHelper(aTransaction, aRequest),
    mOpenRequest(aRequest), mOldVersion(aOldVersion),
    mRequestedVersion(aRequestedVersion)
  {
    MOZ_ASSERT(aTransaction);
    MOZ_ASSERT(aRequest);
  }

  virtual nsresult UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
                                            MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  DoDatabaseWork(mozIStorageConnection* aConnection) MOZ_OVERRIDE;

  virtual already_AddRefed<nsIDOMEvent>
  CreateSuccessEvent(mozilla::dom::EventTarget* aOwner) MOZ_OVERRIDE;

  virtual nsresult
  GetSuccessResult(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;
};

class IPCDeleteDatabaseHelper : public AsyncConnectionHelper
{
public:
  IPCDeleteDatabaseHelper(IDBRequest* aRequest)
  : AsyncConnectionHelper(static_cast<IDBDatabase*>(nullptr), aRequest)
  { }

  virtual nsresult UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
                                            MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  GetSuccessResult(JSContext* aCx, JS::MutableHandle<JS::Value> aVal) MOZ_OVERRIDE;

  virtual nsresult
  DoDatabaseWork(mozIStorageConnection* aConnection) MOZ_OVERRIDE;
};

class VersionChangeRunnable : public nsRunnable
{
  nsRefPtr<IDBDatabase> mDatabase;
  uint64_t mOldVersion;
  uint64_t mNewVersion;

public:
  VersionChangeRunnable(IDBDatabase* aDatabase, const uint64_t& aOldVersion,
                        const uint64_t& aNewVersion)
  : mDatabase(aDatabase), mOldVersion(aOldVersion), mNewVersion(aNewVersion)
  {
    MOZ_ASSERT(aDatabase);
  }

  NS_IMETHOD Run() MOZ_OVERRIDE
  {
    if (mDatabase->IsClosed()) {
      return NS_OK;
    }

    nsRefPtr<Event> event =
      IDBVersionChangeEvent::Create(mDatabase, mOldVersion, mNewVersion);
    MOZ_ASSERT(event);

    bool dummy;
    nsresult rv = mDatabase->DispatchEvent(event, &dummy);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }
};

} // anonymous namespace

/*******************************************************************************
 * IndexedDBChild
 ******************************************************************************/

IndexedDBChild::IndexedDBChild(ContentChild* aContentChild,
                               const nsCString& aASCIIOrigin)
: mFactory(nullptr)
, mManagerContent(aContentChild)
, mManagerTab(nullptr)
, mASCIIOrigin(aASCIIOrigin)
#ifdef DEBUG
, mDisconnected(false)
#endif
{
  MOZ_ASSERT(aContentChild);
  MOZ_COUNT_CTOR(IndexedDBChild);
}

IndexedDBChild::IndexedDBChild(TabChild* aTabChild,
                               const nsCString& aASCIIOrigin)
: mFactory(nullptr)
, mManagerContent(nullptr)
, mManagerTab(aTabChild)
, mASCIIOrigin(aASCIIOrigin)
#ifdef DEBUG
, mDisconnected(false)
#endif
{
  MOZ_ASSERT(aTabChild);
  MOZ_COUNT_CTOR(IndexedDBChild);
}

IndexedDBChild::~IndexedDBChild()
{
  MOZ_COUNT_DTOR(IndexedDBChild);
  MOZ_ASSERT(!mFactory);
}

void
IndexedDBChild::SetFactory(IDBFactory* aFactory)
{
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(!mFactory);

  aFactory->SetActor(this);
  mFactory = aFactory;
}

void
IndexedDBChild::Disconnect()
{
#ifdef DEBUG
  MOZ_ASSERT(!mDisconnected);
  mDisconnected = true;
#endif

  const InfallibleTArray<PIndexedDBDatabaseChild*>& databases =
    ManagedPIndexedDBDatabaseChild();
  for (uint32_t i = 0; i < databases.Length(); ++i) {
    static_cast<IndexedDBDatabaseChild*>(databases[i])->Disconnect();
  }
}

void
IndexedDBChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mFactory) {
    mFactory->SetActor(static_cast<IndexedDBChild*>(nullptr));
#ifdef DEBUG
    mFactory = nullptr;
#endif
  }
}

PIndexedDBDatabaseChild*
IndexedDBChild::AllocPIndexedDBDatabaseChild(
                                        const nsString& aName,
                                        const uint64_t& aVersion,
                                        const PersistenceType& aPersistenceType)
{
  return new IndexedDBDatabaseChild(aName, aVersion);
}

bool
IndexedDBChild::DeallocPIndexedDBDatabaseChild(PIndexedDBDatabaseChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBDeleteDatabaseRequestChild*
IndexedDBChild::AllocPIndexedDBDeleteDatabaseRequestChild(
                                        const nsString& aName,
                                        const PersistenceType& aPersistenceType)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
IndexedDBChild::DeallocPIndexedDBDeleteDatabaseRequestChild(
                                   PIndexedDBDeleteDatabaseRequestChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBDatabaseChild
 ******************************************************************************/

IndexedDBDatabaseChild::IndexedDBDatabaseChild(const nsString& aName,
                                               uint64_t aVersion)
: mDatabase(nullptr), mName(aName), mVersion(aVersion)
{
  MOZ_COUNT_CTOR(IndexedDBDatabaseChild);
}

IndexedDBDatabaseChild::~IndexedDBDatabaseChild()
{
  MOZ_COUNT_DTOR(IndexedDBDatabaseChild);
  MOZ_ASSERT(!mDatabase);
  MOZ_ASSERT(!mStrongDatabase);
}

void
IndexedDBDatabaseChild::SetRequest(IDBOpenDBRequest* aRequest)
{
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(!mRequest);

  mRequest = aRequest;
}

void
IndexedDBDatabaseChild::Disconnect()
{
  const InfallibleTArray<PIndexedDBTransactionChild*>& transactions =
    ManagedPIndexedDBTransactionChild();
  for (uint32_t i = 0; i < transactions.Length(); ++i) {
    static_cast<IndexedDBTransactionChild*>(transactions[i])->Disconnect();
  }
}

bool
IndexedDBDatabaseChild::EnsureDatabase(
                           IDBOpenDBRequest* aRequest,
                           const DatabaseInfoGuts& aDBInfo,
                           const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo)
{
  nsCString databaseId;
  if (mDatabase) {
    databaseId = mDatabase->Id();
  }
  else {
    QuotaManager::GetStorageId(aDBInfo.persistenceType, aDBInfo.origin,
                               Client::IDB, aDBInfo.name, databaseId);
  }
  MOZ_ASSERT(!databaseId.IsEmpty());

  nsRefPtr<DatabaseInfo> dbInfo;
  if (DatabaseInfo::Get(databaseId, getter_AddRefs(dbInfo))) {
    dbInfo->version = aDBInfo.version;
  }
  else {
    nsRefPtr<DatabaseInfo> newInfo = new DatabaseInfo();

    *static_cast<DatabaseInfoGuts*>(newInfo.get()) = aDBInfo;
    newInfo->id = databaseId;

    if (!DatabaseInfo::Put(newInfo)) {
      NS_WARNING("Out of memory!");
      return false;
    }

    newInfo.swap(dbInfo);

    // This is more or less copied from IDBFactory::SetDatabaseMetadata.
    for (uint32_t i = 0; i < aOSInfo.Length(); i++) {
      nsRefPtr<ObjectStoreInfo> newInfo = new ObjectStoreInfo();
      *static_cast<ObjectStoreInfoGuts*>(newInfo.get()) = aOSInfo[i];

      if (!dbInfo->PutObjectStore(newInfo)) {
        NS_WARNING("Out of memory!");
        return false;
      }
    }
  }

  if (!mDatabase) {
    nsRefPtr<IDBDatabase> database =
      IDBDatabase::Create(aRequest, aRequest->Factory(), dbInfo.forget(),
                          aDBInfo.origin, nullptr, nullptr);
    if (!database) {
      NS_WARNING("Failed to create database!");
      return false;
    }

    database->SetActor(this);

    mDatabase = database;
    mStrongDatabase = database.forget();
  }

  return true;
}

void
IndexedDBDatabaseChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mDatabase) {
    mDatabase->SetActor(static_cast<IndexedDBDatabaseChild*>(nullptr));
#ifdef DEBUG
    mDatabase = nullptr;
#endif
  }
}

bool
IndexedDBDatabaseChild::RecvSuccess(
                           const DatabaseInfoGuts& aDBInfo,
                           const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo)
{
#ifdef DEBUG
  {
    IndexedDBChild* manager = static_cast<IndexedDBChild*>(Manager());
    MOZ_ASSERT(aDBInfo.origin == manager->ASCIIOrigin());
    MOZ_ASSERT(aDBInfo.name == mName);
    MOZ_ASSERT(!mVersion || aDBInfo.version == mVersion);
  }
#endif

  MOZ_ASSERT(mRequest);

  nsRefPtr<IDBOpenDBRequest> request;
  mRequest.swap(request);

  nsRefPtr<AsyncConnectionHelper> openHelper;
  mOpenHelper.swap(openHelper);

  if (!EnsureDatabase(request, aDBInfo, aOSInfo)) {
    return false;
  }

  MOZ_ASSERT(mStrongDatabase);
  nsRefPtr<IDBDatabase> database;
  mStrongDatabase.swap(database);

  if (openHelper) {
    request->Reset();
  }
  else {
    openHelper = new IPCOpenDatabaseHelper(mDatabase, request);
  }

  ImmediateRunEventTarget target;
  if (NS_FAILED(openHelper->Dispatch(&target))) {
    NS_WARNING("Dispatch of IPCOpenDatabaseHelper failed!");
    return false;
  }

  return true;
}

bool
IndexedDBDatabaseChild::RecvError(const nsresult& aRv)
{
  MOZ_ASSERT(mRequest);

  nsRefPtr<IDBOpenDBRequest> request;
  mRequest.swap(request);

  nsRefPtr<IDBDatabase> database;
  mStrongDatabase.swap(database);

  nsRefPtr<AsyncConnectionHelper> openHelper;
  mOpenHelper.swap(openHelper);

  if (openHelper) {
    request->Reset();
  }
  else {
    openHelper = new IPCOpenDatabaseHelper(nullptr, request);
  }

  openHelper->SetError(aRv);

  ImmediateRunEventTarget target;
  if (NS_FAILED(openHelper->Dispatch(&target))) {
    NS_WARNING("Dispatch of IPCOpenDatabaseHelper failed!");
    return false;
  }

  return true;
}

bool
IndexedDBDatabaseChild::RecvBlocked(const uint64_t& aOldVersion)
{
  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(!mDatabase);

  nsCOMPtr<nsIRunnable> runnable =
    IDBVersionChangeEvent::CreateBlockedRunnable(mRequest, aOldVersion, mVersion);

  ImmediateRunEventTarget target;
  if (NS_FAILED(target.Dispatch(runnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Dispatch of blocked event failed!");
  }

  return true;
}

bool
IndexedDBDatabaseChild::RecvVersionChange(const uint64_t& aOldVersion,
                                          const uint64_t& aNewVersion)
{
  MOZ_ASSERT(mDatabase);

  nsCOMPtr<nsIRunnable> runnable =
    new VersionChangeRunnable(mDatabase, aOldVersion, aNewVersion);

  ImmediateRunEventTarget target;
  if (NS_FAILED(target.Dispatch(runnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Dispatch of versionchange event failed!");
  }

  return true;
}

bool
IndexedDBDatabaseChild::RecvInvalidate()
{
  if (mDatabase) {
    mDatabase->Invalidate();
  }
  return true;
}

bool
IndexedDBDatabaseChild::RecvPIndexedDBTransactionConstructor(
                                             PIndexedDBTransactionChild* aActor,
                                             const TransactionParams& aParams)
{
  // This only happens when the parent has created a version-change transaction
  // for us.

  IndexedDBTransactionChild* actor =
    static_cast<IndexedDBTransactionChild*>(aActor);
  MOZ_ASSERT(!actor->GetTransaction());

  MOZ_ASSERT(aParams.type() ==
             TransactionParams::TVersionChangeTransactionParams);

  const VersionChangeTransactionParams& params =
    aParams.get_VersionChangeTransactionParams();

  const DatabaseInfoGuts& dbInfo = params.dbInfo();
  const InfallibleTArray<ObjectStoreInfoGuts>& osInfo = params.osInfo();
  uint64_t oldVersion = params.oldVersion();

  MOZ_ASSERT(dbInfo.origin ==
             static_cast<IndexedDBChild*>(Manager())->ASCIIOrigin());
  MOZ_ASSERT(dbInfo.name == mName);
  MOZ_ASSERT(!mVersion || dbInfo.version == mVersion);
  MOZ_ASSERT(!mVersion || oldVersion < mVersion);

  MOZ_ASSERT(mRequest);
  MOZ_ASSERT(!mDatabase);
  MOZ_ASSERT(!mOpenHelper);

  if (!EnsureDatabase(mRequest, dbInfo, osInfo)) {
    return false;
  }

  nsRefPtr<IPCOpenDatabaseHelper> helper =
    new IPCOpenDatabaseHelper(mDatabase, mRequest);

  Sequence<nsString> storesToOpen;
  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::CreateInternal(mDatabase, storesToOpen,
                                   IDBTransaction::VERSION_CHANGE, false, true);
  NS_ENSURE_TRUE(transaction, false);

  nsRefPtr<IPCSetVersionHelper> versionHelper =
    new IPCSetVersionHelper(transaction, mRequest, oldVersion, mVersion);

  mDatabase->EnterSetVersionTransaction();
  mDatabase->mPreviousDatabaseInfo->version = oldVersion;

  actor->SetTransaction(transaction);

  ImmediateRunEventTarget target;
  if (NS_FAILED(versionHelper->Dispatch(&target))) {
    NS_WARNING("Dispatch of IPCSetVersionHelper failed!");
    return false;
  }

  mOpenHelper = helper.forget();
  return true;
}

PIndexedDBTransactionChild*
IndexedDBDatabaseChild::AllocPIndexedDBTransactionChild(
                                               const TransactionParams& aParams)
{
  MOZ_ASSERT(aParams.type() ==
             TransactionParams::TVersionChangeTransactionParams);
  return new IndexedDBTransactionChild();
}

bool
IndexedDBDatabaseChild::DeallocPIndexedDBTransactionChild(
                                             PIndexedDBTransactionChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBTransactionChild
 ******************************************************************************/

IndexedDBTransactionChild::IndexedDBTransactionChild()
: mTransaction(nullptr)
{
  MOZ_COUNT_CTOR(IndexedDBTransactionChild);
}

IndexedDBTransactionChild::~IndexedDBTransactionChild()
{
  MOZ_COUNT_DTOR(IndexedDBTransactionChild);
  MOZ_ASSERT(!mTransaction);
  MOZ_ASSERT(!mStrongTransaction);
}

void
IndexedDBTransactionChild::SetTransaction(IDBTransaction* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(!mTransaction);

  aTransaction->SetActor(this);

  mTransaction = aTransaction;
  mStrongTransaction = aTransaction;
}

void
IndexedDBTransactionChild::Disconnect()
{
  const InfallibleTArray<PIndexedDBObjectStoreChild*>& objectStores =
    ManagedPIndexedDBObjectStoreChild();
  for (uint32_t i = 0; i < objectStores.Length(); ++i) {
    static_cast<IndexedDBObjectStoreChild*>(objectStores[i])->Disconnect();
  }
}

void
IndexedDBTransactionChild::FireCompleteEvent(nsresult aRv)
{
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mStrongTransaction);

  nsRefPtr<IDBTransaction> transaction;
  mStrongTransaction.swap(transaction);

  if (transaction->GetMode() == IDBTransaction::VERSION_CHANGE) {
    transaction->Database()->ExitSetVersionTransaction();
  }

  nsRefPtr<CommitHelper> helper = new CommitHelper(transaction, aRv);

  ImmediateRunEventTarget target;
  if (NS_FAILED(target.Dispatch(helper, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Dispatch of CommitHelper failed!");
  }
}

void
IndexedDBTransactionChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mStrongTransaction) {
    // We're being torn down before we received a complete event from the parent
    // so fake one here.
    FireCompleteEvent(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

    MOZ_ASSERT(!mStrongTransaction);
  }

  if (mTransaction) {
    mTransaction->SetActor(static_cast<IndexedDBTransactionChild*>(nullptr));
#ifdef DEBUG
    mTransaction = nullptr;
#endif
  }
}

bool
IndexedDBTransactionChild::RecvComplete(const CompleteParams& aParams)
{
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mStrongTransaction);

  nsresult resultCode;

  switch (aParams.type()) {
    case CompleteParams::TCompleteResult:
      resultCode = NS_OK;
      break;
    case CompleteParams::TAbortResult:
      resultCode = aParams.get_AbortResult().errorCode();
      if (NS_SUCCEEDED(resultCode)) {
        resultCode = NS_ERROR_DOM_INDEXEDDB_ABORT_ERR;
      }
      break;

    default:
      MOZ_CRASH("Unknown union type!");
  }

  FireCompleteEvent(resultCode);
  return true;
}

PIndexedDBObjectStoreChild*
IndexedDBTransactionChild::AllocPIndexedDBObjectStoreChild(
                                    const ObjectStoreConstructorParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct an object store!");
}

bool
IndexedDBTransactionChild::DeallocPIndexedDBObjectStoreChild(
                                             PIndexedDBObjectStoreChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBObjectStoreChild
 ******************************************************************************/

IndexedDBObjectStoreChild::IndexedDBObjectStoreChild(
                                                   IDBObjectStore* aObjectStore)
: mObjectStore(aObjectStore)
{
  MOZ_COUNT_CTOR(IndexedDBObjectStoreChild);
  aObjectStore->SetActor(this);
}

IndexedDBObjectStoreChild::~IndexedDBObjectStoreChild()
{
  MOZ_COUNT_DTOR(IndexedDBObjectStoreChild);
  MOZ_ASSERT(!mObjectStore);
}

void
IndexedDBObjectStoreChild::Disconnect()
{
  const InfallibleTArray<PIndexedDBRequestChild*>& requests =
    ManagedPIndexedDBRequestChild();
  for (uint32_t i = 0; i < requests.Length(); ++i) {
    static_cast<IndexedDBRequestChildBase*>(requests[i])->Disconnect();
  }

  const InfallibleTArray<PIndexedDBIndexChild*>& indexes =
    ManagedPIndexedDBIndexChild();
  for (uint32_t i = 0; i < indexes.Length(); ++i) {
    static_cast<IndexedDBIndexChild*>(indexes[i])->Disconnect();
  }

  const InfallibleTArray<PIndexedDBCursorChild*>& cursors =
    ManagedPIndexedDBCursorChild();
  for (uint32_t i = 0; i < cursors.Length(); ++i) {
    static_cast<IndexedDBCursorChild*>(cursors[i])->Disconnect();
  }
}

void
IndexedDBObjectStoreChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mObjectStore) {
    mObjectStore->SetActor(static_cast<IndexedDBObjectStoreChild*>(nullptr));
#ifdef DEBUG
    mObjectStore = nullptr;
#endif
  }
}

bool
IndexedDBObjectStoreChild::RecvPIndexedDBCursorConstructor(
                              PIndexedDBCursorChild* aActor,
                              const ObjectStoreCursorConstructorParams& aParams)
{
  IndexedDBCursorChild* actor = static_cast<IndexedDBCursorChild*>(aActor);

  IndexedDBObjectStoreRequestChild* requestActor =
    static_cast<IndexedDBObjectStoreRequestChild*>(aParams.requestChild());
  NS_ASSERTION(requestActor, "Must have an actor here!");

  nsRefPtr<IDBRequest> request = requestActor->GetRequest();
  NS_ASSERTION(request, "Must have a request here!");

  size_t direction = static_cast<size_t>(aParams.direction());

  nsRefPtr<IDBCursor> cursor;
  nsresult rv;

  typedef ipc::OptionalStructuredCloneReadInfo CursorUnionType;

  switch (aParams.optionalCloneInfo().type()) {
    case CursorUnionType::TSerializedStructuredCloneReadInfo: {
      nsTArray<StructuredCloneFile> blobs;
      IDBObjectStore::ConvertActorsToBlobs(aParams.blobsChild(), blobs);

      const SerializedStructuredCloneReadInfo& cloneInfo =
        aParams.optionalCloneInfo().get_SerializedStructuredCloneReadInfo();

      rv = mObjectStore->OpenCursorFromChildProcess(request, direction,
                                                    aParams.key(), cloneInfo,
                                                    blobs,
                                                    getter_AddRefs(cursor));
      NS_ENSURE_SUCCESS(rv, false);

      MOZ_ASSERT(blobs.IsEmpty(), "Should have swapped blob elements!");
    } break;

    case CursorUnionType::Tvoid_t:
      MOZ_ASSERT(aParams.blobsChild().IsEmpty());

      rv = mObjectStore->OpenCursorFromChildProcess(request, direction,
                                                    aParams.key(),
                                                    getter_AddRefs(cursor));
      NS_ENSURE_SUCCESS(rv, false);
      break;

    default:
      MOZ_CRASH("Unknown union type!");
  }

  actor->SetCursor(cursor);
  return true;
}

PIndexedDBRequestChild*
IndexedDBObjectStoreChild::AllocPIndexedDBRequestChild(
                                        const ObjectStoreRequestParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
IndexedDBObjectStoreChild::DeallocPIndexedDBRequestChild(
                                                 PIndexedDBRequestChild* aActor)
{
  delete aActor;
  return false;
}

PIndexedDBIndexChild*
IndexedDBObjectStoreChild::AllocPIndexedDBIndexChild(
                                          const IndexConstructorParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct an index!");
}

bool
IndexedDBObjectStoreChild::DeallocPIndexedDBIndexChild(PIndexedDBIndexChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBCursorChild*
IndexedDBObjectStoreChild::AllocPIndexedDBCursorChild(
                              const ObjectStoreCursorConstructorParams& aParams)
{
  return new IndexedDBCursorChild();
}

bool
IndexedDBObjectStoreChild::DeallocPIndexedDBCursorChild(
                                                  PIndexedDBCursorChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBIndexChild
 ******************************************************************************/

IndexedDBIndexChild::IndexedDBIndexChild(IDBIndex* aIndex)
: mIndex(aIndex)
{
  MOZ_COUNT_CTOR(IndexedDBIndexChild);
  aIndex->SetActor(this);
}

IndexedDBIndexChild::~IndexedDBIndexChild()
{
  MOZ_COUNT_DTOR(IndexedDBIndexChild);
  MOZ_ASSERT(!mIndex);
}

void
IndexedDBIndexChild::Disconnect()
{
  const InfallibleTArray<PIndexedDBRequestChild*>& requests =
    ManagedPIndexedDBRequestChild();
  for (uint32_t i = 0; i < requests.Length(); ++i) {
    static_cast<IndexedDBRequestChildBase*>(requests[i])->Disconnect();
  }

  const InfallibleTArray<PIndexedDBCursorChild*>& cursors =
    ManagedPIndexedDBCursorChild();
  for (uint32_t i = 0; i < cursors.Length(); ++i) {
    static_cast<IndexedDBCursorChild*>(cursors[i])->Disconnect();
  }
}

void
IndexedDBIndexChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mIndex) {
    mIndex->SetActor(static_cast<IndexedDBIndexChild*>(nullptr));
#ifdef DEBUG
    mIndex = nullptr;
#endif
  }
}

bool
IndexedDBIndexChild::RecvPIndexedDBCursorConstructor(
                                    PIndexedDBCursorChild* aActor,
                                    const IndexCursorConstructorParams& aParams)
{
  IndexedDBCursorChild* actor = static_cast<IndexedDBCursorChild*>(aActor);

  IndexedDBObjectStoreRequestChild* requestActor =
    static_cast<IndexedDBObjectStoreRequestChild*>(aParams.requestChild());
  NS_ASSERTION(requestActor, "Must have an actor here!");

  nsRefPtr<IDBRequest> request = requestActor->GetRequest();
  NS_ASSERTION(request, "Must have a request here!");

  size_t direction = static_cast<size_t>(aParams.direction());

  nsRefPtr<IDBCursor> cursor;
  nsresult rv;

  typedef ipc::OptionalStructuredCloneReadInfo CursorUnionType;

  switch (aParams.optionalCloneInfo().type()) {
    case CursorUnionType::TSerializedStructuredCloneReadInfo: {
      nsTArray<StructuredCloneFile> blobs;
      IDBObjectStore::ConvertActorsToBlobs(aParams.blobsChild(), blobs);

      const SerializedStructuredCloneReadInfo& cloneInfo =
        aParams.optionalCloneInfo().get_SerializedStructuredCloneReadInfo();

      rv = mIndex->OpenCursorFromChildProcess(request, direction, aParams.key(),
                                              aParams.objectKey(), cloneInfo,
                                              blobs,
                                              getter_AddRefs(cursor));
      NS_ENSURE_SUCCESS(rv, false);
    } break;

    case CursorUnionType::Tvoid_t:
      MOZ_ASSERT(aParams.blobsChild().IsEmpty());

      rv = mIndex->OpenCursorFromChildProcess(request, direction, aParams.key(),
                                              aParams.objectKey(),
                                              getter_AddRefs(cursor));
      NS_ENSURE_SUCCESS(rv, false);
      break;

    default:
      MOZ_CRASH("Unknown union type!");
  }

  actor->SetCursor(cursor);
  return true;
}

PIndexedDBRequestChild*
IndexedDBIndexChild::AllocPIndexedDBRequestChild(const IndexRequestParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
IndexedDBIndexChild::DeallocPIndexedDBRequestChild(PIndexedDBRequestChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBCursorChild*
IndexedDBIndexChild::AllocPIndexedDBCursorChild(
                                    const IndexCursorConstructorParams& aParams)
{
  return new IndexedDBCursorChild();
}

bool
IndexedDBIndexChild::DeallocPIndexedDBCursorChild(PIndexedDBCursorChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBCursorChild
 ******************************************************************************/

IndexedDBCursorChild::IndexedDBCursorChild()
: mCursor(nullptr)
{
  MOZ_COUNT_CTOR(IndexedDBCursorChild);
}

IndexedDBCursorChild::~IndexedDBCursorChild()
{
  MOZ_COUNT_DTOR(IndexedDBCursorChild);
  MOZ_ASSERT(!mCursor);
  MOZ_ASSERT(!mStrongCursor);
}

void
IndexedDBCursorChild::SetCursor(IDBCursor* aCursor)
{
  MOZ_ASSERT(aCursor);
  MOZ_ASSERT(!mCursor);

  aCursor->SetActor(this);

  mCursor = aCursor;
  mStrongCursor = aCursor;
}

void
IndexedDBCursorChild::Disconnect()
{
  const InfallibleTArray<PIndexedDBRequestChild*>& requests =
    ManagedPIndexedDBRequestChild();
  for (uint32_t i = 0; i < requests.Length(); ++i) {
    static_cast<IndexedDBRequestChildBase*>(requests[i])->Disconnect();
  }
}

void
IndexedDBCursorChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mCursor) {
    mCursor->SetActor(static_cast<IndexedDBCursorChild*>(nullptr));
#ifdef DEBUG
    mCursor = nullptr;
#endif
  }
}

PIndexedDBRequestChild*
IndexedDBCursorChild::AllocPIndexedDBRequestChild(const CursorRequestParams& aParams)
{
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool
IndexedDBCursorChild::DeallocPIndexedDBRequestChild(PIndexedDBRequestChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBRequestChildBase
 ******************************************************************************/

IndexedDBRequestChildBase::IndexedDBRequestChildBase(
                                                 AsyncConnectionHelper* aHelper)
: mHelper(aHelper)
{
  MOZ_COUNT_CTOR(IndexedDBRequestChildBase);
}

IndexedDBRequestChildBase::~IndexedDBRequestChildBase()
{
  MOZ_COUNT_DTOR(IndexedDBRequestChildBase);
}

IDBRequest*
IndexedDBRequestChildBase::GetRequest() const
{
  return mHelper ? mHelper->GetRequest() : nullptr;
}

void
IndexedDBRequestChildBase::Disconnect()
{
  if (mHelper) {
    IDBRequest* request = mHelper->GetRequest();

    if (request->IsPending()) {
      request->SetError(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      IDBTransaction* transaction = mHelper->GetTransaction();
      if (transaction) {
        transaction->OnRequestDisconnected();
      }
    }
  }
}

bool
IndexedDBRequestChildBase::Recv__delete__(const ResponseValue& aResponse)
{
  MOZ_CRASH("This should be overridden!");
}

/*******************************************************************************
 * IndexedDBObjectStoreRequestChild
 ******************************************************************************/

IndexedDBObjectStoreRequestChild::IndexedDBObjectStoreRequestChild(
                                                 AsyncConnectionHelper* aHelper,
                                                 IDBObjectStore* aObjectStore,
                                                 RequestType aRequestType)
: IndexedDBRequestChildBase(aHelper), mObjectStore(aObjectStore),
  mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBObjectStoreRequestChild);
  MOZ_ASSERT(aHelper);
  MOZ_ASSERT(aObjectStore);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBObjectStoreRequestChild::~IndexedDBObjectStoreRequestChild()
{
  MOZ_COUNT_DTOR(IndexedDBObjectStoreRequestChild);
}

bool
IndexedDBObjectStoreRequestChild::Recv__delete__(const ResponseValue& aResponse)
{
  switch (aResponse.type()) {
    case ResponseValue::Tnsresult:
      break;
    case ResponseValue::TGetResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetParams);
      break;
    case ResponseValue::TGetAllResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllParams);
      break;
    case ResponseValue::TGetAllKeysResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllKeysParams);
      break;
    case ResponseValue::TAddResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TAddParams);
      break;
    case ResponseValue::TPutResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TPutParams);
      break;
    case ResponseValue::TDeleteResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TDeleteParams);
      break;
    case ResponseValue::TClearResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TClearParams);
      break;
    case ResponseValue::TCountResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TCountParams);
      break;
    case ResponseValue::TOpenCursorResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenCursorParams ||
                 mRequestType == ParamsUnionType::TOpenKeyCursorParams);
      break;

    default:
      MOZ_CRASH("Received invalid response parameters!");
  }

  nsresult rv = mHelper->OnParentProcessRequestComplete(aResponse);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

/*******************************************************************************
 * IndexedDBIndexRequestChild
 ******************************************************************************/

IndexedDBIndexRequestChild::IndexedDBIndexRequestChild(
                                                 AsyncConnectionHelper* aHelper,
                                                 IDBIndex* aIndex,
                                                 RequestType aRequestType)
: IndexedDBRequestChildBase(aHelper), mIndex(aIndex), mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBIndexRequestChild);
  MOZ_ASSERT(aHelper);
  MOZ_ASSERT(aIndex);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBIndexRequestChild::~IndexedDBIndexRequestChild()
{
  MOZ_COUNT_DTOR(IndexedDBIndexRequestChild);
}

bool
IndexedDBIndexRequestChild::Recv__delete__(const ResponseValue& aResponse)
{
  switch (aResponse.type()) {
    case ResponseValue::Tnsresult:
      break;
    case ResponseValue::TGetResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetParams);
      break;
    case ResponseValue::TGetKeyResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetKeyParams);
      break;
    case ResponseValue::TGetAllResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllParams);
      break;
    case ResponseValue::TGetAllKeysResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllKeysParams);
      break;
    case ResponseValue::TCountResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TCountParams);
      break;
    case ResponseValue::TOpenCursorResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenCursorParams ||
                 mRequestType == ParamsUnionType::TOpenKeyCursorParams);
      break;

    default:
      MOZ_CRASH("Received invalid response parameters!");
  }

  nsresult rv = mHelper->OnParentProcessRequestComplete(aResponse);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

/*******************************************************************************
 * IndexedDBCursorRequestChild
 ******************************************************************************/

IndexedDBCursorRequestChild::IndexedDBCursorRequestChild(
                                                 AsyncConnectionHelper* aHelper,
                                                 IDBCursor* aCursor,
                                                 RequestType aRequestType)
: IndexedDBRequestChildBase(aHelper), mCursor(aCursor),
  mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBCursorRequestChild);
  MOZ_ASSERT(aHelper);
  MOZ_ASSERT(aCursor);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBCursorRequestChild::~IndexedDBCursorRequestChild()
{
  MOZ_COUNT_DTOR(IndexedDBCursorRequestChild);
}

bool
IndexedDBCursorRequestChild::Recv__delete__(const ResponseValue& aResponse)
{
  switch (aResponse.type()) {
    case ResponseValue::Tnsresult:
      break;
    case ResponseValue::TContinueResponse:
      MOZ_ASSERT(mRequestType == ParamsUnionType::TContinueParams);
      break;

    default:
      MOZ_CRASH("Received invalid response parameters!");
  }

  nsresult rv = mHelper->OnParentProcessRequestComplete(aResponse);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

/*******************************************************************************
 * IndexedDBDeleteDatabaseRequestChild
 ******************************************************************************/

IndexedDBDeleteDatabaseRequestChild::IndexedDBDeleteDatabaseRequestChild(
                                                 IDBFactory* aFactory,
                                                 IDBOpenDBRequest* aOpenRequest,
                                                 const nsACString& aDatabaseId)
: mFactory(aFactory), mOpenRequest(aOpenRequest), mDatabaseId(aDatabaseId)
{
  MOZ_COUNT_CTOR(IndexedDBDeleteDatabaseRequestChild);
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(aOpenRequest);
  MOZ_ASSERT(!aDatabaseId.IsEmpty());
}

IndexedDBDeleteDatabaseRequestChild::~IndexedDBDeleteDatabaseRequestChild()
{
  MOZ_COUNT_DTOR(IndexedDBDeleteDatabaseRequestChild);
}

bool
IndexedDBDeleteDatabaseRequestChild::Recv__delete__(const nsresult& aRv)
{
  nsRefPtr<IPCDeleteDatabaseHelper> helper =
    new IPCDeleteDatabaseHelper(mOpenRequest);

  if (NS_SUCCEEDED(aRv)) {
    DatabaseInfo::Remove(mDatabaseId);
  }
  else {
    helper->SetError(aRv);
  }

  ImmediateRunEventTarget target;
  if (NS_FAILED(helper->Dispatch(&target))) {
    NS_WARNING("Dispatch of IPCSetVersionHelper failed!");
    return false;
  }

  return true;
}

bool
IndexedDBDeleteDatabaseRequestChild::RecvBlocked(
                                                const uint64_t& aCurrentVersion)
{
  MOZ_ASSERT(mOpenRequest);

  nsCOMPtr<nsIRunnable> runnable =
    IDBVersionChangeEvent::CreateBlockedRunnable(mOpenRequest,
                                                 aCurrentVersion, 0);

  ImmediateRunEventTarget target;
  if (NS_FAILED(target.Dispatch(runnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Dispatch of blocked event failed!");
  }

  return true;
}

/*******************************************************************************
 * Helpers
 ******************************************************************************/

nsresult
IPCOpenDatabaseHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
}

AsyncConnectionHelper::ChildProcessSendResult
IPCOpenDatabaseHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  MOZ_CRASH("Don't call me!");
}

nsresult
IPCOpenDatabaseHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  MOZ_CRASH("Don't call me!");
}

nsresult
IPCOpenDatabaseHelper::GetSuccessResult(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  return WrapNative(aCx, NS_ISUPPORTS_CAST(EventTarget*, mDatabase),
                    aVal);
}

nsresult
IPCSetVersionHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
}

AsyncConnectionHelper::ChildProcessSendResult
IPCSetVersionHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  MOZ_CRASH("Don't call me!");
}

nsresult
IPCSetVersionHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  MOZ_CRASH("Don't call me!");
}

already_AddRefed<nsIDOMEvent>
IPCSetVersionHelper::CreateSuccessEvent(mozilla::dom::EventTarget* aOwner)
{
  return IDBVersionChangeEvent::CreateUpgradeNeeded(aOwner,
                                                    mOldVersion,
                                                    mRequestedVersion);
}

nsresult
IPCSetVersionHelper::GetSuccessResult(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  mOpenRequest->SetTransaction(mTransaction);

  return WrapNative(aCx, NS_ISUPPORTS_CAST(EventTarget*, mDatabase),
                    aVal);
}

nsresult
IPCDeleteDatabaseHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  MOZ_CRASH("Don't call me!");
}

AsyncConnectionHelper::ChildProcessSendResult
IPCDeleteDatabaseHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  MOZ_CRASH("Don't call me!");
}

nsresult
IPCDeleteDatabaseHelper::GetSuccessResult(JSContext* aCx, JS::MutableHandle<JS::Value> aVal)
{
  aVal.setUndefined();
  return NS_OK;
}

nsresult
IPCDeleteDatabaseHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  MOZ_CRASH("Don't call me!");
}
