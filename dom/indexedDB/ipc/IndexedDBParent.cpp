/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IndexedDBParent.h"

#include "nsIDOMFile.h"
#include "nsIDOMEvent.h"
#include "nsIIDBVersionChangeEvent.h"
#include "nsIJSContextStack.h"
#include "nsIXPConnect.h"

#include "mozilla/Assertions.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ipc/Blob.h"
#include "nsContentUtils.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBDatabase.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBIndex.h"
#include "IDBKeyRange.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"

USING_INDEXEDDB_NAMESPACE

using namespace mozilla::dom;

/*******************************************************************************
 * AutoSetCurrentTransaction
 ******************************************************************************/

AutoSetCurrentTransaction::AutoSetCurrentTransaction(
                                                   IDBTransaction* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  AsyncConnectionHelper::SetCurrentTransaction(aTransaction);
}

AutoSetCurrentTransaction::~AutoSetCurrentTransaction()
{
  AsyncConnectionHelper::SetCurrentTransaction(NULL);
}

/*******************************************************************************
 * IndexedDBParent
 ******************************************************************************/

IndexedDBParent::IndexedDBParent()
{
  MOZ_COUNT_CTOR(IndexedDBParent);
}

IndexedDBParent::~IndexedDBParent()
{
  MOZ_COUNT_DTOR(IndexedDBParent);
}

void
IndexedDBParent::ActorDestroy(ActorDestroyReason aWhy)
{
  // Nothing really needs to be done here...
}

