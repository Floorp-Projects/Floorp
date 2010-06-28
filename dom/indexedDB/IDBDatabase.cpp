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

#include "mozilla/storage.h"
#include "nsDOMClassInfo.h"
#include "nsProxyRelease.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "DatabaseInfo.h"
#include "IDBEvents.h"
#include "IDBObjectStore.h"
#include "IDBTransaction.h"
#include "IDBFactory.h"
#include "LazyIdleThread.h"

USING_INDEXEDDB_NAMESPACE

namespace {

const PRUint32 kDefaultDatabaseTimeoutSeconds = 30;

inline
nsISupports*
isupports_cast(IDBDatabase* aClassPtr)
{
  return static_cast<nsISupports*>(
    static_cast<IDBRequest::Generator*>(aClassPtr));
}

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
                          IDBRequest* aRequest,
                          const nsAString& aName,
                          const nsAString& aKeyPath,
                          bool aAutoIncrement)
  : AsyncConnectionHelper(aTransaction, aRequest), mName(aName),
    mKeyPath(aKeyPath), mAutoIncrement(aAutoIncrement),
    mId(LL_MININT)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

protected:
  // In-params.
  nsString mName;
  nsString mKeyPath;
  bool mAutoIncrement;

  // Out-params.
  PRInt64 mId;
};

class RemoveObjectStoreHelper : public AsyncConnectionHelper
{
public:
  RemoveObjectStoreHelper(IDBTransaction* aTransaction,
                          IDBRequest* aRequest,
                          const nsAString& aName)
  : AsyncConnectionHelper(aTransaction, aRequest), mName(aName)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  nsString mName;
};

class AutoFree
{
public:
  AutoFree(void* aPtr) : mPtr(aPtr) { }
  ~AutoFree() { NS_Free(mPtr); }
private:
  void* mPtr;
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

} // anonymous namespace

// static
already_AddRefed<IDBDatabase>
IDBDatabase::Create(DatabaseInfo* aDatabaseInfo,
                    LazyIdleThread* aThread,
                    nsCOMPtr<mozIStorageConnection>& aConnection)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ASSERTION(aDatabaseInfo, "Null pointer!");
  NS_ASSERTION(aThread, "Null pointer!");
  NS_ASSERTION(aConnection, "Null pointer!");

  nsRefPtr<IDBDatabase> db(new IDBDatabase());

  db->mDatabaseId = aDatabaseInfo->id;
  db->mName = aDatabaseInfo->name;
  db->mDescription = aDatabaseInfo->description;
  db->mFilePath = aDatabaseInfo->filePath;

  aThread->SetWeakIdleObserver(db);
  db->mConnectionThread = aThread;

  db->mConnection.swap(aConnection);

  return db.forget();
}

IDBDatabase::IDBDatabase()
: mDatabaseId(0)
{

}

