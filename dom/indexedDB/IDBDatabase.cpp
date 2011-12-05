/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "IDBDatabase.h"

#include "jscntxt.h"
#include "mozilla/Mutex.h"
#include "mozilla/storage.h"
#include "nsDOMClassInfo.h"
#include "nsEventDispatcher.h"
#include "nsJSUtils.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "CheckQuotaHelper.h"
#include "DatabaseInfo.h"
#include "IDBEvents.h"
#include "IDBIndex.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IDBFactory.h"
#include "IndexedDatabaseManager.h"
#include "LazyIdleThread.h"
#include "TransactionThreadPool.h"

USING_INDEXEDDB_NAMESPACE

namespace {

class CreateObjectStoreHelper : public AsyncConnectionHelper
{
public:
  CreateObjectStoreHelper(IDBTransaction* aTransaction,
                          IDBObjectStore* aObjectStore)
  : AsyncConnectionHelper(aTransaction, nsnull), mObjectStore(aObjectStore)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);

  nsresult OnSuccess()
  {
    return NS_OK;
  }

  void OnError()
  {
    NS_ASSERTION(mTransaction->IsAborted(), "How else can this fail?!");
  }

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  nsRefPtr<IDBObjectStore> mObjectStore;
};

class DeleteObjectStoreHelper : public AsyncConnectionHelper
{
public:
  DeleteObjectStoreHelper(IDBTransaction* aTransaction,
                          PRInt64 aObjectStoreId)
  : AsyncConnectionHelper(aTransaction, nsnull), mObjectStoreId(aObjectStoreId)
  { }

  nsresult DoDatabaseWork(mozIStorageConnection* aConnection);

  nsresult OnSuccess()
  {
    return NS_OK;
  }

  void OnError()
  {
    NS_ASSERTION(mTransaction->IsAborted(), "How else can this fail?!");
  }

private:
  // In-params.
  PRInt64 mObjectStoreId;
};

NS_STACK_CLASS
class AutoRemoveObjectStore
{
public:
  AutoRemoveObjectStore(IDBDatabase* aDatabase, const nsAString& aName)
  : mDatabase(aDatabase), mName(aName)
  { }

  ~AutoRemoveObjectStore()
  {
    if (mDatabase) {
      mDatabase->Info()->RemoveObjectStore(mName);
    }
  }

  void forget()
  {
    mDatabase = nsnull;
  }

private:
  IDBDatabase* mDatabase;
  nsString mName;
};

} // anonymous namespace

// static
already_AddRefed<IDBDatabase>
IDBDatabase::Create(nsIScriptContext* aScriptContext,
                    nsPIDOMWindow* aOwner,
                    already_AddRefed<DatabaseInfo> aDatabaseInfo,
                    const nsACString& aASCIIOrigin)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!aASCIIOrigin.IsEmpty(), "Empty origin!");

  nsRefPtr<DatabaseInfo> databaseInfo(aDatabaseInfo);
  NS_ASSERTION(databaseInfo, "Null pointer!");

  nsRefPtr<IDBDatabase> db(new IDBDatabase());

  db->mScriptContext = aScriptContext;
  db->mOwner = aOwner;

  db->mDatabaseId = databaseInfo->id;
  db->mName = databaseInfo->name;
  db->mFilePath = databaseInfo->filePath;
  databaseInfo.swap(db->mDatabaseInfo);
  db->mASCIIOrigin = aASCIIOrigin;

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  if (!mgr->RegisterDatabase(db)) {
    // Either out of memory or shutting down.
    return nsnull;
  }

  return db.forget();
}

IDBDatabase::IDBDatabase()
: mDatabaseId(0),
  mInvalidated(0),
  mRegistered(false),
  mClosed(false),
  mRunningVersionChange(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
}

IDBDatabase::~IDBDatabase()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mRegistered) {
    CloseInternal(true);

    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    if (mgr) {
      mgr->UnregisterDatabase(this);
    }
  }

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }
}

void
IDBDatabase::Invalidate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  // Make sure we're closed too.
  Close();

  // When the IndexedDatabaseManager needs to invalidate databases, all it has
  // is an origin, so we call back into the manager to cancel any prompts for
  // our owner.
  IndexedDatabaseManager::CancelPromptsForWindow(Owner());

  mInvalidated = true;
}

bool
IDBDatabase::IsInvalidated()
{
  return !!mInvalidated;
}

void
IDBDatabase::CloseInternal(bool aIsDead)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mClosed) {
    // If we're getting called from Unlink, avoid cloning the DatabaseInfo.
    {
      nsRefPtr<DatabaseInfo> previousInfo;
      mDatabaseInfo.swap(previousInfo);

      if (!aIsDead) {
        nsRefPtr<DatabaseInfo> clonedInfo = previousInfo->Clone();

        clonedInfo.swap(mDatabaseInfo);
      }
    }

    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    if (mgr) {
      mgr->OnDatabaseClosed(this);
    }
    mClosed = true;
  }
}