bool
IndexedDBParent::RecvPIndexedDBDatabaseConstructor(
                                               PIndexedDBDatabaseParent* aActor,
                                               const nsString& aName,
                                               const uint64_t& aVersion)
{
  nsRefPtr<IDBOpenDBRequest> request;
  nsresult rv =
    mFactory->OpenCommon(aName, aVersion, false, nullptr,
                         getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, false);

  IndexedDBDatabaseParent* actor =
    static_cast<IndexedDBDatabaseParent*>(aActor);

  rv = actor->SetOpenRequest(request);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
IndexedDBParent::RecvPIndexedDBDeleteDatabaseRequestConstructor(
                                  PIndexedDBDeleteDatabaseRequestParent* aActor,
                                  const nsString& aName)
{
  IndexedDBDeleteDatabaseRequestParent* actor =
    static_cast<IndexedDBDeleteDatabaseRequestParent*>(aActor);

  nsRefPtr<IDBOpenDBRequest> request;

  nsresult rv =
    mFactory->OpenCommon(aName, 0, true, nullptr, getter_AddRefs(request));
  NS_ENSURE_SUCCESS(rv, false);

  rv = actor->SetOpenRequest(request);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

PIndexedDBDatabaseParent*
IndexedDBParent::AllocPIndexedDBDatabase(const nsString& aName,
                                         const uint64_t& aVersion)
{
  return new IndexedDBDatabaseParent();
}

bool
IndexedDBParent::DeallocPIndexedDBDatabase(PIndexedDBDatabaseParent* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBDeleteDatabaseRequestParent*
IndexedDBParent::AllocPIndexedDBDeleteDatabaseRequest(const nsString& aName)
{
  return new IndexedDBDeleteDatabaseRequestParent(mFactory);
}

bool
IndexedDBParent::DeallocPIndexedDBDeleteDatabaseRequest(
                                  PIndexedDBDeleteDatabaseRequestParent* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBDatabaseParent
 ******************************************************************************/

IndexedDBDatabaseParent::IndexedDBDatabaseParent()
: mEventListener(ALLOW_THIS_IN_INITIALIZER_LIST(this))
{
  MOZ_COUNT_CTOR(IndexedDBDatabaseParent);
}

IndexedDBDatabaseParent::~IndexedDBDatabaseParent()
{
  MOZ_COUNT_DTOR(IndexedDBDatabaseParent);
}

nsresult
IndexedDBDatabaseParent::SetOpenRequest(IDBOpenDBRequest* aRequest)
{
  MOZ_ASSERT(aRequest);
  MOZ_ASSERT(!mOpenRequest);

  nsIDOMEventTarget* target = static_cast<nsIDOMEventTarget*>(aRequest);

  nsresult rv = target->AddEventListener(NS_LITERAL_STRING(SUCCESS_EVT_STR),
                                         mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = target->AddEventListener(NS_LITERAL_STRING(ERROR_EVT_STR),
                                mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = target->AddEventListener(NS_LITERAL_STRING(BLOCKED_EVT_STR),
                                mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = target->AddEventListener(NS_LITERAL_STRING(UPGRADENEEDED_EVT_STR),
                                mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  mOpenRequest = aRequest;
  return NS_OK;
}

nsresult
IndexedDBDatabaseParent::HandleEvent(nsIDOMEvent* aEvent)
{
  MOZ_ASSERT(aEvent);

  nsString type;
  nsresult rv = aEvent->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMEventTarget> target;
  rv = aEvent->GetTarget(getter_AddRefs(target));
  NS_ENSURE_SUCCESS(rv, rv);

  if (mDatabase &&
      SameCOMIdentity(target, NS_ISUPPORTS_CAST(nsIDOMEventTarget*,
                                                mDatabase))) {
    rv = HandleDatabaseEvent(aEvent, type);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  if (mOpenRequest &&
      SameCOMIdentity(target, NS_ISUPPORTS_CAST(nsIDOMEventTarget*,
                                                mOpenRequest))) {
    rv = HandleRequestEvent(aEvent, type);
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  MOZ_NOT_REACHED("Unexpected message!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
IndexedDBDatabaseParent::HandleRequestEvent(nsIDOMEvent* aEvent,
                                            const nsAString& aType)
{
  MOZ_ASSERT(mOpenRequest);

  nsresult rv;

  if (aType.EqualsLiteral(ERROR_EVT_STR)) {
    nsRefPtr<IDBOpenDBRequest> request;
    mOpenRequest.swap(request);

    rv = request->GetErrorCode();
    MOZ_ASSERT(NS_FAILED(rv));

    if (!SendError(rv)) {
      return NS_ERROR_FAILURE;
    }

    rv = aEvent->PreventDefault();
    NS_ENSURE_SUCCESS(rv, rv);

    return NS_OK;
  }

  if (aType.EqualsLiteral(BLOCKED_EVT_STR)) {
    MOZ_ASSERT(!mDatabase);

    nsCOMPtr<nsIIDBVersionChangeEvent> changeEvent = do_QueryInterface(aEvent);
    NS_ENSURE_TRUE(changeEvent, NS_ERROR_FAILURE);

    PRUint64 oldVersion;
    rv = changeEvent->GetOldVersion(&oldVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!SendBlocked(oldVersion)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  jsval result;
  rv = mOpenRequest->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);

  MOZ_ASSERT(!JSVAL_IS_PRIMITIVE(result));

  nsIXPConnect* xpc = nsContentUtils::XPConnect();
  MOZ_ASSERT(xpc);

  JSContext* cx =  nsContentUtils::ThreadJSContextStack()->GetSafeJSContext();
  MOZ_ASSERT(cx);

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  rv = xpc->GetWrappedNativeOfJSObject(cx, JSVAL_TO_OBJECT(result),
                                       getter_AddRefs(wrapper));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIIDBDatabase> database;
  if (!wrapper || !(database = do_QueryInterface(wrapper->Native()))) {
    NS_WARNING("Didn't get the object we expected!");
    return NS_ERROR_FAILURE;
  }

  IDBDatabase* databaseConcrete = static_cast<IDBDatabase*>(database.get());

  DatabaseInfo* dbInfo = databaseConcrete->Info();
  MOZ_ASSERT(dbInfo);

  nsAutoTArray<nsString, 20> objectStoreNames;
  if (!dbInfo->GetObjectStoreNames(objectStoreNames)) {
    MOZ_NOT_REACHED("This should never fail!");
  }

  InfallibleTArray<ObjectStoreInfoGuts> objectStoreInfos;
  if (!objectStoreNames.IsEmpty()) {
    uint32_t length = objectStoreNames.Length();

    objectStoreInfos.SetCapacity(length);

    for (uint32_t i = 0; i < length; i++) {
      ObjectStoreInfo* osInfo = dbInfo->GetObjectStore(objectStoreNames[i]);
      MOZ_ASSERT(osInfo);

      objectStoreInfos.AppendElement(*osInfo);
    }
  }

  if (aType.EqualsLiteral(SUCCESS_EVT_STR)) {
    nsRefPtr<IDBOpenDBRequest> request;
    mOpenRequest.swap(request);

    nsIDOMEventTarget* target =
      static_cast<nsIDOMEventTarget*>(databaseConcrete);

#ifdef DEBUG
    {
      nsresult rvDEBUG =
        target->AddEventListener(NS_LITERAL_STRING(ERROR_EVT_STR),
                                 mEventListener, false);
      NS_WARN_IF_FALSE(NS_SUCCEEDED(rvDEBUG), "Failed to add error listener!");
    }
#endif

    NS_NAMED_LITERAL_STRING(versionChange, VERSIONCHANGE_EVT_STR);
    rv = target->AddEventListener(versionChange, mEventListener, false);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!SendSuccess(*dbInfo, objectStoreInfos)) {
      return NS_ERROR_FAILURE;
    }

    MOZ_ASSERT(!mDatabase || mDatabase == databaseConcrete);
    mDatabase = databaseConcrete;

    return NS_OK;
  }

  if (aType.EqualsLiteral(UPGRADENEEDED_EVT_STR)) {
    MOZ_ASSERT(!mDatabase);

    nsCOMPtr<nsIIDBVersionChangeEvent> changeEvent = do_QueryInterface(aEvent);
    NS_ENSURE_TRUE(changeEvent, NS_ERROR_FAILURE);

    PRUint64 oldVersion;
    rv = changeEvent->GetOldVersion(&oldVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    nsAutoPtr<IndexedDBVersionChangeTransactionParent> actor(
      new IndexedDBVersionChangeTransactionParent());

    IDBTransaction* transaction =
      AsyncConnectionHelper::GetCurrentTransaction();
    MOZ_ASSERT(transaction);

    rv = actor->SetTransaction(transaction);
    NS_ENSURE_SUCCESS(rv, rv);

    VersionChangeTransactionParams versionChangeParams;
    versionChangeParams.dbInfo() = *dbInfo;
    versionChangeParams.osInfo() = objectStoreInfos;
    versionChangeParams.oldVersion() = oldVersion;

    if (!SendPIndexedDBTransactionConstructor(actor.forget(),
                                              versionChangeParams)) {
      return NS_ERROR_FAILURE;
    }

    mDatabase = databaseConcrete;

    return NS_OK;
  }

  MOZ_NOT_REACHED("Unexpected message type!");
  return NS_ERROR_UNEXPECTED;
}

nsresult
IndexedDBDatabaseParent::HandleDatabaseEvent(nsIDOMEvent* aEvent,
                                             const nsAString& aType)
{
  MOZ_ASSERT(mDatabase);
  MOZ_ASSERT(!aType.EqualsLiteral(ERROR_EVT_STR),
             "Should never get error events in the parent process!");

  nsresult rv;

  if (aType.EqualsLiteral(VERSIONCHANGE_EVT_STR)) {
    JSContext* cx = nsContentUtils::GetSafeJSContext();
    NS_ENSURE_TRUE(cx, NS_ERROR_FAILURE);

    nsCOMPtr<nsIIDBVersionChangeEvent> changeEvent = do_QueryInterface(aEvent);
    NS_ENSURE_TRUE(changeEvent, NS_ERROR_FAILURE);

    PRUint64 oldVersion;
    rv = changeEvent->GetOldVersion(&oldVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    JS::Value newVersionVal;
    rv = changeEvent->GetNewVersion(cx, &newVersionVal);
    NS_ENSURE_SUCCESS(rv, rv);

    uint64_t newVersion;
    if (newVersionVal.isNull()) {
      newVersion = 0;
    }
    else {
      MOZ_ASSERT(newVersionVal.isNumber());
      newVersion = static_cast<uint64_t>(newVersionVal.toNumber());
    }

    if (!SendVersionChange(oldVersion, newVersion)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

  MOZ_NOT_REACHED("Unexpected message type!");
  return NS_ERROR_UNEXPECTED;
}

void
IndexedDBDatabaseParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mDatabase) {
    mDatabase->SetActor(static_cast<IndexedDBDatabaseParent*>(NULL));
    mDatabase->Invalidate();
  }
}

bool
IndexedDBDatabaseParent::RecvClose(const bool& aUnlinked)
{
  MOZ_ASSERT(mDatabase);

  mDatabase->CloseInternal(aUnlinked);
  return true;
}

bool
IndexedDBDatabaseParent::RecvPIndexedDBTransactionConstructor(
                                            PIndexedDBTransactionParent* aActor,
                                            const TransactionParams& aParams)
{
  MOZ_ASSERT(aParams.type() ==
             TransactionParams::TNormalTransactionParams);
  MOZ_ASSERT(!mOpenRequest);
  MOZ_ASSERT(mDatabase);

  IndexedDBTransactionParent* actor =
    static_cast<IndexedDBTransactionParent*>(aActor);

  const NormalTransactionParams& params = aParams.get_NormalTransactionParams();

  nsTArray<nsString> storesToOpen;
  storesToOpen.AppendElements(params.names());

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(mDatabase, storesToOpen, params.mode(), false);
  NS_ENSURE_TRUE(transaction, false);

  nsresult rv = actor->SetTransaction(transaction);
  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

PIndexedDBTransactionParent*
IndexedDBDatabaseParent::AllocPIndexedDBTransaction(
                                               const TransactionParams& aParams)
{
  MOZ_ASSERT(aParams.type() ==
             TransactionParams::TNormalTransactionParams);
  return new IndexedDBTransactionParent();
}

bool
IndexedDBDatabaseParent::DeallocPIndexedDBTransaction(
                                            PIndexedDBTransactionParent* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBTransactionParent
 ******************************************************************************/

IndexedDBTransactionParent::IndexedDBTransactionParent()
: mEventListener(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
  mArtificialRequestCount(false)
{
  MOZ_COUNT_CTOR(IndexedDBTransactionParent);
}

IndexedDBTransactionParent::~IndexedDBTransactionParent()
{
  MOZ_COUNT_DTOR(IndexedDBTransactionParent);
}

nsresult
IndexedDBTransactionParent::SetTransaction(IDBTransaction* aTransaction)
{
  MOZ_ASSERT(aTransaction);
  MOZ_ASSERT(!mTransaction);

  nsIDOMEventTarget* target = static_cast<nsIDOMEventTarget*>(aTransaction);

  NS_NAMED_LITERAL_STRING(complete, COMPLETE_EVT_STR);
  nsresult rv = target->AddEventListener(complete, mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = target->AddEventListener(NS_LITERAL_STRING(ABORT_EVT_STR),
                                mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  aTransaction->OnNewRequest();
  mArtificialRequestCount = true;

  aTransaction->SetActor(this);

  mTransaction = aTransaction;
  return NS_OK;
}

nsresult
IndexedDBTransactionParent::HandleEvent(nsIDOMEvent* aEvent)
{
  nsString type;
  nsresult rv = aEvent->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  nsresult transactionResult;

  if (type.EqualsLiteral(COMPLETE_EVT_STR)) {
    transactionResult = NS_OK;
  }
  else if (type.EqualsLiteral(ABORT_EVT_STR)) {
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMEventTarget> target;
      if (NS_FAILED(aEvent->GetTarget(getter_AddRefs(target)))) {
        NS_WARNING("Failed to get target!");
      }
      else {
        MOZ_ASSERT(SameCOMIdentity(target, NS_ISUPPORTS_CAST(nsIDOMEventTarget*,
                                                             mTransaction)));
      }
    }
#endif
    MOZ_ASSERT(NS_FAILED(mTransaction->GetAbortCode()));
    transactionResult = mTransaction->GetAbortCode();
  }
  else {
    NS_WARNING("Unknown message type!");
    return NS_ERROR_UNEXPECTED;
  }

  if (!SendComplete(transactionResult)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
IndexedDBTransactionParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mTransaction) {
    if (mArtificialRequestCount) {
      // The transaction never completed and now the child side is dead. Abort
      // here to be safe.
      mTransaction->Abort();

      mTransaction->OnRequestFinished();
#ifdef DEBUG
      mArtificialRequestCount = false;
#endif
    }
    mTransaction->SetActor(static_cast<IndexedDBTransactionParent*>(NULL));
  }
}

bool
IndexedDBTransactionParent::RecvAbort(const nsresult& aAbortCode)
{
  MOZ_ASSERT(mTransaction);
  mTransaction->Abort(aAbortCode);
  return true;
}

bool
IndexedDBTransactionParent::RecvAllRequestsFinished()
{
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mArtificialRequestCount);

  mTransaction->OnRequestFinished();
  mArtificialRequestCount = false;

  return true;
}

bool
IndexedDBTransactionParent::RecvDeleteObjectStore(const nsString& aName)
{
  MOZ_NOT_REACHED("Should be overridden, don't call me!");
  return false;
}

bool
IndexedDBTransactionParent::RecvPIndexedDBObjectStoreConstructor(
                                    PIndexedDBObjectStoreParent* aActor,
                                    const ObjectStoreConstructorParams& aParams)
{
  IndexedDBObjectStoreParent* actor =
    static_cast<IndexedDBObjectStoreParent*>(aActor);

  if (aParams.type() ==
      ObjectStoreConstructorParams::TGetObjectStoreParams) {
    const GetObjectStoreParams& params = aParams.get_GetObjectStoreParams();
    const nsString& name = params.name();

    nsRefPtr<IDBObjectStore> objectStore;

    {
      AutoSetCurrentTransaction asct(mTransaction);

      nsresult rv =
        mTransaction->ObjectStoreInternal(name, getter_AddRefs(objectStore));
      NS_ENSURE_SUCCESS(rv, false);

      actor->SetObjectStore(objectStore);
    }

    objectStore->SetActor(actor);
    return true;
  }

  if (aParams.type() ==
      ObjectStoreConstructorParams::TCreateObjectStoreParams) {
    MOZ_NOT_REACHED("Should be overridden, don't call me!");
    return false;
  }

  MOZ_NOT_REACHED("Unknown param type!");
  return false;
}

PIndexedDBObjectStoreParent*
IndexedDBTransactionParent::AllocPIndexedDBObjectStore(
                                    const ObjectStoreConstructorParams& aParams)
{
  return new IndexedDBObjectStoreParent();
}

bool
IndexedDBTransactionParent::DeallocPIndexedDBObjectStore(
                                            PIndexedDBObjectStoreParent* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBVersionChangeTransactionParent
 ******************************************************************************/

IndexedDBVersionChangeTransactionParent::
  IndexedDBVersionChangeTransactionParent()
{
  MOZ_COUNT_CTOR(IndexedDBVersionChangeTransactionParent);
}

IndexedDBVersionChangeTransactionParent::
  ~IndexedDBVersionChangeTransactionParent()
{
  MOZ_COUNT_DTOR(IndexedDBVersionChangeTransactionParent);
}

bool
IndexedDBVersionChangeTransactionParent::RecvDeleteObjectStore(
                                                          const nsString& aName)
{
  MOZ_ASSERT(mTransaction->GetMode() == IDBTransaction::VERSION_CHANGE);

  IDBDatabase* db = mTransaction->Database();
  MOZ_ASSERT(db);

  nsresult rv;

  {
    AutoSetCurrentTransaction asct(mTransaction);

    rv = db->DeleteObjectStore(aName);
  }

  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
IndexedDBVersionChangeTransactionParent::RecvPIndexedDBObjectStoreConstructor(
                                    PIndexedDBObjectStoreParent* aActor,
                                    const ObjectStoreConstructorParams& aParams)
{
  IndexedDBObjectStoreParent* actor =
    static_cast<IndexedDBObjectStoreParent*>(aActor);

  if (aParams.type() ==
      ObjectStoreConstructorParams::TCreateObjectStoreParams) {
    MOZ_ASSERT(mTransaction->GetMode() == IDBTransaction::VERSION_CHANGE);

    const CreateObjectStoreParams& params =
      aParams.get_CreateObjectStoreParams();

    const ObjectStoreInfoGuts& info = params.info();

    IDBDatabase* db = mTransaction->Database();
    MOZ_ASSERT(db);

    nsRefPtr<IDBObjectStore> objectStore;

    nsresult rv;

    {
      AutoSetCurrentTransaction asct(mTransaction);

      rv = db->CreateObjectStoreInternal(mTransaction, info,
                                         getter_AddRefs(objectStore));
    }

    NS_ENSURE_SUCCESS(rv, false);

    actor->SetObjectStore(objectStore);
    objectStore->SetActor(actor);
    return true;
  }

  return
    IndexedDBTransactionParent::RecvPIndexedDBObjectStoreConstructor(aActor,
                                                                     aParams);
}

PIndexedDBObjectStoreParent*
IndexedDBVersionChangeTransactionParent::AllocPIndexedDBObjectStore(
                                    const ObjectStoreConstructorParams& aParams)
{
  MOZ_ASSERT(mTransaction);
  if (aParams.type() ==
      ObjectStoreConstructorParams::TCreateObjectStoreParams ||
      mTransaction->GetMode() == IDBTransaction::VERSION_CHANGE) {
    return new IndexedDBVersionChangeObjectStoreParent();
  }

  return IndexedDBTransactionParent::AllocPIndexedDBObjectStore(aParams);
}

/*******************************************************************************
 * IndexedDBObjectStoreParent
 ******************************************************************************/

IndexedDBObjectStoreParent::IndexedDBObjectStoreParent()
{
  MOZ_COUNT_CTOR(IndexedDBObjectStoreParent);
}

IndexedDBObjectStoreParent::~IndexedDBObjectStoreParent()
{
  MOZ_COUNT_DTOR(IndexedDBObjectStoreParent);
}

void
IndexedDBObjectStoreParent::SetObjectStore(IDBObjectStore* aObjectStore)
{
  MOZ_ASSERT(aObjectStore);
  MOZ_ASSERT(!mObjectStore);

  mObjectStore = aObjectStore;
}

void
IndexedDBObjectStoreParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mObjectStore) {
    mObjectStore->SetActor(static_cast<IndexedDBObjectStoreParent*>(NULL));
  }
}

bool
IndexedDBObjectStoreParent::RecvDeleteIndex(const nsString& aName)
{
  MOZ_NOT_REACHED("Should be overridden, don't call me!");
  return false;
}

bool
IndexedDBObjectStoreParent::RecvPIndexedDBRequestConstructor(
                                        PIndexedDBRequestParent* aActor,
                                        const ObjectStoreRequestParams& aParams)
{
  IndexedDBObjectStoreRequestParent* actor =
    static_cast<IndexedDBObjectStoreRequestParent*>(aActor);

  switch (aParams.type()) {
    case ObjectStoreRequestParams::TGetParams:
      return actor->Get(aParams.get_GetParams());

    case ObjectStoreRequestParams::TGetAllParams:
      return actor->GetAll(aParams.get_GetAllParams());

    case ObjectStoreRequestParams::TAddParams:
      return actor->Add(aParams.get_AddParams());

    case ObjectStoreRequestParams::TPutParams:
      return actor->Put(aParams.get_PutParams());

    case ObjectStoreRequestParams::TDeleteParams:
      return actor->Delete(aParams.get_DeleteParams());

    case ObjectStoreRequestParams::TClearParams:
      return actor->Clear(aParams.get_ClearParams());

    case ObjectStoreRequestParams::TCountParams:
      return actor->Count(aParams.get_CountParams());

    case ObjectStoreRequestParams::TOpenCursorParams:
      return actor->OpenCursor(aParams.get_OpenCursorParams());

    default:
      MOZ_NOT_REACHED("Unknown type!");
      return false;
  }

  MOZ_NOT_REACHED("Should never get here!");
  return false;
}

bool
IndexedDBObjectStoreParent::RecvPIndexedDBIndexConstructor(
                                          PIndexedDBIndexParent* aActor,
                                          const IndexConstructorParams& aParams)
{
  IndexedDBIndexParent* actor = static_cast<IndexedDBIndexParent*>(aActor);

  if (aParams.type() == IndexConstructorParams::TGetIndexParams) {
    const GetIndexParams& params = aParams.get_GetIndexParams();
    const nsString& name = params.name();

    nsRefPtr<IDBIndex> index;

    {
      AutoSetCurrentTransaction asct(mObjectStore->Transaction());

      nsresult rv = mObjectStore->IndexInternal(name, getter_AddRefs(index));
      NS_ENSURE_SUCCESS(rv, false);

      actor->SetIndex(index);
    }

    index->SetActor(actor);
    return true;
  }

  if (aParams.type() == IndexConstructorParams::TCreateIndexParams) {
    MOZ_NOT_REACHED("Should be overridden, don't call me!");
    return false;
  }

  MOZ_NOT_REACHED("Unknown param type!");
  return false;
}

PIndexedDBRequestParent*
IndexedDBObjectStoreParent::AllocPIndexedDBRequest(
                                        const ObjectStoreRequestParams& aParams)
{
  MOZ_ASSERT(mObjectStore);
  return new IndexedDBObjectStoreRequestParent(mObjectStore, aParams.type());
}

bool
IndexedDBObjectStoreParent::DeallocPIndexedDBRequest(
                                                PIndexedDBRequestParent* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBIndexParent*
IndexedDBObjectStoreParent::AllocPIndexedDBIndex(
                                          const IndexConstructorParams& aParams)
{
  return new IndexedDBIndexParent();
}

bool
IndexedDBObjectStoreParent::DeallocPIndexedDBIndex(
                                                  PIndexedDBIndexParent* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBCursorParent*
IndexedDBObjectStoreParent::AllocPIndexedDBCursor(
                              const ObjectStoreCursorConstructorParams& aParams)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a cursor!");
  return NULL;
}

bool
IndexedDBObjectStoreParent::DeallocPIndexedDBCursor(
                                                 PIndexedDBCursorParent* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBVersionChangeObjectStoreParent
 ******************************************************************************/

IndexedDBVersionChangeObjectStoreParent::
  IndexedDBVersionChangeObjectStoreParent()
{
  MOZ_COUNT_CTOR(IndexedDBVersionChangeObjectStoreParent);
}

IndexedDBVersionChangeObjectStoreParent::
  ~IndexedDBVersionChangeObjectStoreParent()
{
  MOZ_COUNT_DTOR(IndexedDBVersionChangeObjectStoreParent);
}

bool
IndexedDBVersionChangeObjectStoreParent::RecvDeleteIndex(const nsString& aName)
{
  MOZ_ASSERT(mObjectStore->Transaction()->GetMode() ==
             IDBTransaction::VERSION_CHANGE);

  nsresult rv;

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    rv = mObjectStore->DeleteIndex(aName);
  }

  NS_ENSURE_SUCCESS(rv, false);

  return true;
}

bool
IndexedDBVersionChangeObjectStoreParent::RecvPIndexedDBIndexConstructor(
                                          PIndexedDBIndexParent* aActor,
                                          const IndexConstructorParams& aParams)
{
  IndexedDBIndexParent* actor = static_cast<IndexedDBIndexParent*>(aActor);

  if (aParams.type() == IndexConstructorParams::TCreateIndexParams) {
    MOZ_ASSERT(mObjectStore->Transaction()->GetMode() ==
               IDBTransaction::VERSION_CHANGE);

    const CreateIndexParams& params = aParams.get_CreateIndexParams();
    const IndexInfo& info = params.info();

    nsRefPtr<IDBIndex> index;

    nsresult rv;

    {
      AutoSetCurrentTransaction asct(mObjectStore->Transaction());

      rv = mObjectStore->CreateIndexInternal(info, getter_AddRefs(index));
    }

    NS_ENSURE_SUCCESS(rv, false);

    actor->SetIndex(index);
    index->SetActor(actor);
    return true;
  }

  return IndexedDBObjectStoreParent::RecvPIndexedDBIndexConstructor(aActor,
                                                                    aParams);
}

/*******************************************************************************
 * IndexedDBIndexParent
 ******************************************************************************/

IndexedDBIndexParent::IndexedDBIndexParent()
{
  MOZ_COUNT_CTOR(IndexedDBIndexParent);
}

IndexedDBIndexParent::~IndexedDBIndexParent()
{
  MOZ_COUNT_DTOR(IndexedDBIndexParent);
}

void
IndexedDBIndexParent::SetIndex(IDBIndex* aIndex)
{
  MOZ_ASSERT(aIndex);
  MOZ_ASSERT(!mIndex);

  mIndex = aIndex;
}

void
IndexedDBIndexParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mIndex) {
    mIndex->SetActor(static_cast<IndexedDBIndexParent*>(NULL));
  }
}

bool
IndexedDBIndexParent::RecvPIndexedDBRequestConstructor(
                                              PIndexedDBRequestParent* aActor,
                                              const IndexRequestParams& aParams)
{
  IndexedDBIndexRequestParent* actor =
    static_cast<IndexedDBIndexRequestParent*>(aActor);

  switch (aParams.type()) {
    case IndexRequestParams::TGetParams:
      return actor->Get(aParams.get_GetParams());

    case IndexRequestParams::TGetKeyParams:
      return actor->GetKey(aParams.get_GetKeyParams());

    case IndexRequestParams::TGetAllParams:
      return actor->GetAll(aParams.get_GetAllParams());

    case IndexRequestParams::TGetAllKeysParams:
      return actor->GetAllKeys(aParams.get_GetAllKeysParams());

    case IndexRequestParams::TCountParams:
      return actor->Count(aParams.get_CountParams());

    case IndexRequestParams::TOpenCursorParams:
      return actor->OpenCursor(aParams.get_OpenCursorParams());

    case IndexRequestParams::TOpenKeyCursorParams:
      return actor->OpenKeyCursor(aParams.get_OpenKeyCursorParams());

    default:
      MOZ_NOT_REACHED("Unknown type!");
      return false;
  }

  MOZ_NOT_REACHED("Should never get here!");
  return false;
}

PIndexedDBRequestParent*
IndexedDBIndexParent::AllocPIndexedDBRequest(const IndexRequestParams& aParams)
{
  MOZ_ASSERT(mIndex);
  return new IndexedDBIndexRequestParent(mIndex, aParams.type());
}

bool
IndexedDBIndexParent::DeallocPIndexedDBRequest(PIndexedDBRequestParent* aActor)
{
  delete aActor;
  return true;
}

PIndexedDBCursorParent*
IndexedDBIndexParent::AllocPIndexedDBCursor(
                                    const IndexCursorConstructorParams& aParams)
{
  MOZ_NOT_REACHED("Caller is supposed to manually construct a cursor!");
  return NULL;
}

bool
IndexedDBIndexParent::DeallocPIndexedDBCursor(PIndexedDBCursorParent* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBCursorParent
 ******************************************************************************/

IndexedDBCursorParent::IndexedDBCursorParent(IDBCursor* aCursor)
: mCursor(aCursor)
{
  MOZ_COUNT_CTOR(IndexedDBCursorParent);
  MOZ_ASSERT(aCursor);
  aCursor->SetActor(this);
}

IndexedDBCursorParent::~IndexedDBCursorParent()
{
  MOZ_COUNT_DTOR(IndexedDBCursorParent);
}

void
IndexedDBCursorParent::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mCursor) {
    mCursor->SetActor(static_cast<IndexedDBCursorParent*>(NULL));
  }
}

bool
IndexedDBCursorParent::RecvPIndexedDBRequestConstructor(
                                             PIndexedDBRequestParent* aActor,
                                             const CursorRequestParams& aParams)
{
  IndexedDBCursorRequestParent* actor =
    static_cast<IndexedDBCursorRequestParent*>(aActor);

  switch (aParams.type()) {
    case CursorRequestParams::TContinueParams:
      return actor->Continue(aParams.get_ContinueParams());

    default:
      MOZ_NOT_REACHED("Unknown type!");
      return false;
  }

  MOZ_NOT_REACHED("Should never get here!");
  return false;
}

PIndexedDBRequestParent*
IndexedDBCursorParent::AllocPIndexedDBRequest(
                                             const CursorRequestParams& aParams)
{
  return new IndexedDBCursorRequestParent(mCursor, aParams.type());
}

bool
IndexedDBCursorParent::DeallocPIndexedDBRequest(PIndexedDBRequestParent* aActor)
{
  delete aActor;
  return true;
}

/*******************************************************************************
 * IndexedDBRequestParentBase
 ******************************************************************************/

IndexedDBRequestParentBase::IndexedDBRequestParentBase()
{
  MOZ_COUNT_CTOR(IndexedDBRequestParentBase);
}

IndexedDBRequestParentBase::~IndexedDBRequestParentBase()
{
  MOZ_COUNT_DTOR(IndexedDBRequestParentBase);
}

void
IndexedDBRequestParentBase::ActorDestroy(ActorDestroyReason aWhy)
{
  if (mRequest) {
    mRequest->SetActor(NULL);
  }
}

/*******************************************************************************
 * IndexedDBObjectStoreRequestParent
 ******************************************************************************/

IndexedDBObjectStoreRequestParent::IndexedDBObjectStoreRequestParent(
                                                   IDBObjectStore* aObjectStore,
                                                   RequestType aRequestType)
: mObjectStore(aObjectStore), mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBObjectStoreRequestParent);
  MOZ_ASSERT(aObjectStore);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBObjectStoreRequestParent::~IndexedDBObjectStoreRequestParent()
{
  MOZ_COUNT_DTOR(IndexedDBObjectStoreRequestParent);
}

void
IndexedDBObjectStoreRequestParent::ConvertBlobActors(
                                  const InfallibleTArray<PBlobParent*>& aActors,
                                  nsTArray<nsCOMPtr<nsIDOMBlob> >& aBlobs)
{
  MOZ_ASSERT(aBlobs.IsEmpty());

  if (!aActors.IsEmpty()) {
    // Walk the chain to get to ContentParent.
    MOZ_ASSERT(mObjectStore->Transaction()->Database()->GetContentParent());

    uint32_t length = aActors.Length();
    aBlobs.SetCapacity(length);

    for (uint32_t index = 0; index < length; index++) {
      BlobParent* actor = static_cast<BlobParent*>(aActors[index]);
      aBlobs.AppendElement(actor->GetBlob());
    }
  }
}

bool
IndexedDBObjectStoreRequestParent::Get(const GetParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TGetParams);

  nsRefPtr<IDBRequest> request;

  nsRefPtr<IDBKeyRange> keyRange =
    IDBKeyRange::FromSerializedKeyRange(aParams.keyRange());
  MOZ_ASSERT(keyRange);

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    nsresult rv = mObjectStore->GetInternal(keyRange, nullptr,
                                            getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBObjectStoreRequestParent::GetAll(const GetAllParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllParams);

  nsRefPtr<IDBRequest> request;

  const ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange keyRangeUnion =
    aParams.optionalKeyRange();

  nsRefPtr<IDBKeyRange> keyRange;

  switch (keyRangeUnion.type()) {
    case ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange::TKeyRange:
      keyRange =
        IDBKeyRange::FromSerializedKeyRange(keyRangeUnion.get_KeyRange());
      break;

    case ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_NOT_REACHED("Unknown param type!");
      return false;
  }

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    nsresult rv = mObjectStore->GetAllInternal(keyRange, aParams.limit(),
                                               nullptr,
                                               getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBObjectStoreRequestParent::Add(const AddParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TAddParams);

  ipc::AddPutParams params = aParams.commonParams();

  nsTArray<nsCOMPtr<nsIDOMBlob> > blobs;
  ConvertBlobActors(params.blobsParent(), blobs);

  nsRefPtr<IDBRequest> request;

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    nsresult rv =
      mObjectStore->AddOrPutInternal(params.cloneInfo(), params.key(),
                                     params.indexUpdateInfos(), blobs, false,
                                     getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBObjectStoreRequestParent::Put(const PutParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TPutParams);

  ipc::AddPutParams params = aParams.commonParams();

  nsTArray<nsCOMPtr<nsIDOMBlob> > blobs;
  ConvertBlobActors(params.blobsParent(), blobs);

  nsRefPtr<IDBRequest> request;

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    nsresult rv =
      mObjectStore->AddOrPutInternal(params.cloneInfo(), params.key(),
                                     params.indexUpdateInfos(), blobs, true,
                                     getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBObjectStoreRequestParent::Delete(const DeleteParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TDeleteParams);

  nsRefPtr<IDBRequest> request;

  nsRefPtr<IDBKeyRange> keyRange =
    IDBKeyRange::FromSerializedKeyRange(aParams.keyRange());
  MOZ_ASSERT(keyRange);

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    nsresult rv =
      mObjectStore->DeleteInternal(keyRange, nullptr, getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBObjectStoreRequestParent::Clear(const ClearParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TClearParams);

  nsRefPtr<IDBRequest> request;

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    nsresult rv = mObjectStore->ClearInternal(nullptr, getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBObjectStoreRequestParent::Count(const CountParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TCountParams);

  const ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange keyRangeUnion =
    aParams.optionalKeyRange();

  nsRefPtr<IDBKeyRange> keyRange;

  switch (keyRangeUnion.type()) {
    case ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange::TKeyRange:
      keyRange =
        IDBKeyRange::FromSerializedKeyRange(keyRangeUnion.get_KeyRange());
      break;

    case ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_NOT_REACHED("Unknown param type!");
      return false;
  }

  nsRefPtr<IDBRequest> request;

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    nsresult rv =
      mObjectStore->CountInternal(keyRange, nullptr, getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBObjectStoreRequestParent::OpenCursor(const OpenCursorParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenCursorParams);

  const ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange keyRangeUnion =
    aParams.optionalKeyRange();

  nsRefPtr<IDBKeyRange> keyRange;

  switch (keyRangeUnion.type()) {
    case ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange::TKeyRange:
      keyRange =
        IDBKeyRange::FromSerializedKeyRange(keyRangeUnion.get_KeyRange());
      break;

    case ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_NOT_REACHED("Unknown param type!");
      return false;
  }

  size_t direction = static_cast<size_t>(aParams.direction());

  nsRefPtr<IDBRequest> request;

  {
    AutoSetCurrentTransaction asct(mObjectStore->Transaction());

    nsresult rv =
      mObjectStore->OpenCursorInternal(keyRange, direction, nullptr,
                                       getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

/*******************************************************************************
 * IndexedDBIndexRequestParent
 ******************************************************************************/

IndexedDBIndexRequestParent::IndexedDBIndexRequestParent(
                                                       IDBIndex* aIndex,
                                                       RequestType aRequestType)
: mIndex(aIndex), mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBIndexRequestParent);
  MOZ_ASSERT(aIndex);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBIndexRequestParent::~IndexedDBIndexRequestParent()
{
  MOZ_COUNT_DTOR(IndexedDBIndexRequestParent);
}

bool
IndexedDBIndexRequestParent::Get(const GetParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TGetParams);

  nsRefPtr<IDBRequest> request;

  nsRefPtr<IDBKeyRange> keyRange =
    IDBKeyRange::FromSerializedKeyRange(aParams.keyRange());
  MOZ_ASSERT(keyRange);

  {
    AutoSetCurrentTransaction asct(mIndex->ObjectStore()->Transaction());

    nsresult rv = mIndex->GetInternal(keyRange, nullptr,
                                      getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBIndexRequestParent::GetKey(const GetKeyParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TGetKeyParams);

  nsRefPtr<IDBRequest> request;

  nsRefPtr<IDBKeyRange> keyRange =
    IDBKeyRange::FromSerializedKeyRange(aParams.keyRange());
  MOZ_ASSERT(keyRange);

  {
    AutoSetCurrentTransaction asct(mIndex->ObjectStore()->Transaction());

    nsresult rv = mIndex->GetKeyInternal(keyRange, nullptr,
                                         getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBIndexRequestParent::GetAll(const GetAllParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllParams);

  nsRefPtr<IDBRequest> request;

  const ipc::FIXME_Bug_521898_index::OptionalKeyRange keyRangeUnion =
    aParams.optionalKeyRange();

  nsRefPtr<IDBKeyRange> keyRange;

  switch (keyRangeUnion.type()) {
    case ipc::FIXME_Bug_521898_index::OptionalKeyRange::TKeyRange:
      keyRange =
        IDBKeyRange::FromSerializedKeyRange(keyRangeUnion.get_KeyRange());
      break;

    case ipc::FIXME_Bug_521898_index::OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_NOT_REACHED("Unknown param type!");
      return false;
  }

  {
    AutoSetCurrentTransaction asct(mIndex->ObjectStore()->Transaction());

    nsresult rv = mIndex->GetAllInternal(keyRange, aParams.limit(), nullptr,
                                         getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBIndexRequestParent::GetAllKeys(const GetAllKeysParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TGetAllKeysParams);

  nsRefPtr<IDBRequest> request;

  const ipc::FIXME_Bug_521898_index::OptionalKeyRange keyRangeUnion =
    aParams.optionalKeyRange();

  nsRefPtr<IDBKeyRange> keyRange;

  switch (keyRangeUnion.type()) {
    case ipc::FIXME_Bug_521898_index::OptionalKeyRange::TKeyRange:
      keyRange =
        IDBKeyRange::FromSerializedKeyRange(keyRangeUnion.get_KeyRange());
      break;

    case ipc::FIXME_Bug_521898_index::OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_NOT_REACHED("Unknown param type!");
      return false;
  }

  {
    AutoSetCurrentTransaction asct(mIndex->ObjectStore()->Transaction());

    nsresult rv = mIndex->GetAllKeysInternal(keyRange, aParams.limit(), nullptr,
                                             getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBIndexRequestParent::Count(const CountParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TCountParams);

  const ipc::FIXME_Bug_521898_index::OptionalKeyRange keyRangeUnion =
    aParams.optionalKeyRange();

  nsRefPtr<IDBKeyRange> keyRange;

  switch (keyRangeUnion.type()) {
    case ipc::FIXME_Bug_521898_index::OptionalKeyRange::TKeyRange:
      keyRange =
        IDBKeyRange::FromSerializedKeyRange(keyRangeUnion.get_KeyRange());
      break;

    case ipc::FIXME_Bug_521898_index::OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_NOT_REACHED("Unknown param type!");
      return false;
  }

  nsRefPtr<IDBRequest> request;

  {
    AutoSetCurrentTransaction asct(mIndex->ObjectStore()->Transaction());

    nsresult rv = mIndex->CountInternal(keyRange, nullptr,
                                        getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBIndexRequestParent::OpenCursor(const OpenCursorParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenCursorParams);

  const ipc::FIXME_Bug_521898_index::OptionalKeyRange keyRangeUnion =
    aParams.optionalKeyRange();

  nsRefPtr<IDBKeyRange> keyRange;

  switch (keyRangeUnion.type()) {
    case ipc::FIXME_Bug_521898_index::OptionalKeyRange::TKeyRange:
      keyRange =
        IDBKeyRange::FromSerializedKeyRange(keyRangeUnion.get_KeyRange());
      break;

    case ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_NOT_REACHED("Unknown param type!");
      return false;
  }

  size_t direction = static_cast<size_t>(aParams.direction());

  nsRefPtr<IDBRequest> request;

  {
    AutoSetCurrentTransaction asct(mIndex->ObjectStore()->Transaction());

    nsresult rv =
      mIndex->OpenCursorInternal(keyRange, direction, nullptr,
                                 getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

bool
IndexedDBIndexRequestParent::OpenKeyCursor(const OpenKeyCursorParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TOpenKeyCursorParams);

  const ipc::FIXME_Bug_521898_index::OptionalKeyRange keyRangeUnion =
    aParams.optionalKeyRange();

  nsRefPtr<IDBKeyRange> keyRange;

  switch (keyRangeUnion.type()) {
    case ipc::FIXME_Bug_521898_index::OptionalKeyRange::TKeyRange:
      keyRange =
        IDBKeyRange::FromSerializedKeyRange(keyRangeUnion.get_KeyRange());
      break;

    case ipc::FIXME_Bug_521898_objectstore::OptionalKeyRange::Tvoid_t:
      break;

    default:
      MOZ_NOT_REACHED("Unknown param type!");
      return false;
  }

  size_t direction = static_cast<size_t>(aParams.direction());

  nsRefPtr<IDBRequest> request;

  {
    AutoSetCurrentTransaction asct(mIndex->ObjectStore()->Transaction());

    nsresult rv =
      mIndex->OpenKeyCursorInternal(keyRange, direction, nullptr,
                                    getter_AddRefs(request));
    NS_ENSURE_SUCCESS(rv, false);
  }

  request->SetActor(this);
  mRequest.swap(request);
  return true;
}

/*******************************************************************************
 * IndexedDBCursorRequestParent
 ******************************************************************************/

IndexedDBCursorRequestParent::IndexedDBCursorRequestParent(
                                                       IDBCursor* aCursor,
                                                       RequestType aRequestType)
: mCursor(aCursor), mRequestType(aRequestType)
{
  MOZ_COUNT_CTOR(IndexedDBCursorRequestParent);
  MOZ_ASSERT(aCursor);
  MOZ_ASSERT(aRequestType > ParamsUnionType::T__None &&
             aRequestType <= ParamsUnionType::T__Last);
}

IndexedDBCursorRequestParent::~IndexedDBCursorRequestParent()
{
  MOZ_COUNT_DTOR(IndexedDBCursorRequestParent);
}

bool
IndexedDBCursorRequestParent::Continue(const ContinueParams& aParams)
{
  MOZ_ASSERT(mRequestType == ParamsUnionType::TContinueParams);

  {
    AutoSetCurrentTransaction asct(mCursor->Transaction());

    nsresult rv = mCursor->ContinueInternal(aParams.key(), aParams.count());
    NS_ENSURE_SUCCESS(rv, false);
  }

  mRequest = mCursor->Request();
  MOZ_ASSERT(mRequest);

  mRequest->SetActor(this);
  return true;
}

/*******************************************************************************
 * IndexedDBDeleteDatabaseRequestParent
 ******************************************************************************/

IndexedDBDeleteDatabaseRequestParent::IndexedDBDeleteDatabaseRequestParent(
                                                           IDBFactory* aFactory)
: mEventListener(ALLOW_THIS_IN_INITIALIZER_LIST(this)), mFactory(aFactory)
{
  MOZ_COUNT_CTOR(IndexedDBDeleteDatabaseRequestParent);
  MOZ_ASSERT(aFactory);
}

IndexedDBDeleteDatabaseRequestParent::~IndexedDBDeleteDatabaseRequestParent()
{
  MOZ_COUNT_DTOR(IndexedDBDeleteDatabaseRequestParent);
}

nsresult
IndexedDBDeleteDatabaseRequestParent::HandleEvent(nsIDOMEvent* aEvent)
{
  MOZ_ASSERT(aEvent);

  nsString type;
  nsresult rv = aEvent->GetType(type);
  NS_ENSURE_SUCCESS(rv, rv);

  if (type.EqualsASCII(BLOCKED_EVT_STR)) {
    nsCOMPtr<nsIIDBVersionChangeEvent> event = do_QueryInterface(aEvent);
    MOZ_ASSERT(event);

    PRUint64 currentVersion;
    rv = event->GetOldVersion(&currentVersion);
    NS_ENSURE_SUCCESS(rv, rv);

    if (!SendBlocked(currentVersion)) {
      return NS_ERROR_FAILURE;
    }

    return NS_OK;
  }

#ifdef DEBUG
  if (type.EqualsASCII(SUCCESS_EVT_STR)) {
    MOZ_ASSERT(NS_SUCCEEDED(mOpenRequest->GetErrorCode()));
  }
  else {
    MOZ_ASSERT(type.EqualsASCII(ERROR_EVT_STR));
    MOZ_ASSERT(NS_FAILED(mOpenRequest->GetErrorCode()));
  }
#endif

  if (!Send__delete__(this, mOpenRequest->GetErrorCode())) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
IndexedDBDeleteDatabaseRequestParent::SetOpenRequest(
                                                 IDBOpenDBRequest* aOpenRequest)
{
  MOZ_ASSERT(aOpenRequest);
  MOZ_ASSERT(!mOpenRequest);

  nsIDOMEventTarget* target = static_cast<nsIDOMEventTarget*>(aOpenRequest);

  nsresult rv = target->AddEventListener(NS_LITERAL_STRING(SUCCESS_EVT_STR),
                                         mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = target->AddEventListener(NS_LITERAL_STRING(ERROR_EVT_STR),
                                mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = target->AddEventListener(NS_LITERAL_STRING(BLOCKED_EVT_STR),
                                mEventListener, false);
  NS_ENSURE_SUCCESS(rv, rv);

  mOpenRequest = aOpenRequest;
  return NS_OK;
}

/*******************************************************************************
 * WeakEventListener
 ******************************************************************************/

 NS_IMPL_ISUPPORTS1(WeakEventListenerBase, nsIDOMEventListener)

 NS_IMETHODIMP
 WeakEventListenerBase::HandleEvent(nsIDOMEvent* aEvent)
{
  MOZ_NOT_REACHED("This must be overridden!");
  return NS_ERROR_FAILURE;
}
