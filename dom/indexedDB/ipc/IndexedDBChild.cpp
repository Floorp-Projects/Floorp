/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IndexedDBChild.h"

#include "mozilla/Assertions.h"

#include "nsIAtom.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IndexedDatabaseManager.h"

USING_INDEXEDDB_NAMESPACE

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
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  GetSuccessResult(JSContext* aCx, jsval* aVal) MOZ_OVERRIDE;

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
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  DoDatabaseWork(mozIStorageConnection* aConnection) MOZ_OVERRIDE;

  virtual already_AddRefed<nsDOMEvent>
  CreateSuccessEvent() MOZ_OVERRIDE;

  virtual nsresult
  GetSuccessResult(JSContext* aCx, jsval* aVal) MOZ_OVERRIDE;
};

class IPCDeleteDatabaseHelper : public AsyncConnectionHelper
{
public:
  IPCDeleteDatabaseHelper(IDBRequest* aRequest)
  : AsyncConnectionHelper(static_cast<IDBDatabase*>(NULL), aRequest)
  { }

  virtual nsresult UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
                                            MOZ_OVERRIDE;

  virtual ChildProcessSendResult
  MaybeSendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  GetSuccessResult(JSContext* aCx, jsval* aVal) MOZ_OVERRIDE;

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

    nsRefPtr<nsDOMEvent> event =
      IDBVersionChangeEvent::Create(mOldVersion, mNewVersion);
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

IndexedDBChild::IndexedDBChild(const nsCString& aASCIIOrigin)
: mFactory(nsnull), mASCIIOrigin(aASCIIOrigin)
{
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
IndexedDBChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mFactory) {
    mFactory->SetActor(static_cast<IndexedDBChild*>(NULL));
#ifdef DEBUG
    mFactory = NULL;
#endif
  }
}

PIndexedDBDatabaseChild*
IndexedDBChild::AllocPIndexedDBDatabase(const nsString& aName,
                                        const uint64_t& aVersion)
{
  return new IndexedDBDatabaseChild(aName, aVersion);
}

bool
IndexedDBChild::DeallocPIndexedDBDatabase(PIndexedDBDatabaseChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBDeleteDatabaseRequestChild*
IndexedDBChild::AllocPIndexedDBDeleteDatabaseRequest(const nsString& aName)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a request!");
  return NULL;
}

