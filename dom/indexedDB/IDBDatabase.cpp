/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IDBDatabase.h"

#include "DictionaryHelpers.h"
#include "mozilla/Mutex.h"
#include "mozilla/storage.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/QuotaManager.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfo.h"
#include "nsDOMLists.h"
#include "nsJSUtils.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBEvents.h"
#include "IDBFactory.h"
#include "IDBFileHandle.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IDBFactory.h"
#include "ProfilerHelpers.h"
#include "TransactionThreadPool.h"

#include "ipc/IndexedDBChild.h"
#include "ipc/IndexedDBParent.h"

#include "mozilla/dom/IDBDatabaseBinding.h"

USING_INDEXEDDB_NAMESPACE
using mozilla::dom::ContentParent;
using mozilla::dom::quota::AssertIsOnIOThread;
using mozilla::dom::quota::Client;
using mozilla::dom::quota::QuotaManager;
using namespace mozilla::dom;

namespace {

class NoRequestDatabaseHelper : public AsyncConnectionHelper
{
public:
  NoRequestDatabaseHelper(IDBTransaction* aTransaction)
  : AsyncConnectionHelper(aTransaction, nullptr)
  {
    NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    NS_ASSERTION(aTransaction, "Null transaction!");
  }

  virtual ChildProcessSendResult
  SendResponseToChildProcess(nsresult aResultCode) MOZ_OVERRIDE;

  virtual nsresult
  UnpackResponseFromParentProcess(const ResponseValue& aResponseValue)
                                  MOZ_OVERRIDE;

  virtual nsresult OnSuccess() MOZ_OVERRIDE;

  virtual void OnError() MOZ_OVERRIDE;
};

class CreateObjectStoreHelper : public NoRequestDatabaseHelper
{
public:
  CreateObjectStoreHelper(IDBTransaction* aTransaction,
                          IDBObjectStore* aObjectStore)
  : NoRequestDatabaseHelper(aTransaction), mObjectStore(aObjectStore)
  { }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

  virtual void ReleaseMainThreadObjects() MOZ_OVERRIDE;

private:
  nsRefPtr<IDBObjectStore> mObjectStore;
};

class DeleteObjectStoreHelper : public NoRequestDatabaseHelper
{
public:
  DeleteObjectStoreHelper(IDBTransaction* aTransaction,
                          int64_t aObjectStoreId)
  : NoRequestDatabaseHelper(aTransaction), mObjectStoreId(aObjectStoreId)
  { }

  virtual nsresult DoDatabaseWork(mozIStorageConnection* aConnection)
                                  MOZ_OVERRIDE;

private:
  // In-params.
  int64_t mObjectStoreId;
};

class CreateFileHelper : public AsyncConnectionHelper
{
public:
  CreateFileHelper(IDBDatabase* aDatabase,
                   IDBRequest* aRequest,
                   const nsAString& aName,
                   const nsAString& aType)
  : AsyncConnectionHelper(aDatabase, aRequest),
    mName(aName), mType(aType)
  { }

  ~CreateFileHelper()
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);
  nsresult GetSuccessResult(JSContext* aCx,
                            JS::MutableHandle<JS::Value> aVal);
  void ReleaseMainThreadObjects()
  {
    mFileInfo = nullptr;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

  virtual ChildProcessSendResult SendResponseToChildProcess(
                                                           nsresult aResultCode)
                                                           MOZ_OVERRIDE
  {
    return Success_NotSent;
  }

  virtual nsresult UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
                                            MOZ_OVERRIDE
  {
    MOZ_CRASH("Should never get here!");
  }

private:
  // In-params.
  nsString mName;
  nsString mType;

  // Out-params.
  nsRefPtr<FileInfo> mFileInfo;
};

class MOZ_STACK_CLASS AutoRemoveObjectStore
{
public:
  AutoRemoveObjectStore(DatabaseInfo* aInfo, const nsAString& aName)
  : mInfo(aInfo), mName(aName)
  { }

  ~AutoRemoveObjectStore()
  {
    if (mInfo) {
      mInfo->RemoveObjectStore(mName);
    }
  }

  void forget()
  {
    mInfo = nullptr;
  }

private:
  DatabaseInfo* mInfo;
  nsString mName;
};

} // anonymous namespace