IDBDatabase::~IDBDatabase()
{
  if (mConnectionThread) {
    mConnectionThread->SetWeakIdleObserver(nsnull);
  }

  CloseConnection();

  if (mDatabaseId) {
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

nsresult
IDBDatabase::GetOrCreateConnection(mozIStorageConnection** aResult)
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (!mConnection) {
    mConnection = IDBFactory::GetConnection(mFilePath);
    NS_ENSURE_TRUE(mConnection, NS_ERROR_FAILURE);
  }

  nsCOMPtr<mozIStorageConnection> result(mConnection);
  result.forget(aResult);
  return NS_OK;
}

void
IDBDatabase::CloseConnection()
{
  if (mConnection) {
    if (mConnectionThread) {
      NS_ProxyRelease(mConnectionThread, mConnection, PR_TRUE);
    }
    else {
      NS_ERROR("Leaking connection!");
      mozIStorageConnection* leak;
      mConnection.forget(&leak);
    }
  }
}

NS_IMPL_ADDREF(IDBDatabase)
NS_IMPL_RELEASE(IDBDatabase)

NS_INTERFACE_MAP_BEGIN(IDBDatabase)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, IDBRequest::Generator)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabase)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabase)
NS_INTERFACE_MAP_END

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
                               nsIIDBRequest** _retval)
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

  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabaseId, &info)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }

  if (info->ContainsStoreName(aName)) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  nsRefPtr<IDBRequest> request = GenerateWriteRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  nsTArray<nsString> objectStores;
  nsString* name = objectStores.AppendElement(aName);
  if (!name) {
    NS_ERROR("Out of memory?");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, objectStores, nsIIDBTransaction::READ_WRITE,
                           kDefaultDatabaseTimeoutSeconds);

  nsRefPtr<CreateObjectStoreHelper> helper =
    new CreateObjectStoreHelper(transaction, request, aName, keyPath,
                                !!aAutoIncrement);
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::RemoveObjectStore(const nsAString& aName,
                               nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabaseId, &info)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }

  if (!info->ContainsStoreName(aName)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<nsString> storesToOpen;
  if (!storesToOpen.AppendElement(aName)) {
    NS_ERROR("Out of memory!");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, storesToOpen, nsIIDBTransaction::READ_WRITE,
                           kDefaultDatabaseTimeoutSeconds);
  NS_ENSURE_TRUE(transaction, NS_ERROR_FAILURE);


  nsRefPtr<IDBRequest> request = GenerateWriteRequest();

  nsRefPtr<RemoveObjectStoreHelper> helper =
    new RemoveObjectStoreHelper(transaction, request, aName);
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::SetVersion(const nsAString& aVersion,
                        nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabaseId, &info)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }

  nsRefPtr<IDBRequest> request = GenerateWriteRequest();

  // Lock the whole database
  nsTArray<nsString> storesToOpen;
  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, storesToOpen, IDBTransaction::FULL_LOCK,
                           kDefaultDatabaseTimeoutSeconds);
  NS_ENSURE_TRUE(transaction, NS_ERROR_FAILURE);

  nsRefPtr<SetVersionHelper> helper =
    new SetVersionHelper(transaction, request, aVersion);
  nsresult rv = helper->DispatchToTransactionPool();
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::Transaction(nsIVariant* aStoreNames,
                         PRUint16 aMode,
                         PRUint32 aTimeout,
                         PRUint8 aOptionalArgCount,
                         nsIIDBTransaction** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

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
IDBDatabase::ObjectStore(const nsAString& aName,
                         PRUint16 aMode,
                         PRUint8 aOptionalArgCount,
                         nsIIDBObjectStore** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  if (aOptionalArgCount) {
    if (aMode != nsIIDBTransaction::READ_WRITE &&
        aMode != nsIIDBTransaction::READ_ONLY &&
        aMode != nsIIDBTransaction::SNAPSHOT_READ) {
      return NS_ERROR_INVALID_ARG;
    }
  }
  else {
    aMode = nsIIDBTransaction::READ_ONLY;
  }

  DatabaseInfo* info;
  if (!DatabaseInfo::Get(mDatabaseId, &info)) {
    NS_ERROR("This should never fail!");
    return NS_ERROR_UNEXPECTED;
  }

  if (!info->ContainsStoreName(aName)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsTArray<nsString> storesToOpen;
  if (!storesToOpen.AppendElement(aName)) {
    NS_ERROR("Out of memory?");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<IDBTransaction> transaction =
    IDBTransaction::Create(this, storesToOpen, aMode,
                           kDefaultDatabaseTimeoutSeconds);
  NS_ENSURE_TRUE(transaction, NS_ERROR_FAILURE);

  nsresult rv = transaction->ObjectStore(aName, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabase::Observe(nsISupports* aSubject,
                     const char* aTopic,
                     const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_FALSE(strcmp(aTopic, IDLE_THREAD_TOPIC), NS_ERROR_UNEXPECTED);

  CloseConnection();

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

PRUint16
CreateObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  // Insert the data into the database.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "INSERT INTO object_store (name, key_path, auto_increment) "
    "VALUES (:name, :key_path, :auto_increment)"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("key_path"), mKeyPath);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindInt32ByName(NS_LITERAL_CSTRING("auto_increment"),
                             mAutoIncrement ? 1 : 0);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::CONSTRAINT_ERR;
  }

  // Get the id of this object store, and store it for future use.
  (void)aConnection->GetLastInsertRowID(&mId);

  return OK;
}

PRUint16
CreateObjectStoreHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  nsAutoPtr<ObjectStoreInfo> info(new ObjectStoreInfo());

  info->name = mName;
  info->id = mId;
  info->keyPath = mKeyPath;
  info->autoIncrement = mAutoIncrement;
  info->databaseId = mDatabase->Id();

  if (!ObjectStoreInfo::Put(info)) {
    NS_ERROR("Put failed!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }
  info.forget();

  nsCOMPtr<nsIIDBObjectStore> result;
  nsresult rv = mTransaction->ObjectStore(mName, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  aResult->SetAsISupports(result);

  return OK;
}

PRUint16
RemoveObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM object_store "
    "WHERE name = :name "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::NOT_FOUND_ERR;
  }

  return OK;
}

PRUint16
RemoveObjectStoreHelper::GetSuccessResult(nsIWritableVariant* /* aResult */)
{
  ObjectStoreInfo::Remove(mDatabase->Id(), mName);
  return OK;
}