bool
IndexedDBChild::DeallocPIndexedDBDeleteDatabaseRequest(
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
: mDatabase(NULL), mName(aName), mVersion(aVersion)
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

bool
IndexedDBDatabaseChild::EnsureDatabase(
                           IDBRequest* aRequest,
                           const DatabaseInfoGuts& aDBInfo,
                           const InfallibleTArray<ObjectStoreInfoGuts>& aOSInfo)
{
  nsCOMPtr<nsIAtom> databaseId;
  if (mDatabase) {
    databaseId = mDatabase->Id();
  }
  else {
    databaseId =
      IndexedDatabaseManager::GetDatabaseId(aDBInfo.origin, aDBInfo.name);
  }
  NS_ENSURE_TRUE(databaseId, false);

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
      IDBDatabase::Create(aRequest, dbInfo.forget(), aDBInfo.origin, NULL);
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
  MOZ_ASSERT(!mStrongDatabase);
  if (mDatabase) {
    mDatabase->SetActor(static_cast<IndexedDBDatabaseChild*>(NULL));
#ifdef DEBUG
    mDatabase = NULL;
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

  request->SetTransaction(NULL);

  MainThreadEventTarget target;
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
    openHelper = new IPCOpenDatabaseHelper(NULL, request);
  }

  openHelper->SetError(aRv);
  request->SetTransaction(NULL);

  MainThreadEventTarget target;
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
    IDBVersionChangeEvent::CreateBlockedRunnable(aOldVersion, mVersion,
                                                 mRequest);

  MainThreadEventTarget target;
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

  MainThreadEventTarget target;
  if (NS_FAILED(target.Dispatch(runnable, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Dispatch of versionchange event failed!");
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

  nsTArray<nsString> storesToOpen;
  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::CreateInternal(mDatabase, storesToOpen,
                                   IDBTransaction::VERSION_CHANGE, false, true);
  NS_ENSURE_TRUE(transaction, false);

  nsRefPtr<IPCSetVersionHelper> versionHelper =
    new IPCSetVersionHelper(transaction, mRequest, oldVersion, mVersion);

  mDatabase->EnterSetVersionTransaction();
  mDatabase->mPreviousDatabaseInfo->version = oldVersion;

  MainThreadEventTarget target;
  if (NS_FAILED(versionHelper->Dispatch(&target))) {
    NS_WARNING("Dispatch of IPCSetVersionHelper failed!");
    return false;
  }

  actor->SetTransaction(transaction);

  mOpenHelper = helper.forget();
  return true;
}

PIndexedDBTransactionChild*
IndexedDBDatabaseChild::AllocPIndexedDBTransaction(
                                               const TransactionParams& aParams)
{
  MOZ_ASSERT(aParams.type() ==
             TransactionParams::TVersionChangeTransactionParams);
  return new IndexedDBTransactionChild();
}

bool
IndexedDBDatabaseChild::DeallocPIndexedDBTransaction(
                                             PIndexedDBTransactionChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBTransactionChild
 ******************************************************************************/

IndexedDBTransactionChild::IndexedDBTransactionChild()
: mTransaction(NULL)
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

  MainThreadEventTarget target;
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
    mTransaction->SetActor(static_cast<IndexedDBTransactionChild*>(NULL));
#ifdef DEBUG
    mTransaction = NULL;
#endif
  }
}

bool
IndexedDBTransactionChild::RecvComplete(const nsresult& aRv)
{
  FireCompleteEvent(aRv);
  return true;
}

PIndexedDBObjectStoreChild*
IndexedDBTransactionChild::AllocPIndexedDBObjectStore(
                                    const ObjectStoreConstructorParams& aParams)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct an object store!");
  return NULL;
}

bool
IndexedDBTransactionChild::DeallocPIndexedDBObjectStore(
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
IndexedDBObjectStoreChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mObjectStore) {
    mObjectStore->SetActor(static_cast<IndexedDBObjectStoreChild*>(NULL));
#ifdef DEBUG
    mObjectStore = NULL;
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
  nsresult rv =
    mObjectStore->OpenCursorFromChildProcess(request, direction, aParams.key(),
                                             aParams.cloneInfo(),
                                             getter_AddRefs(cursor));
  NS_ENSURE_SUCCESS(rv, false);

  actor->SetCursor(cursor);
  return true;
}

PIndexedDBRequestChild*
IndexedDBObjectStoreChild::AllocPIndexedDBRequest(
                                        const ObjectStoreRequestParams& aParams)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a request!");
  return NULL;
}

bool
IndexedDBObjectStoreChild::DeallocPIndexedDBRequest(
                                                 PIndexedDBRequestChild* aActor)
{
  delete aActor;
  return false;
}

PIndexedDBIndexChild*
IndexedDBObjectStoreChild::AllocPIndexedDBIndex(
                                          const IndexConstructorParams& aParams)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct an index!");
  return NULL;
}

bool
IndexedDBObjectStoreChild::DeallocPIndexedDBIndex(PIndexedDBIndexChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBCursorChild*
IndexedDBObjectStoreChild::AllocPIndexedDBCursor(
                              const ObjectStoreCursorConstructorParams& aParams)
{
  return new IndexedDBCursorChild();
}

bool
IndexedDBObjectStoreChild::DeallocPIndexedDBCursor(
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
IndexedDBIndexChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mIndex) {
    mIndex->SetActor(static_cast<IndexedDBIndexChild*>(NULL));
#ifdef DEBUG
    mIndex = NULL;
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
      const SerializedStructuredCloneReadInfo& cloneInfo =
        aParams.optionalCloneInfo().get_SerializedStructuredCloneReadInfo();

      rv = mIndex->OpenCursorFromChildProcess(request, direction, aParams.key(),
                                              aParams.objectKey(), cloneInfo,
                                              getter_AddRefs(cursor));
      NS_ENSURE_SUCCESS(rv, false);
    } break;

    case CursorUnionType::Tvoid_t:
      rv = mIndex->OpenCursorFromChildProcess(request, direction, aParams.key(),
                                              aParams.objectKey(),
                                              getter_AddRefs(cursor));
      NS_ENSURE_SUCCESS(rv, false);
      break;

    default:
      MOZ_NOT_REACHED("Unknown union type!");
      return false;
  }

  actor->SetCursor(cursor);
  return true;
}

PIndexedDBRequestChild*
IndexedDBIndexChild::AllocPIndexedDBRequest(const IndexRequestParams& aParams)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a request!");
  return NULL;
}