// static
already_AddRefed<IDBDatabase>
IDBDatabase::Create(IDBWrapperCache* aOwnerCache,
                    IDBFactory* aFactory,
                    already_AddRefed<DatabaseInfo> aDatabaseInfo,
                    const nsACString& aASCIIOrigin,
                    FileManager* aFileManager,
                    mozilla::dom::ContentParent* aContentParent)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aFactory, "Null pointer!");
  NS_ASSERTION(!aASCIIOrigin.IsEmpty(), "Empty origin!");

  nsRefPtr<DatabaseInfo> databaseInfo(aDatabaseInfo);
  NS_ASSERTION(databaseInfo, "Null pointer!");

  nsRefPtr<IDBDatabase> db(new IDBDatabase());

  db->BindToOwner(aOwnerCache);
  db->SetScriptOwner(aOwnerCache->GetScriptOwner());
  db->mFactory = aFactory;
  db->mDatabaseId = databaseInfo->id;
  db->mName = databaseInfo->name;
  db->mFilePath = databaseInfo->filePath;
  databaseInfo.swap(db->mDatabaseInfo);
  db->mASCIIOrigin = aASCIIOrigin;
  db->mFileManager = aFileManager;
  db->mContentParent = aContentParent;

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "This should never be null!");

  db->mQuotaClient = quotaManager->GetClient(Client::IDB);
  NS_ASSERTION(db->mQuotaClient, "This shouldn't fail!");

  if (!quotaManager->RegisterStorage(db)) {
    // Either out of memory or shutting down.
    return nullptr;
  }

  db->mRegistered = true;

  return db.forget();
}

// static
IDBDatabase*
IDBDatabase::FromStorage(nsIOfflineStorage* aStorage)
{
  return aStorage->GetClient()->GetType() == Client::IDB ?
         static_cast<IDBDatabase*>(aStorage) : nullptr;
}

IDBDatabase::IDBDatabase()
: mDatabaseId(0),
  mActorChild(nullptr),
  mActorParent(nullptr),
  mContentParent(nullptr),
  mInvalidated(false),
  mRegistered(false),
  mClosed(false),
  mRunningVersionChange(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBDatabase::~IDBDatabase()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

void
IDBDatabase::LastRelease()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  NS_ASSERTION(!mActorParent, "Actor parent owns us, how can we be dying?!");
  if (mActorChild) {
    NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorChild->Send__delete__(mActorChild);
    NS_ASSERTION(!mActorChild, "Should have cleared in Send__delete__!");
  }

  if (mRegistered) {
    CloseInternal(true);

    QuotaManager* quotaManager = QuotaManager::Get();
    if (quotaManager) {
      quotaManager->UnregisterStorage(this);
    }
    mRegistered = false;
  }
}

NS_IMETHODIMP_(void)
IDBDatabase::Invalidate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IsInvalidated()) {
    return;
  }

  mInvalidated = true;

  // Make sure we're closed too.
  Close();

  // When the IndexedDatabaseManager needs to invalidate databases, all it has
  // is an origin, so we call into the quota manager here to cancel any prompts
  // for our owner.
  nsPIDOMWindow* owner = GetOwner();
  if (owner) {
    QuotaManager::CancelPromptsForWindow(owner);
  }

  DatabaseInfo::Remove(mDatabaseId);

  // And let the child process know as well.
  if (mActorParent) {
    NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
    mActorParent->Invalidate();
  }
}

void
IDBDatabase::DisconnectFromActorParent()
{
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Make sure we're closed too.
  Close();

  // Kill any outstanding prompts.
  nsPIDOMWindow* owner = GetOwner();
  if (owner) {
    QuotaManager::CancelPromptsForWindow(owner);
  }
}

void
IDBDatabase::CloseInternal(bool aIsDead)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mClosed) {
    mClosed = true;

    // If we're getting called from Unlink, avoid cloning the DatabaseInfo.
    {
      nsRefPtr<DatabaseInfo> previousInfo;
      mDatabaseInfo.swap(previousInfo);

      if (!aIsDead) {
        nsRefPtr<DatabaseInfo> clonedInfo = previousInfo->Clone();

        clonedInfo.swap(mDatabaseInfo);
      }
    }

    QuotaManager* quotaManager = QuotaManager::Get();
    if (quotaManager) {
      quotaManager->OnStorageClosed(this);
    }

    // And let the parent process know as well.
    if (mActorChild && !IsInvalidated()) {
      NS_ASSERTION(!IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
      mActorChild->SendClose(aIsDead);
    }
  }
}

