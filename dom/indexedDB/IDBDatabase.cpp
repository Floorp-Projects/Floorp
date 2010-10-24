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

#include "nsIIDBDatabaseException.h"

#include "mozilla/Mutex.h"
#include "mozilla/storage.h"
#include "nsDOMClassInfo.h"
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

const PRUint32 kDefaultDatabaseTimeoutSeconds = 30;

PRUint32 gDatabaseInstanceCount = 0;
mozilla::Mutex* gPromptHelpersMutex = nsnull;

// Protected by gPromptHelpersMutex.
nsTArray<nsRefPtr<CheckQuotaHelper> >* gPromptHelpers = nsnull;

class SetVersionHelper : public AsyncConnectionHelper
{
public:
  SetVersionHelper(IDBTransaction* aTransaction,
                   IDBRequest* aRequest,
                   const nsAString& aVersion)
  : AsyncConnectionHelper(aTransaction, aRequest), mVersion(aVersion)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params
  nsString mVersion;
};

class CreateObjectStoreHelper : public AsyncConnectionHelper
{
public:
  CreateObjectStoreHelper(IDBTransaction* aTransaction,
                          IDBObjectStore* aObjectStore)
  : AsyncConnectionHelper(aTransaction, nsnull), mObjectStore(aObjectStore)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  void OnError(nsIDOMEventTarget* aTarget, PRUint16 aErrorCode);

  void ReleaseMainThreadObjects()
  {
    mObjectStore = nsnull;
    AsyncConnectionHelper::ReleaseMainThreadObjects();
  }

private:
  nsRefPtr<IDBObjectStore> mObjectStore;
};

class RemoveObjectStoreHelper : public AsyncConnectionHelper
{
public:
  RemoveObjectStoreHelper(IDBTransaction* aTransaction,
                          PRInt64 aObjectStoreId)
  : AsyncConnectionHelper(aTransaction, nsnull), mObjectStoreId(aObjectStoreId)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  void OnError(nsIDOMEventTarget* aTarget, PRUint16 aErrorCode);

private:
  // In-params.
  PRInt64 mObjectStoreId;
};

NS_STACK_CLASS
class AutoFree
{
public:
  AutoFree(void* aPtr) : mPtr(aPtr) { }
  ~AutoFree() { NS_Free(mPtr); }
private:
  void* mPtr;
};

NS_STACK_CLASS
class AutoRemoveObjectStore
{
public:
  AutoRemoveObjectStore(PRUint32 aId, const nsAString& aName)
  : mId(aId), mName(aName)
  { }

  ~AutoRemoveObjectStore()
  {
    if (mId) {
      ObjectStoreInfo::Remove(mId, mName);
    }
  }

  void forget()
  {
    mId = 0;
  }

private:
  PRUint32 mId;
  nsString mName;
};

inline
nsresult
ConvertVariantToStringArray(nsIVariant* aVariant,
                            nsTArray<nsString>& aStringArray)
{
#ifdef DEBUG
  PRUint16 type;
  NS_ASSERTION(NS_SUCCEEDED(aVariant->GetDataType(&type)) &&
               type == nsIDataType::VTYPE_ARRAY, "Bad arg!");
#endif

  PRUint16 valueType;
  nsIID iid;
  PRUint32 valueCount;
  void* rawArray;

  nsresult rv = aVariant->GetAsArray(&valueType, &iid, &valueCount, &rawArray);
  NS_ENSURE_SUCCESS(rv, rv);

  AutoFree af(rawArray);

  // Just delete anything that we don't expect and return.
  if (valueType != nsIDataType::VTYPE_WCHAR_STR) {
    switch (valueType) {
      case nsIDataType::VTYPE_ID:
      case nsIDataType::VTYPE_CHAR_STR: {
        char** charArray = reinterpret_cast<char**>(rawArray);
        for (PRUint32 index = 0; index < valueCount; index++) {
          if (charArray[index]) {
            NS_Free(charArray[index]);
          }
        }
      } break;

      case nsIDataType::VTYPE_INTERFACE:
      case nsIDataType::VTYPE_INTERFACE_IS: {
        nsISupports** supportsArray = reinterpret_cast<nsISupports**>(rawArray);
        for (PRUint32 index = 0; index < valueCount; index++) {
          NS_IF_RELEASE(supportsArray[index]);
        }
      } break;

      default: {
        // The other types are primitives that do not need to be freed.
      }
    }

    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUnichar** strings = reinterpret_cast<PRUnichar**>(rawArray);

  for (PRUint32 index = 0; index < valueCount; index++) {
    nsString* newString = aStringArray.AppendElement();

    if (!newString) {
      NS_ERROR("Out of memory?");
      for (; index < valueCount; index++) {
        NS_Free(strings[index]);
      }
      return NS_ERROR_OUT_OF_MEMORY;
    }

    newString->Adopt(strings[index], -1);
  }

  return NS_OK;
}

inline
already_AddRefed<IDBRequest>
GenerateRequest(IDBDatabase* aDatabase)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return IDBRequest::Create(static_cast<nsPIDOMEventTarget*>(aDatabase),
                            aDatabase->ScriptContext(), aDatabase->Owner());
}

} // anonymous namespace