bool
IDBDatabase::IsClosed()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return mClosed;
}

void
IDBDatabase::EnterSetVersionTransaction()
{
  NS_ASSERTION(!mRunningVersionChange, "How did that happen?");
  mRunningVersionChange = true;
}

void
IDBDatabase::ExitSetVersionTransaction()
{
  NS_ASSERTION(mRunningVersionChange, "How did that happen?");
  mRunningVersionChange = false;
}

void
IDBDatabase::OnUnlink()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mOwner, "Should have been cleared already!");

  // We've been unlinked, at the very least we should be able to prevent further
  // transactions from starting and unblock any other SetVersion callers.
  CloseInternal(true);

  // No reason for the IndexedDatabaseManager to track us any longer.
  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  if (mgr) {
    mgr->UnregisterDatabase(this);

    // Don't try to unregister again in the destructor.
    mRegistered = false;
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBDatabase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(IDBDatabase,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnVersionChangeListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBDatabase,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnAbortListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnVersionChangeListener)

  // Do some cleanup.
  tmp->OnUnlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(IDBDatabase)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabase)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabase)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(IDBDatabase, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(IDBDatabase, nsDOMEventTargetHelper)

DOMCI_DATA(IDBDatabase, IDBDatabase)

NS_IMETHODIMP
IDBDatabase::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::GetVersion(PRUint64* aVersion)
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
  PRUint32 count = objectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
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
      transaction->Mode() != nsIIDBTransaction::VERSION_CHANGE) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  DatabaseInfo* databaseInfo = Info();

  if (databaseInfo->ContainsStoreName(aName)) {
    return NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR;
  }

  nsString keyPath;
  bool autoIncrement = false;

  if (!JSVAL_IS_VOID(aOptions) && !JSVAL_IS_NULL(aOptions)) {
    if (JSVAL_IS_PRIMITIVE(aOptions)) {
      // XXX This isn't the right error
      return NS_ERROR_DOM_TYPE_ERR;
    }

    NS_ASSERTION(JSVAL_IS_OBJECT(aOptions), "Huh?!");
    JSObject* options = JSVAL_TO_OBJECT(aOptions);

    jsval val;
    if (!JS_GetPropertyById(aCx, options, nsDOMClassInfo::sKeyPath_id, &val)) {
      NS_WARNING("JS_GetPropertyById failed!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    if (!JSVAL_IS_VOID(val) && !JSVAL_IS_NULL(val)) {
      JSString* str = JS_ValueToString(aCx, val);
      if (!str) {
        NS_WARNING("JS_ValueToString failed!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
      nsDependentJSString dependentKeyPath;
      if (!dependentKeyPath.init(aCx, str)) {
        NS_WARNING("Initializing keyPath failed!");
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }
      keyPath = dependentKeyPath;
    }

    if (!JS_GetPropertyById(aCx, options, nsDOMClassInfo::sAutoIncrement_id,
                            &val)) {
      NS_WARNING("JS_GetPropertyById failed!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }

    JSBool boolVal;
    if (!JS_ValueToBoolean(aCx, val, &boolVal)) {
      NS_WARNING("JS_ValueToBoolean failed!");
      return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
    }
    autoIncrement = !!boolVal;
  }

  if (!IDBObjectStore::IsValidKeyPath(aCx, keyPath)) {
    return NS_ERROR_DOM_SYNTAX_ERR;
  }

  nsAutoPtr<ObjectStoreInfo> newInfo(new ObjectStoreInfo());

  newInfo->name = aName;
  newInfo->id = databaseInfo->nextObjectStoreId++;
  newInfo->keyPath = keyPath;
  newInfo->autoIncrement = autoIncrement;
  newInfo->databaseId = mDatabaseId;

  if (!Info()->PutObjectStore(newInfo)) {
    NS_WARNING("Put failed!");
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }
  ObjectStoreInfo* objectStoreInfo = newInfo.forget();

  // Don't leave this in the hash if we fail below!
  AutoRemoveObjectStore autoRemove(this, aName);

  nsRefPtr<IDBObjectStore> objectStore =
    transaction->GetOrCreateObjectStore(aName, objectStoreInfo);
  NS_ENSURE_TRUE(objectStore, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  nsRefPtr<CreateObjectStoreHelper> helper =
    new CreateObjectStoreHelper(transaction, objectStore);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  autoRemove.forget();

  objectStore.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::DeleteObjectStore(const nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  IDBTransaction* transaction = AsyncConnectionHelper::GetCurrentTransaction();

  if (!transaction ||
      transaction->Mode() != nsIIDBTransaction::VERSION_CHANGE) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  DatabaseInfo* info = Info();
  ObjectStoreInfo* objectStoreInfo;
  if (!info->GetObjectStore(aName, &objectStoreInfo)) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR;
  }

  nsRefPtr<DeleteObjectStoreHelper> helper =
    new DeleteObjectStoreHelper(transaction, objectStoreInfo->id);
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  info->RemoveObjectStore(aName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::Transaction(const jsval& aStoreNames,
                         PRUint16 aMode,
                         JSContext* aCx,
                         PRUint8 aOptionalArgCount,
                         nsIIDBTransaction** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IndexedDatabaseManager::IsShuttingDown()) {
    return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
  }

  if (mClosed) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  if (mRunningVersionChange) {
    return NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR;
  }

  if (aOptionalArgCount) {
    if (aMode != nsIIDBTransaction::READ_WRITE &&
        aMode != nsIIDBTransaction::READ_ONLY) {
      return NS_ERROR_DOM_INDEXEDDB_NON_TRANSIENT_ERR;
    }
  }
  else {
    aMode = nsIIDBTransaction::READ_ONLY;
  }

  nsresult rv;
  nsTArray<nsString> storesToOpen;

  if (!JSVAL_IS_PRIMITIVE(aStoreNames)) {
    JSObject* obj = JSVAL_TO_OBJECT(aStoreNames);

    // See if this is a JS array.
    if (JS_IsArrayObject(aCx, obj)) {
      jsuint length;
      if (!JS_GetArrayLength(aCx, obj, &length)) {
        return NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR;
      }

      if (!length) {
        return NS_ERROR_DOM_INVALID_ACCESS_ERR;
      }

      storesToOpen.SetCapacity(length);

      for (jsuint index = 0; index < length; index++) {
        jsval val;
        JSString* jsstr;
        nsDependentJSString str;
        if (!JS_GetElement(aCx, obj, index, &val) ||
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
          PRUint32 length;
          rv = list->GetLength(&length);
          NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

          if (!length) {
            return NS_ERROR_DOM_INVALID_ACCESS_ERR;
          }

          storesToOpen.SetCapacity(length);

          for (PRUint32 index = 0; index < length; index++) {
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
  for (PRUint32 index = 0; index < storesToOpen.Length(); index++) {
    if (!info->ContainsStoreName(storesToOpen[index])) {
      return NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR;
    }
  }

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, storesToOpen, aMode, false);
  NS_ENSURE_TRUE(transaction, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  transaction.forget(_retval);
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

NS_IMETHODIMP
IDBDatabase::SetOnabort(nsIDOMEventListener* aAbortListener)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(ABORT_EVT_STR),
                                mOnAbortListener, aAbortListener);
}

NS_IMETHODIMP
IDBDatabase::GetOnabort(nsIDOMEventListener** aAbortListener)
{
  return GetInnerEventListener(mOnAbortListener, aAbortListener);
}

NS_IMETHODIMP
IDBDatabase::SetOnerror(nsIDOMEventListener* aErrorListener)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(ERROR_EVT_STR),
                                mOnErrorListener, aErrorListener);
}

NS_IMETHODIMP
IDBDatabase::GetOnerror(nsIDOMEventListener** aErrorListener)
{
  return GetInnerEventListener(mOnErrorListener, aErrorListener);
}

NS_IMETHODIMP
IDBDatabase::SetOnversionchange(nsIDOMEventListener* aVersionChangeListener)
{
  return RemoveAddEventListener(NS_LITERAL_STRING(VERSIONCHANGE_EVT_STR),
                                mOnVersionChangeListener,
                                aVersionChangeListener);
}

NS_IMETHODIMP
IDBDatabase::GetOnversionchange(nsIDOMEventListener** aVersionChangeListener)
{
  return GetInnerEventListener(mOnVersionChangeListener,
                               aVersionChangeListener);
}

nsresult
IDBDatabase::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  NS_ENSURE_TRUE(aVisitor.mDOMEvent, NS_ERROR_UNEXPECTED);

  if (aVisitor.mEventStatus != nsEventStatus_eConsumeNoDefault) {
    nsString type;
    nsresult rv = aVisitor.mDOMEvent->GetType(type);
    NS_ENSURE_SUCCESS(rv, rv);

    if (type.EqualsLiteral(ERROR_EVT_STR)) {
      nsRefPtr<nsDOMEvent> duplicateEvent =
        CreateGenericEvent(type, eDoesNotBubble, eNotCancelable);
      NS_ENSURE_STATE(duplicateEvent);

      nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(mOwner));
      NS_ASSERTION(target, "How can this happen?!");

      bool dummy;
      rv = target->DispatchEvent(duplicateEvent, &dummy);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
CreateObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(NS_LITERAL_CSTRING(
    "INSERT INTO object_store (id, name, key_path, auto_increment) "
    "VALUES (:id, :name, :key_path, :auto_increment)"
  ));
  NS_ENSURE_TRUE(stmt, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                       mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mObjectStore->Name());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"),
                              mObjectStore->KeyPath());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mObjectStore->IsAutoIncrement() ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR);

  return NS_OK;
}

nsresult
DeleteObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
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