NS_IMETHODIMP_(bool)
IDBDatabase::IsClosed()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return mClosed;
}

void
IDBDatabase::EnterSetVersionTransaction()
{
  NS_ASSERTION(!mRunningVersionChange, "How did that happen?");

  mPreviousDatabaseInfo = mDatabaseInfo->Clone();

  mRunningVersionChange = true;
}

void
IDBDatabase::ExitSetVersionTransaction()
{
  NS_ASSERTION(mRunningVersionChange, "How did that happen?");

  mPreviousDatabaseInfo = nullptr;

  mRunningVersionChange = false;
}

void
IDBDatabase::RevertToPreviousState()
{
  mDatabaseInfo = mPreviousDatabaseInfo;
  mPreviousDatabaseInfo = nullptr;
}

void
IDBDatabase::OnUnlink()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // We've been unlinked, at the very least we should be able to prevent further
  // transactions from starting and unblock any other SetVersion callers.
  CloseInternal(true);

  // No reason for the QuotaManager to track us any longer.
  QuotaManager* quotaManager = QuotaManager::Get();
  if (quotaManager) {
    quotaManager->UnregisterStorage(this);

    // Don't try to unregister again in the destructor.
    mRegistered = false;
  }
}

nsresult
IDBDatabase::CreateObjectStoreInternal(IDBTransaction* aTransaction,
                                       const ObjectStoreInfoGuts& aInfo,
                                       IDBObjectStore** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aTransaction, "Null transaction!");

  DatabaseInfo* databaseInfo = aTransaction->DBInfo();

  nsRefPtr<ObjectStoreInfo> newInfo = new ObjectStoreInfo();
  *static_cast<ObjectStoreInfoGuts*>(newInfo.get()) = aInfo;

  newInfo->nextAutoIncrementId = aInfo.autoIncrement ? 1 : 0;
  newInfo->comittedAutoIncrementId = newInfo->nextAutoIncrementId;

  if (!databaseInfo->PutObjectStore(newInfo)) {
    NS_WARNING("Put failed!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  // Don't leave this in the hash if we fail below!
  AutoRemoveObjectStore autoRemove(databaseInfo, newInfo->name);

  nsRefPtr<IDBObjectStore> objectStore =
    aTransaction->GetOrCreateObjectStore(newInfo->name, newInfo, true);
  NS_ENSURE_TRUE(objectStore, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  if (IndexedDatabaseManager::IsMainProcess()) {
    nsRefPtr<CreateObjectStoreHelper> helper =
      new CreateObjectStoreHelper(aTransaction, objectStore);

    nsresult rv = helper->DispatchToTransactionPool();
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  autoRemove.forget();

  IDB_PROFILER_MARK("IndexedDB Pseudo-request: "
                    "database(%s).transaction(%s).createObjectStore(%s)",
                    "MT IDBDatabase.createObjectStore()",
                    IDB_PROFILER_STRING(this),
                    IDB_PROFILER_STRING(aTransaction),
                    IDB_PROFILER_STRING(objectStore));

  objectStore.forget(_retval);

  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBDatabase, IDBWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFactory)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBDatabase, IDBWrapperCache)
  // Don't unlink mFactory!

  // Do some cleanup.
  tmp->OnUnlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBDatabase)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabase)
  NS_INTERFACE_MAP_ENTRY(nsIFileStorage)
  NS_INTERFACE_MAP_ENTRY(nsIOfflineStorage)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabase)
NS_INTERFACE_MAP_END_INHERITING(IDBWrapperCache)

NS_IMPL_ADDREF_INHERITED(IDBDatabase, IDBWrapperCache)
NS_IMPL_RELEASE_INHERITED(IDBDatabase, IDBWrapperCache)

DOMCI_DATA(IDBDatabase, IDBDatabase)

NS_IMPL_EVENT_HANDLER(IDBDatabase, abort)
NS_IMPL_EVENT_HANDLER(IDBDatabase, error)
NS_IMPL_EVENT_HANDLER(IDBDatabase, versionchange)