// static
already_AddRefed<IDBDatabase>
IDBDatabase::Create(nsIScriptContext* aScriptContext,
                    nsPIDOMWindow* aOwner,
                    DatabaseInfo* aDatabaseInfo,
                    const nsACString& aASCIIOrigin)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabaseInfo, "Null pointer!");
  NS_ASSERTION(!aASCIIOrigin.IsEmpty(), "Empty origin!");

  nsRefPtr<IDBDatabase> db(new IDBDatabase());

  db->mScriptContext = aScriptContext;
  db->mOwner = aOwner;

  db->mDatabaseId = aDatabaseInfo->id;
  db->mName = aDatabaseInfo->name;
  db->mDescription = aDatabaseInfo->description;
  db->mFilePath = aDatabaseInfo->filePath;
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
  mClosed(false)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!gDatabaseInstanceCount++) {
    NS_ASSERTION(!gPromptHelpersMutex, "Should be null!");
    gPromptHelpersMutex = new mozilla::Mutex("IDBDatabase gPromptHelpersMutex");
  }
}

IDBDatabase::~IDBDatabase()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mRegistered) {
    CloseInternal();

    IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
    if (mgr) {
      mgr->UnregisterDatabase(this);
    }
  }

  if (mDatabaseId && !mInvalidated) {
    DatabaseInfo* info;
    if (!DatabaseInfo::Get(mDatabaseId, &info)) {
      NS_ERROR("This should never fail!");
    }

    NS_ASSERTION(info->referenceCount, "Bad reference count!");
    if (--info->referenceCount == 0) {
      DatabaseInfo::Remove(mDatabaseId);
    }
  }

  if (mListenerManager) {
    mListenerManager->Disconnect();
  }

  if (!--gDatabaseInstanceCount) {
    NS_ASSERTION(gPromptHelpersMutex, "Should not be null!");

    delete gPromptHelpers;
    gPromptHelpers = nsnull;

    delete gPromptHelpersMutex;
    gPromptHelpersMutex = nsnull;
  }
}

bool
IDBDatabase::IsQuotaDisabled()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(gPromptHelpersMutex, "This should never be null!");

  MutexAutoLock lock(*gPromptHelpersMutex);

  if (!gPromptHelpers) {
    gPromptHelpers = new nsAutoTArray<nsRefPtr<CheckQuotaHelper>, 10>();
  }

  CheckQuotaHelper* foundHelper = nsnull;

  PRUint32 count = gPromptHelpers->Length();
  for (PRUint32 index = 0; index < count; index++) {
    nsRefPtr<CheckQuotaHelper>& helper = gPromptHelpers->ElementAt(index);
    if (helper->WindowSerial() == Owner()->GetSerial()) {
      foundHelper = helper;
      break;
    }
  }

  if (!foundHelper) {
    nsRefPtr<CheckQuotaHelper>* newHelper = gPromptHelpers->AppendElement();
    if (!newHelper) {
      NS_WARNING("Out of memory!");
      return false;
    }
    *newHelper = new CheckQuotaHelper(this, *gPromptHelpersMutex);
    foundHelper = *newHelper;

    {
      // Unlock before calling out to XPCOM.
      MutexAutoUnlock unlock(*gPromptHelpersMutex);

      nsresult rv = NS_DispatchToMainThread(foundHelper, NS_DISPATCH_NORMAL);
      NS_ENSURE_SUCCESS(rv, false);
    }
  }

  return foundHelper->PromptAndReturnQuotaIsDisabled();
}