bool
IndexedDBIndexChild::DeallocPIndexedDBRequest(PIndexedDBRequestChild* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBCursorChild*
IndexedDBIndexChild::AllocPIndexedDBCursor(
                                    const IndexCursorConstructorParams& aParams)
{
  return new IndexedDBCursorChild();
}

bool
IndexedDBIndexChild::DeallocPIndexedDBCursor(PIndexedDBCursorChild* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBCursorChild
 ******************************************************************************/

IndexedDBCursorChild::IndexedDBCursorChild()
: mCursor(NULL)
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
IndexedDBCursorChild::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mCursor) {
    mCursor->SetActor(static_cast<IndexedDBCursorChild*>(NULL));
#ifdef DEBUG
    mCursor = NULL;
#endif
  }
}

PIndexedDBRequestChild*
IndexedDBCursorChild::AllocPIndexedDBRequest(const CursorRequestParams& aParams)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a request!");
  return NULL;
}

bool
IndexedDBCursorChild::DeallocPIndexedDBRequest(PIndexedDBRequestChild* aActor)
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
  return mHelper ? mHelper->GetRequest() : NULL;
}

bool
IndexedDBRequestChildBase::Recv__delete__(const ResponseValue& aResponse)
{
  MOZ_NOT_REACHED("This should be overridden!");
  return false;
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
      MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenCursorParams);
      break;

    default:
      MOZ_NOT_REACHED("Received invalid response parameters!");
      return false;
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
      MOZ_NOT_REACHED("Received invalid response parameters!");
      return false;
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
      MOZ_NOT_REACHED("Received invalid response parameters!");
      return false;
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
                                                 nsIAtom* aDatabaseId)
: mFactory(aFactory), mOpenRequest(aOpenRequest), mDatabaseId(aDatabaseId)
{
  MOZ_COUNT_CTOR(IndexedDBDeleteDatabaseRequestChild);
  MOZ_ASSERT(aFactory);
  MOZ_ASSERT(aOpenRequest);
  MOZ_ASSERT(aDatabaseId);
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

  MainThreadEventTarget target;
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
    IDBVersionChangeEvent::CreateBlockedRunnable(aCurrentVersion, 0,
                                                 mOpenRequest);

  MainThreadEventTarget target;
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

HelperBase::ChildProcessSendResult
IPCOpenDatabaseHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  MOZ_NOT_REACHED("Don't call me!");
  return Error;
}

nsresult
IPCOpenDatabaseHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  MOZ_NOT_REACHED("Don't call me!");
  return NS_ERROR_FAILURE;
}

nsresult
IPCOpenDatabaseHelper::GetSuccessResult(JSContext* aCx, jsval* aVal)
{
  return WrapNative(aCx, NS_ISUPPORTS_CAST(nsIDOMEventTarget*, mDatabase),
                    aVal);
}

nsresult
IPCSetVersionHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  NS_NOTREACHED("Should never get here!");
  return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
}

HelperBase::ChildProcessSendResult
IPCSetVersionHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  MOZ_NOT_REACHED("Don't call me!");
  return Error;
}

nsresult
IPCSetVersionHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  MOZ_NOT_REACHED("Don't call me!");
  return NS_ERROR_FAILURE;
}

already_AddRefed<nsDOMEvent>
IPCSetVersionHelper::CreateSuccessEvent()
{
  return IDBVersionChangeEvent::CreateUpgradeNeeded(mOldVersion,
                                                    mRequestedVersion);
}

nsresult
IPCSetVersionHelper::GetSuccessResult(JSContext* aCx, jsval* aVal)
{
  mOpenRequest->SetTransaction(mTransaction);

  return WrapNative(aCx, NS_ISUPPORTS_CAST(nsIDOMEventTarget*, mDatabase),
                    aVal);
}

nsresult
IPCDeleteDatabaseHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  MOZ_NOT_REACHED("Don't call me!");
  return NS_ERROR_FAILURE;
}

HelperBase::ChildProcessSendResult
IPCDeleteDatabaseHelper::MaybeSendResponseToChildProcess(nsresult aResultCode)
{
  MOZ_NOT_REACHED("Don't call me!");
  return Error;
}

nsresult
IPCDeleteDatabaseHelper::GetSuccessResult(JSContext* aCx, jsval* aVal)
{
  *aVal = JSVAL_VOID;
  return NS_OK;
}

nsresult
IPCDeleteDatabaseHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  MOZ_NOT_REACHED("Don't call me!");
  return NS_ERROR_FAILURE;
}