NS_IMETHODIMP
IDBDatabase::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::GetVersion(uint64_t* aVersion)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  DatabaseInfo* info = Info();
  *aVersion = info->version;

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::GetObjectStoreNames(nsIDOMDOMStringList** aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  DatabaseInfo* info = Info();

  nsAutoTArray<nsString, 10> objectStoreNames;
  if (!info->GetObjectStoreNames(objectStoreNames)) {
    NS_WARNING("Couldn't get names!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  uint32_t count = objectStoreNames.Length();
  for (uint32_t index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(objectStoreNames[index]),
                   NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  list.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::CreateObjectStore(const nsAString& aName,
                               const jsval& aOptions,
                               JSContext* aCx,
                               nsIIDBObjectStore** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = AsyncConnectionHelper::GetCurrentTransaction();

  if (!transaction ||
      transaction->GetMode() != IDBTransaction::VERSION_CHANGE) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  DatabaseInfo* databaseInfo = transaction->DBInfo();

  RootedDictionary<IDBObjectStoreParameters> params(aCx);
  JS::Rooted<JS::Value> options(aCx, aOptions);
  if (!params.Init(aCx, options)) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  KeyPath keyPath(0);
  if (NS_FAILED(KeyPath::Parse(aCx, params.mKeyPath, &keyPath))) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  if (databaseInfo->ContainsStoreName(aName)) {
    return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
  }

  if (!keyPath.IsAllowedForObjectStore(params.mAutoIncrement)) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  ObjectStoreInfoGuts guts;

  guts.name = aName;
  guts.id = databaseInfo->nextObjectStoreId++;
  guts.keyPath = keyPath;
  guts.autoIncrement = params.mAutoIncrement;

  nsRefPtr<IDBObjectStore> objectStore;
  nsresult rv = CreateObjectStoreInternal(transaction, guts,
                                          getter_AddRefs(objectStore));
  NS_ENSURE_SUCCESS(rv, rv);

  objectStore.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::DeleteObjectStore(const nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = AsyncConnectionHelper::GetCurrentTransaction();

  if (!transaction ||
      transaction->GetMode() != IDBTransaction::VERSION_CHANGE) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  DatabaseInfo* info = transaction->DBInfo();
  ObjectStoreInfo* objectStoreInfo = info->GetObjectStore(aName);
  if (!objectStoreInfo) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR;
  }

  nsresult rv;

  if (IndexedDatabaseManager::IsMainProcess()) {
    nsRefPtr<DeleteObjectStoreHelper> helper =
      new DeleteObjectStoreHelper(transaction, objectStoreInfo->id);

    rv = helper->DispatchToTransactionPool();
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else {
    IndexedDBTransactionChild* actor = transaction->GetActorChild();
    NS_ASSERTION(actor, "Must have an actor here!");

    actor->SendDeleteObjectStore(nsString(aName));
  }

  transaction->RemoveObjectStore(aName);

  IDB_PROFILER_MARK("IndexedDB Pseudo-request: "
                    "database(%s).transaction(%s).deleteObjectStore(\"%s\")",
                    "MT IDBDatabase.deleteObjectStore()",
                    IDB_PROFILER_STRING(this),
                    IDB_PROFILER_STRING(transaction),
                    NS_ConvertUTF16toUTF8(aName).get());

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::Transaction(const jsval& aStoreNames,
                         const nsAString& aMode,
                         JSContext* aCx,
                         uint8_t aOptionalArgCount,
                         nsISupports** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (QuotaManager::IsShuttingDown()) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (mClosed) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  if (mRunningVersionChange) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  IDBTransaction::Mode transactionMode = IDBTransaction::READ_ONLY;
  if (aOptionalArgCount) {
    if (aMode.EqualsLiteral("readwrite")) {
      transactionMode = IDBTransaction::READ_WRITE;
    }
    else if (!aMode.EqualsLiteral("readonly")) {
      return NS_ERROR_TYPE_ERR;
    }
  }

  nsresult rv;
  nsTArray<nsString> storesToOpen;

  if (!JSVAL_IS_PRIMITIVE(aStoreNames)) {
    JS::Rooted<JSObject*> obj(aCx, JSVAL_TO_OBJECT(aStoreNames));

    // See if this is a JS array.
    if (JS_IsArrayObject(aCx, obj)) {
      uint32_t length;
      if (!JS_GetArrayLength(aCx, obj, &length)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      if (!length) {
        return NS_ERROR_DOM_INVALID_ACCESS_ERR;
      }

      storesToOpen.SetCapacity(length);

      for (uint32_t index = 0; index < length; index++) {
        JS::Rooted<JS::Value> val(aCx);
        JSString* jsstr;
        nsDependentJSString str;
        if (!JS_GetElement(aCx, obj, index, val.address()) ||
            !(jsstr = JS_ValueToString(aCx, val)) ||
            !str.init(aCx, jsstr)) {
          return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
        }

        storesToOpen.AppendElement(str);
      }

      NS_ASSERTION(!storesToOpen.IsEmpty(),
                   "Must have something here or else code below will "
                   "misbehave!");
    }
    else {
      // Perhaps some kind of wrapped object?
      nsIXPConnect* xpc = nsContentUtils::XPConnect();
      NS_ASSERTION(xpc, "This should never be null!");

      nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
      rv = xpc->GetWrappedNativeOfJSObject(aCx, obj, getter_AddRefs(wrapper));
      NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

      if (wrapper) {
        nsISupports* wrappedObject = wrapper->Native();
        NS_ENSURE_TRUE(wrappedObject, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

        // We only accept DOMStringList.
        nsCOMPtr<nsIDOMDOMStringList> list = do_QueryInterface(wrappedObject);
        if (list) {
          uint32_t length;
          rv = list->GetLength(&length);
          NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

          if (!length) {
            return NS_ERROR_DOM_INVALID_ACCESS_ERR;
          }

          storesToOpen.SetCapacity(length);

          for (uint32_t index = 0; index < length; index++) {
            nsString* item = storesToOpen.AppendElement();
            NS_ASSERTION(item, "This should never fail!");

            rv = list->Item(index, *item);
            NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
          }

          NS_ASSERTION(!storesToOpen.IsEmpty(),
                       "Must have something here or else code below will "
                       "misbehave!");
        }
      }
    }
  }

  // If our list is empty here then the argument must have been an object that
  // we don't support or a primitive. Either way we convert to a string.
  if (storesToOpen.IsEmpty()) {
    JSString* jsstr;
    nsDependentJSString str;
    if (!(jsstr = JS_ValueToString(aCx, aStoreNames)) ||
        !str.init(aCx, jsstr)) {
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    storesToOpen.AppendElement(str);
  }

  // Now check to make sure the object store names we collected actually exist.
  DatabaseInfo* info = Info();
  for (uint32_t index = 0; index < storesToOpen.Length(); index++) {
    if (!info->ContainsStoreName(storesToOpen[index])) {
      return NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR;
    }
  }

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, storesToOpen, transactionMode, false);
  NS_ENSURE_TRUE(transaction, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  IDB_PROFILER_MARK("IndexedDB Transaction %llu: database(%s).transaction(%s)",
                    "IDBTransaction[%llu] MT Started",
                    transaction->GetSerialNumber(), IDB_PROFILER_STRING(this),
                    IDB_PROFILER_STRING(transaction));

  nsRefPtr<IDBWrapperCache> tmp = transaction.get();
  tmp.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::MozCreateFileHandle(const nsAString& aName,
                                 const nsAString& aType,
                                 JSContext* aCx,
                                 nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!IndexedDatabaseManager::IsMainProcess()) {
    NS_WARNING("Not supported yet!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (QuotaManager::IsShuttingDown()) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (mClosed) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  nsRefPtr<IDBRequest> request = IDBRequest::Create(nullptr, this, nullptr);

  nsRefPtr<CreateFileHelper> helper =
    new CreateFileHelper(this, request, aName, aType);

  QuotaManager* quotaManager = QuotaManager::Get();
  NS_ASSERTION(quotaManager, "We should definitely have a manager here");

  nsresult rv = helper->Dispatch(quotaManager->IOThread());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  CloseInternal(false);

  NS_ASSERTION(mClosed, "Should have set the closed flag!");
  return NS_OK;
}

NS_IMETHODIMP_(nsIAtom*)
IDBDatabase::Id()
{
  return mDatabaseId;
}

NS_IMETHODIMP_(bool)
IDBDatabase::IsInvalidated()
{
  return mInvalidated;
}

NS_IMETHODIMP_(bool)
IDBDatabase::IsShuttingDown()
{
  return QuotaManager::IsShuttingDown();
}

NS_IMETHODIMP_(void)
IDBDatabase::SetThreadLocals()
{
  NS_ASSERTION(GetOwner(), "Should have owner!");
  QuotaManager::SetCurrentWindow(GetOwner());
}

NS_IMETHODIMP_(void)
IDBDatabase::UnsetThreadLocals()
{
  QuotaManager::SetCurrentWindow(nullptr);
}

NS_IMETHODIMP_(mozilla::dom::quota::Client*)
IDBDatabase::GetClient()
{
  return mQuotaClient;
}

NS_IMETHODIMP_(bool)
IDBDatabase::IsOwned(nsPIDOMWindow* aOwner)
{
  return GetOwner() == aOwner;
}

NS_IMETHODIMP_(const nsACString&)
IDBDatabase::Origin()
{
  return mASCIIOrigin;
}

nsresult
IDBDatabase::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  return IndexedDatabaseManager::FireWindowOnError(GetOwner(), aVisitor);
}

AsyncConnectionHelper::ChildProcessSendResult
NoRequestDatabaseHelper::SendResponseToChildProcess(nsresult aResultCode)
{
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  return Success_NotSent;
}

nsresult
NoRequestDatabaseHelper::UnpackResponseFromParentProcess(
                                            const ResponseValue& aResponseValue)
{
  MOZ_CRASH("Should never get here!");
}

nsresult
NoRequestDatabaseHelper::OnSuccess()
{
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  return NS_OK;
}

void
NoRequestDatabaseHelper::OnError()
{
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");
  mTransaction->Abort(GetResultCode());
}

nsresult
CreateObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("IndexedDB", "CreateObjectStoreHelper::DoDatabaseWork");

  if (IndexedDatabaseManager::InLowDiskSpaceMode()) {
    NS_WARNING("Refusing to create additional objectStore because disk space "
               "is low!");
    return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
  }

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(NS_LITERAL_CSTRING(
    "INSERT INTO object_store (id, auto_increment, name, key_path) "
    "VALUES (:id, :auto_increment, :name, :key_path)"
  ));
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                       mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mObjectStore->IsAutoIncrement() ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mObjectStore->Name());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  const KeyPath& keyPath = mObjectStore->GetKeyPath();
  if (keyPath.IsValid()) {
    nsAutoString keyPathSerialization;
    keyPath.SerializeToString(keyPathSerialization);
    rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"),
                                keyPathSerialization);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }
  else {
    rv = stmt->BindNullByName(NS_LITERAL_CSTRING("key_path"));
    NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);
  }

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

void
CreateObjectStoreHelper::ReleaseMainThreadObjects()
{
  mObjectStore = nullptr;
  NoRequestDatabaseHelper::ReleaseMainThreadObjects();
}

nsresult
DeleteObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("IndexedDB", "DeleteObjectStoreHelper::DoDatabaseWork");

  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(NS_LITERAL_CSTRING(
    "DELETE FROM object_store "
    "WHERE id = :id "
  ));
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mObjectStoreId);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
CreateFileHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  AssertIsOnIOThread();
  NS_ASSERTION(IndexedDatabaseManager::IsMainProcess(), "Wrong process!");

  PROFILER_LABEL("IndexedDB", "CreateFileHelper::DoDatabaseWork");

  if (IndexedDatabaseManager::InLowDiskSpaceMode()) {
    NS_WARNING("Refusing to create file because disk space is low!");
    return NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR;
  }

  FileManager* fileManager = mDatabase->Manager();

  mFileInfo = fileManager->GetNewFileInfo();
  NS_ENSURE_TRUE(mFileInfo, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  const int64_t& fileId = mFileInfo->Id();

  nsCOMPtr<nsIFile> directory = fileManager->EnsureJournalDirectory();
  NS_ENSURE_TRUE(directory, NS_ERROR_FAILURE);

  nsCOMPtr<nsIFile> file = fileManager->GetFileForId(directory, fileId);
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  nsresult rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  NS_ENSURE_SUCCESS(rv, rv);

  directory = fileManager->GetDirectory();
  NS_ENSURE_TRUE(directory, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  file = fileManager->GetFileForId(directory, fileId);
  NS_ENSURE_TRUE(file, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0644);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
CreateFileHelper::GetSuccessResult(JSContext* aCx,
                                   JS::MutableHandle<JS::Value> aVal)
{
  nsRefPtr<IDBFileHandle> fileHandle =
    IDBFileHandle::Create(mDatabase, mName, mType, mFileInfo.forget());
  NS_ENSURE_TRUE(fileHandle, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return WrapNative(aCx, NS_ISUPPORTS_CAST(nsIDOMFileHandle*, fileHandle),
                    aVal);
}