void
IDBDatabase::Invalidate()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(gPromptHelpersMutex, "This should never be null!");

  // Make sure we're closed too.
  Close();

  // Cancel any quota prompts that are currently being displayed.
  {
    MutexAutoLock lock(*gPromptHelpersMutex);

    if (gPromptHelpers) {
      PRUint32 count = gPromptHelpers->Length();
      for (PRUint32 index = 0; index < count; index++) {
        nsRefPtr<CheckQuotaHelper>& helper = gPromptHelpers->ElementAt(index);
        if (helper->WindowSerial() == Owner()->GetSerial()) {
          helper->Cancel();
          break;
        }
      }
    }
  }

  if (!PR_AtomicSet(&mInvalidated, 1)) {
    DatabaseInfo* info;
    if (!DatabaseInfo::Get(mDatabaseId, &info)) {
      NS_ERROR("This should never fail!");
    }

    NS_ASSERTION(info->referenceCount, "Bad reference count!");
    if (--info->referenceCount == 0) {
      DatabaseInfo::Remove(mDatabaseId);
    }
  }
}

bool
IDBDatabase::IsInvalidated()
{
  return !!mInvalidated;
}

void
IDBDatabase::CloseInternal()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (!mClosed) {
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
IDBDatabase::OnUnlink()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(!mOwner, "Should have been cleared already!");

  // We've been unlinked, at the very least we should be able to prevent further
  // transactions from starting and unblock any other SetVersion callers.
  Close();

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
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOnErrorListener)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(IDBDatabase,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOnErrorListener)

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
IDBDatabase::GetDescription(nsAString& aDescription)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aDescription.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::GetVersion(nsAString& aVersion)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabaseId, &info)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }
  aVersion.Assign(info->version);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::GetObjectStoreNames(nsIDOMDOMStringList** aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabaseId, &info)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoTArray<nsString, 10> objectStoreNames;
  if (!info->GetObjectStoreNames(objectStoreNames)) {
    NS_WARNING("Couldn't get names!");
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  PRUint32 count = objectStoreNames.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(objectStoreNames[index]), NS_ERROR_OUT_OF_MEMORY);
  }

  list.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::CreateObjectStore(const nsAString& aName,
                               const nsAString& aKeyPath,
                               PRBool aAutoIncrement,
                               nsIIDBObjectStore** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // XPConnect makes "null" into a void string, we need an empty string.
  nsString keyPath(aKeyPath);
  if (keyPath.IsVoid()) {
    keyPath.Truncate();
  }

  DatabaseInfo* databaseInfo;
  if (!DatabaseInfo::Get(mDatabaseId, &databaseInfo)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }

  if (databaseInfo->ContainsStoreName(aName)) {
    // XXX Should be nsIIDBTransaction::CONSTRAINT_ERR.
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  IDBTransaction* transaction = AsyncConnectionHelper::GetCurrentTransaction();

  if (!transaction ||
      transaction->Mode() != nsIIDBTransaction::VERSION_CHANGE) {
    // XXX Should be nsIIDBTransaction::NOT_ALLOWED_ERR.
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsAutoPtr<ObjectStoreInfo> newInfo(new ObjectStoreInfo());

  newInfo->name = aName;
  newInfo->id = databaseInfo->nextObjectStoreId++;
  newInfo->keyPath = keyPath;
  newInfo->autoIncrement = aAutoIncrement;
  newInfo->databaseId = mDatabaseId;

  if (!ObjectStoreInfo::Put(newInfo)) {
    NS_ERROR("Put failed!");
    return NS_ERROR_FAILURE;
  }
  ObjectStoreInfo* objectStoreInfo = newInfo.forget();

  // Don't leave this in the hash if we fail below!
  AutoRemoveObjectStore autoRemove(mDatabaseId, aName);

  nsRefPtr<IDBObjectStore> objectStore =
    IDBObjectStore::Create(transaction, objectStoreInfo);
  NS_ENSURE_TRUE(objectStore, NS_ERROR_FAILURE);

  nsRefPtr<CreateObjectStoreHelper> helper =
    new CreateObjectStoreHelper(transaction, objectStore);

  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  autoRemove.forget();

  objectStore.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::RemoveObjectStore(const nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  ObjectStoreInfo* objectStoreInfo;
  if (!ObjectStoreInfo::Get(mDatabaseId, aName, &objectStoreInfo)) {
    NS_ERROR("This should never fail!");
    // XXX Should be nsIIDBTransaction::NOT_FOUND_ERR.
    return NS_ERROR_NOT_AVAILABLE;
  }

  IDBTransaction* transaction = AsyncConnectionHelper::GetCurrentTransaction();

  if (!transaction ||
      transaction->Mode() != nsIIDBTransaction::VERSION_CHANGE) {
    // XXX Should be nsIIDBTransaction::NOT_ALLOWED_ERR.
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsRefPtr<RemoveObjectStoreHelper> helper =
    new RemoveObjectStoreHelper(transaction, objectStoreInfo->id);
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  ObjectStoreInfo::Remove(mDatabaseId, aName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::SetVersion(const nsAString& aVersion,
                        JSContext* aCx,
                        nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (mClosed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabaseId, &info)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }

  // Lock the whole database.
  nsTArray<nsString> storesToOpen;
  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, storesToOpen, IDBTransaction::VERSION_CHANGE,
                           kDefaultDatabaseTimeoutSeconds);
  NS_ENSURE_TRUE(transaction, NS_ERROR_FAILURE);

  nsRefPtr<IDBVersionChangeRequest> request =
    IDBVersionChangeRequest::Create(static_cast<nsPIDOMEventTarget*>(this),
                                    ScriptContext(), Owner());
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsRefPtr<SetVersionHelper> helper =
    new SetVersionHelper(transaction, request, aVersion);

  IndexedDatabaseManager* mgr = IndexedDatabaseManager::Get();
  NS_ASSERTION(mgr, "This should never be null!");

  nsresult rv = mgr->SetDatabaseVersion(this, request, aVersion, helper);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::Transaction(nsIVariant* aStoreNames,
                         PRUint16 aMode,
                         PRUint32 aTimeout,
                         JSContext* aCx,
                         PRUint8 aOptionalArgCount,
                         nsIIDBTransaction** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (IndexedDatabaseManager::IsShuttingDown()) {
    return NS_ERROR_ILLEGAL_DURING_SHUTDOWN;
  }

  if (mClosed) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  if (aOptionalArgCount) {
    if (aMode != nsIIDBTransaction::READ_WRITE &&
        aMode != nsIIDBTransaction::READ_ONLY &&
        aMode != nsIIDBTransaction::SNAPSHOT_READ) {
      return NS_ERROR_INVALID_ARG;
    }
    if (aMode == nsIIDBTransaction::SNAPSHOT_READ) {
      NS_NOTYETIMPLEMENTED("Implement me!");
      return NS_ERROR_NOT_IMPLEMENTED;
    }
  }
  else {
    aMode = nsIIDBTransaction::READ_ONLY;
  }

  if (aOptionalArgCount <= 1) {
    aTimeout = kDefaultDatabaseTimeoutSeconds;
  }

  PRUint16 type;
  nsresult rv = aStoreNames->GetDataType(&type);
  NS_ENSURE_SUCCESS(rv, rv);

  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabaseId, &info)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }

  nsTArray<nsString> storesToOpen;

  switch (type) {
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
    case nsIDataType::VTYPE_EMPTY_ARRAY: {
      // Empty, request all object stores
      if (!info->GetObjectStoreNames(storesToOpen)) {
        NS_ERROR("Out of memory?");
        return NS_ERROR_OUT_OF_MEMORY;
      }
    } break;

    case nsIDataType::VTYPE_WSTRING_SIZE_IS: {
      // Single name
      nsString name;
      rv = aStoreNames->GetAsAString(name);
      NS_ENSURE_SUCCESS(rv, rv);

      if (!info->ContainsStoreName(name)) {
        return NS_ERROR_NOT_AVAILABLE;
      }

      if (!storesToOpen.AppendElement(name)) {
        NS_ERROR("Out of memory?");
        return NS_ERROR_OUT_OF_MEMORY;
      }
    } break;

    case nsIDataType::VTYPE_ARRAY: {
      nsTArray<nsString> names;
      rv = ConvertVariantToStringArray(aStoreNames, names);
      NS_ENSURE_SUCCESS(rv, rv);

      PRUint32 nameCount = names.Length();
      for (PRUint32 nameIndex = 0; nameIndex < nameCount; nameIndex++) {
        nsString& name = names[nameIndex];

        if (!info->ContainsStoreName(name)) {
          return NS_ERROR_NOT_AVAILABLE;
        }

        if (!storesToOpen.AppendElement(name)) {
          NS_ERROR("Out of memory?");
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
      NS_ASSERTION(nameCount == storesToOpen.Length(), "Should have bailed!");
    } break;

    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS: {
      nsCOMPtr<nsISupports> supports;
      nsID *iid;
      rv = aStoreNames->GetAsInterface(&iid, getter_AddRefs(supports));
      NS_ENSURE_SUCCESS(rv, rv);

      NS_Free(iid);

      nsCOMPtr<nsIDOMDOMStringList> stringList(do_QueryInterface(supports));
      if (!stringList) {
        // We don't support anything other than nsIDOMDOMStringList.
        return NS_ERROR_ILLEGAL_VALUE;
      }

      PRUint32 stringCount;
      rv = stringList->GetLength(&stringCount);
      NS_ENSURE_SUCCESS(rv, rv);

      for (PRUint32 stringIndex = 0; stringIndex < stringCount; stringIndex++) {
        nsString name;
        rv = stringList->Item(stringIndex, name);
        NS_ENSURE_SUCCESS(rv, rv);

        if (!info->ContainsStoreName(name)) {
          return NS_ERROR_NOT_AVAILABLE;
        }

        if (!storesToOpen.AppendElement(name)) {
          NS_ERROR("Out of memory?");
          return NS_ERROR_OUT_OF_MEMORY;
        }
      }
    } break;

    default:
      return NS_ERROR_ILLEGAL_VALUE;
  }

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, storesToOpen, aMode,
                           kDefaultDatabaseTimeoutSeconds);
  NS_ENSURE_TRUE(transaction, NS_ERROR_FAILURE);

  transaction.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::Close()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  CloseInternal();

  NS_ASSERTION(mClosed, "Should have set the closed flag!");
  return NS_OK;
}

PRUint16
SetVersionHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  NS_PRECONDITION(aConnection, "Passing a null connection!");

  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "UPDATE database "
    "SET version = :version"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("version"), mVersion);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::CONSTRAINT_ERR;
  }

  return OK;
}

PRUint16
SetVersionHelper::GetSuccessResult(nsIWritableVariant* /* aResult */)
{
  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabase->Id(), &info)) {
    NS_ERROR("This should never fail!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }
  info->version = mVersion;

  return OK;
}

  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  void OnError(nsIDOMEventTarget* aTarget, PRUint16 aErrorCode);

PRUint16
CreateObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(NS_LITERAL_CSTRING(
    "INSERT INTO object_store (id, name, key_path, auto_increment) "
    "VALUES (:id, :name, :key_path, :auto_increment)"
  ));
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"),
                                       mObjectStore->Id());
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mObjectStore->Name());
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"),
                              mObjectStore->KeyPath());
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mObjectStore->IsAutoIncrement() ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
CreateObjectStoreHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  NS_ASSERTION(!aTarget, "Huh?!");
  return OK;
}

void
CreateObjectStoreHelper::OnError(nsIDOMEventTarget* aTarget,
                                 PRUint16 aErrorCode)
{
  NS_ASSERTION(!aTarget, "Huh?!");
}

PRUint16
RemoveObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCOMPtr<mozIStorageStatement> stmt =
    mTransaction->GetCachedStatement(NS_LITERAL_CSTRING(
    "DELETE FROM object_store "
    "WHERE id = :id "
  ));
  NS_ENSURE_TRUE(stmt, nsIIDBDatabaseException::UNKNOWN_ERR);

  mozStorageStatementScoper scoper(stmt);

  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("id"), mObjectStoreId);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->Execute();
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  return OK;
}

PRUint16
RemoveObjectStoreHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  NS_ASSERTION(!aTarget, "Huh?!");

  return OK;
}

void
RemoveObjectStoreHelper::OnError(nsIDOMEventTarget* aTarget,
                                 PRUint16 aErrorCode)
{
  NS_NOTREACHED("Removing an object store should never fail here!");
}
