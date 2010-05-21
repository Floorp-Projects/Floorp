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

#include "IDBDatabaseRequest.h"

#include "nsIIDBDatabaseException.h"
#include "nsIIDBTransactionRequest.h"

#include "mozilla/Storage.h"
#include "nsDOMClassInfo.h"
#include "nsThreadUtils.h"

#include "AsyncConnectionHelper.h"
#include "IDBEvents.h"
#include "IDBObjectStoreRequest.h"
#include "IDBTransactionRequest.h"
#include "IndexedDatabaseRequest.h"
#include "LazyIdleThread.h"

USING_INDEXEDDB_NAMESPACE

namespace {

const PRUint32 kDefaultDatabaseTimeoutSeconds = 30;

inline
nsISupports*
isupports_cast(IDBDatabaseRequest* aClassPtr)
{
  return static_cast<nsISupports*>(
    static_cast<IDBRequest::Generator*>(aClassPtr));
}

template<class T>
inline
already_AddRefed<nsISupports>
do_QIAndNull(nsCOMPtr<T>& aCOMPtr)
{
  nsCOMPtr<nsISupports> temp(do_QueryInterface(aCOMPtr));
  aCOMPtr = nsnull;
  return temp.forget();
}

class CloseConnectionRunnable : public nsRunnable
{
public:
  CloseConnectionRunnable(nsTArray<nsCOMPtr<nsISupports> >& aDoomedObjects)
  {
    mDoomedObjects.SwapElements(aDoomedObjects);
  }

  NS_IMETHOD Run()
  {
    mDoomedObjects.Clear();
    return NS_OK;
  }

private:
  nsTArray<nsCOMPtr<nsISupports> > mDoomedObjects;
};

class SetVersionHelper : public AsyncConnectionHelper
{
public:
  SetVersionHelper(IDBDatabaseRequest* aDatabase,
                   IDBRequest* aRequest,
                   const nsAString& aVersion)
  : AsyncConnectionHelper(aDatabase, aRequest), mVersion(aVersion)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

private:
  // In-params
  nsString mVersion;
};

class CreateObjectStoreHelper : public AsyncConnectionHelper
{
public:
  CreateObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                          IDBRequest* aRequest,
                          const nsAString& aName,
                          const nsAString& aKeyPath,
                          bool aAutoIncrement)
  : AsyncConnectionHelper(aDatabase, aRequest), mName(aName),
    mKeyPath(aKeyPath), mAutoIncrement(aAutoIncrement), mId(LL_MININT)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);

protected:
  // In-params.
  nsString mName;
  nsString mKeyPath;
  bool mAutoIncrement;

  // Out-params.
  PRInt64 mId;
};

#if 0
class OpenObjectStoreHelper : public CreateObjectStoreHelper
{
public:
  OpenObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                        IDBRequest* aRequest,
                        const nsAString& aName,
                        PRUint16 aMode)
  : CreateObjectStoreHelper(aDatabase, aRequest, aName, EmptyString(), false),
    mMode(aMode)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 OnSuccess(nsIDOMEventTarget* aTarget);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

protected:
  // In-params.
  PRUint16 mMode;
};
#endif

class RemoveObjectStoreHelper : public AsyncConnectionHelper
{
public:
  RemoveObjectStoreHelper(IDBDatabaseRequest* aDatabase,
                          IDBRequest* aRequest,
                          const ObjectStoreInfo& aStore)
  : AsyncConnectionHelper(aDatabase, aRequest), mStore(aStore)
  { }

  PRUint16 DoDatabaseWork(mozIStorageConnection* aConnection);
  PRUint16 GetSuccessResult(nsIWritableVariant* aResult);

private:
  // In-params.
  ObjectStoreInfo mStore;
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
already_AddRefed<IDBDatabaseRequest>
IDBDatabaseRequest::Create(const nsAString& aName,
                           const nsAString& aDescription,
                           nsTArray<ObjectStoreInfo>& aObjectStores,
                           const nsAString& aVersion,
                           LazyIdleThread* aThread,
                           const nsAString& aDatabaseFilePath,
                           nsCOMPtr<mozIStorageConnection>& aConnection)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBDatabaseRequest> db(new IDBDatabaseRequest());

  db->mName.Assign(aName);
  db->mDescription.Assign(aDescription);
  db->mObjectStores.SwapElements(aObjectStores);
  db->mVersion.Assign(aVersion);
  db->mDatabaseFilePath.Assign(aDatabaseFilePath);

  aThread->SetWeakIdleObserver(db);
  db->mConnectionThread = aThread;

  db->mConnection.swap(aConnection);

  return db.forget();
}

IDBDatabaseRequest::IDBDatabaseRequest()
{

}

IDBDatabaseRequest::~IDBDatabaseRequest()
{
  mConnectionThread->SetWeakIdleObserver(nsnull);
  FireCloseConnectionRunnable();
}

nsCOMPtr<mozIStorageConnection>&
IDBDatabaseRequest::Connection()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");
  return mConnection;
}

nsresult
IDBDatabaseRequest::EnsureConnection()
{
  NS_ASSERTION(!NS_IsMainThread(), "Wrong thread!");

  if (!mConnection) {
    mConnection = IndexedDatabaseRequest::GetConnection(mDatabaseFilePath);
    NS_ENSURE_TRUE(mConnection, NS_ERROR_FAILURE);
  }

  return NS_OK;
}

already_AddRefed<mozIStorageStatement>
IDBDatabaseRequest::AddStatement(bool aCreate,
                                 bool aOverwrite,
                                 bool aAutoIncrement)
{
#ifdef DEBUG
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  if (!aCreate) {
    NS_ASSERTION(aOverwrite, "Bad param combo!");
  }
#endif

  nsresult rv = EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<mozIStorageStatement>& cachedStatement =
    aAutoIncrement ?
      aCreate ?
        aOverwrite ?
          mAddOrModifyAutoIncrementStmt :
          mAddAutoIncrementStmt :
        mModifyAutoIncrementStmt :
      aCreate ?
        aOverwrite ?
          mAddOrModifyStmt :
          mAddStmt :
        mModifyStmt;

  nsCOMPtr<mozIStorageStatement> result(cachedStatement);

  if (!result) {
    nsCString query;
    if (aAutoIncrement) {
      if (aCreate) {
        if (aOverwrite) {
          query.AssignLiteral(
            "INSERT OR REPLACE INTO ai_object_data (object_store_id, id, data) "
            "VALUES (:osid, :key_value, :data)"
          );
        }
        else {
          query.AssignLiteral(
            "INSERT INTO ai_object_data (object_store_id, data) "
            "VALUES (:osid, :data)"
          );
        }
      }
      else {
        query.AssignLiteral(
          "UPDATE ai_object_data "
          "SET data = :data "
          "WHERE object_store_id = :osid "
          "AND id = :key_value"
        );
      }
    }
    else {
      if (aCreate) {
        if (aOverwrite) {
          query.AssignLiteral(
            "INSERT OR REPLACE INTO object_data (object_store_id, key_value, "
                                                "data) "
            "VALUES (:osid, :key_value, :data)"
          );
        }
        else {
          query.AssignLiteral(
            "INSERT INTO object_data (object_store_id, key_value, data) "
            "VALUES (:osid, :key_value, :data)"
          );
        }
      }
      else {
        query.AssignLiteral(
          "UPDATE object_data "
          "SET data = :data "
          "WHERE object_store_id = :osid "
          "AND key_value = :key_value"
        );
      }
    }

    rv = mConnection->CreateStatement(query, getter_AddRefs(cachedStatement));
    NS_ENSURE_SUCCESS(rv, nsnull);

    result = cachedStatement;
  }
  return result.forget();
}

already_AddRefed<mozIStorageStatement>
IDBDatabaseRequest::RemoveStatement(bool aAutoIncrement)
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  nsresult rv = EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<mozIStorageStatement> result;

  if (aAutoIncrement) {
    if (!mRemoveAutoIncrementStmt) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM ai_object_data "
        "WHERE id = :key_value "
        "AND object_store_id = :osid"
      ), getter_AddRefs(mRemoveAutoIncrementStmt));
      NS_ENSURE_SUCCESS(rv, nsnull);
    }
    result = mRemoveAutoIncrementStmt;
  }
  else {
    if (!mRemoveStmt) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "DELETE FROM object_data "
        "WHERE key_value = :key_value "
        "AND object_store_id = :osid"
      ), getter_AddRefs(mRemoveStmt));
      NS_ENSURE_SUCCESS(rv, nsnull);
    }
    result = mRemoveStmt;
  }

  return result.forget();
}

already_AddRefed<mozIStorageStatement>
IDBDatabaseRequest::GetStatement(bool aAutoIncrement)
{
  NS_PRECONDITION(!NS_IsMainThread(), "Wrong thread!");
  nsresult rv = EnsureConnection();
  NS_ENSURE_SUCCESS(rv, nsnull);

  nsCOMPtr<mozIStorageStatement> result;

  if (aAutoIncrement) {
    if (!mGetAutoIncrementStmt) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT data "
        "FROM ai_object_data "
        "WHERE id = :id "
        "AND object_store_id = :osid"
      ), getter_AddRefs(mGetAutoIncrementStmt));
      NS_ENSURE_SUCCESS(rv, nsnull);
    }
    result = mGetAutoIncrementStmt;
  }
  else {
    if (!mGetStmt) {
      rv = mConnection->CreateStatement(NS_LITERAL_CSTRING(
        "SELECT data "
        "FROM object_data "
        "WHERE key_value = :id "
        "AND object_store_id = :osid"
      ), getter_AddRefs(mGetStmt));
      NS_ENSURE_SUCCESS(rv, nsnull);
    }
    result = mGetStmt;
  }

  return result.forget();
}

void
IDBDatabaseRequest::FireCloseConnectionRunnable()
{
  nsTArray<nsCOMPtr<nsISupports> > doomedObjects;

  doomedObjects.AppendElement(do_QIAndNull(mConnection));
  doomedObjects.AppendElement(do_QIAndNull(mAddStmt));
  doomedObjects.AppendElement(do_QIAndNull(mAddAutoIncrementStmt));
  doomedObjects.AppendElement(do_QIAndNull(mModifyStmt));
  doomedObjects.AppendElement(do_QIAndNull(mModifyAutoIncrementStmt));
  doomedObjects.AppendElement(do_QIAndNull(mAddOrModifyStmt));
  doomedObjects.AppendElement(do_QIAndNull(mAddOrModifyAutoIncrementStmt));
  doomedObjects.AppendElement(do_QIAndNull(mRemoveStmt));
  doomedObjects.AppendElement(do_QIAndNull(mRemoveAutoIncrementStmt));
  doomedObjects.AppendElement(do_QIAndNull(mGetStmt));
  doomedObjects.AppendElement(do_QIAndNull(mGetAutoIncrementStmt));

  nsCOMPtr<nsIRunnable> runnable(new CloseConnectionRunnable(doomedObjects));
  mConnectionThread->Dispatch(runnable, NS_DISPATCH_NORMAL);

  NS_ASSERTION(doomedObjects.Length() == 0, "Should have swapped!");
}

void
IDBDatabaseRequest::OnVersionSet(const nsString& aVersion)
{
  mVersion = aVersion;
}

void
IDBDatabaseRequest::OnObjectStoreCreated(const ObjectStoreInfo& aInfo)
{
  NS_ASSERTION(!mObjectStores.Contains(aInfo), "Already know about this one!");
  if (!mObjectStores.AppendElement(aInfo)) {
    NS_ERROR("Failed to add object store name! OOM?");
  }
}

void
IDBDatabaseRequest::OnObjectStoreRemoved(const ObjectStoreInfo& aInfo)
{
  NS_ASSERTION(mObjectStores.Contains(aInfo), "Didn't know about this one!");
  mObjectStores.RemoveElement(aInfo);
}

nsresult
IDBDatabaseRequest::QueueDatabaseWork(nsIRunnable* aRunnable)
{
  return mPendingDatabaseWork.AppendElement(aRunnable) ? NS_OK :
                                                         NS_ERROR_OUT_OF_MEMORY;
}

NS_IMPL_ADDREF(IDBDatabaseRequest)
NS_IMPL_RELEASE(IDBDatabaseRequest)

NS_INTERFACE_MAP_BEGIN(IDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabaseRequest)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabase)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabaseRequest)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBDatabaseRequest, IDBDatabaseRequest)

NS_IMETHODIMP
IDBDatabaseRequest::GetName(nsAString& aName)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetDescription(nsAString& aDescription)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aDescription.Assign(mDescription);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetVersion(nsAString& aVersion)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  aVersion.Assign(mVersion);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::GetObjectStoreNames(nsIDOMDOMStringList** aObjectStores)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<nsDOMStringList> list(new nsDOMStringList());
  PRUint32 count = mObjectStores.Length();
  for (PRUint32 index = 0; index < count; index++) {
    NS_ENSURE_TRUE(list->Add(mObjectStores[index].name),
                   NS_ERROR_OUT_OF_MEMORY);
  }

  list.forget(aObjectStores);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::CreateObjectStore(const nsAString& aName,
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

  nsRefPtr<IDBRequest> request = GenerateRequest();
  NS_ENSURE_TRUE(request, NS_ERROR_FAILURE);

  bool exists = false;
  PRUint32 count = mObjectStores.Length();
  for (PRUint32 index = 0; index < count; index++) {
    if (mObjectStores[index].name.Equals(aName)) {
      exists = true;
      break;
    }
  }

  nsresult rv;
  if (NS_UNLIKELY(exists)) {
    nsCOMPtr<nsIRunnable> runnable =
      IDBErrorEvent::CreateRunnable(request,
                                    nsIIDBDatabaseException::CONSTRAINT_ERR);
    NS_ENSURE_TRUE(runnable, NS_ERROR_UNEXPECTED);
    rv = NS_DispatchToCurrentThread(runnable);
  }
  else {
    nsRefPtr<CreateObjectStoreHelper> helper =
      new CreateObjectStoreHelper(this, request, aName, keyPath,
                                  !!aAutoIncrement);
    rv = helper->Dispatch(mConnectionThread);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::RemoveObjectStore(const nsAString& aName,
                                      nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  if (aName.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  nsRefPtr<IDBRequest> request = GenerateRequest();

  PRInt64 id;
  bool exists = IdForObjectStoreName(aName, &id);

  nsresult rv;
  if (NS_UNLIKELY(!exists)) {
    nsCOMPtr<nsIRunnable> runnable =
      IDBErrorEvent::CreateRunnable(request,
                                    nsIIDBDatabaseException::NOT_FOUND_ERR);
    NS_ENSURE_TRUE(runnable, NS_ERROR_UNEXPECTED);
    rv = NS_DispatchToCurrentThread(runnable);
  }
  else {
    ObjectStoreInfo info(aName, id);
    nsRefPtr<RemoveObjectStoreHelper> helper =
      new RemoveObjectStoreHelper(this, request, info);
    rv = helper->Dispatch(mConnectionThread);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::SetVersion(const nsAString& aVersion,
                               nsIIDBRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBRequest> request = GenerateRequest();

  nsRefPtr<SetVersionHelper> helper =
    new SetVersionHelper(this, request, aVersion);
  nsresult rv = helper->Dispatch(mConnectionThread);
  NS_ENSURE_SUCCESS(rv, rv);

  request.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::Transaction(nsIVariant* aStoreNames,
                                PRUint16 aMode,
                                PRUint32 aTimeout,
                                PRUint8 aOptionalArgCount,
                                nsIIDBTransactionRequest** _retval)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

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

  if (aOptionalArgCount <= 1) {
    aTimeout = kDefaultDatabaseTimeoutSeconds;
  }

  PRUint16 type;
  nsresult rv = aStoreNames->GetDataType(&type);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 storeCount = mObjectStores.Length();
  nsTArray<ObjectStoreInfo> storesToOpen;

  switch (type) {
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
    case nsIDataType::VTYPE_EMPTY_ARRAY: {
      // Empty, request all object stores
      if (!storesToOpen.AppendElements(mObjectStores)) {
        NS_ERROR("Out of memory?");
        return NS_ERROR_OUT_OF_MEMORY;
      }
    } break;

    case nsIDataType::VTYPE_WSTRING_SIZE_IS: {
      // Single name
      nsString name;
      rv = aStoreNames->GetAsAString(name);
      NS_ENSURE_SUCCESS(rv, rv);

      for (PRUint32 index = 0; index < storeCount; index++) {
        ObjectStoreInfo& info = mObjectStores[index];
        if (info.name == name) {
          if (!storesToOpen.AppendElement(info)) {
            NS_ERROR("Out of memory?");
            return NS_ERROR_OUT_OF_MEMORY;
          }
          break;
        }
      }
      if (storesToOpen.IsEmpty()) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    } break;

    case nsIDataType::VTYPE_ARRAY: {
      nsTArray<nsString> names;
      rv = ConvertVariantToStringArray(aStoreNames, names);
      NS_ENSURE_SUCCESS(rv, rv);

      PRUint32 nameCount = names.Length();
      for (PRUint32 nameIndex = 0; nameIndex < nameCount; nameIndex++) {
        nsString& name = names[nameIndex];
        bool found = false;
        for (PRUint32 storeIndex = 0; storeIndex < storeCount; storeIndex++) {
          ObjectStoreInfo& info = mObjectStores[storeIndex];
          if (info.name == name) {
            if (!storesToOpen.AppendElement(info)) {
              NS_ERROR("Out of memory?");
              return NS_ERROR_OUT_OF_MEMORY;
            }
            found = true;
            break;
          }
        }
        if (!found) {
          return NS_ERROR_ILLEGAL_VALUE;
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
        nsString string;
        rv = stringList->Item(stringIndex, string);
        NS_ENSURE_SUCCESS(rv, rv);

        bool found = false;
        for (PRUint32 storeIndex = 0; storeIndex < storeCount; storeIndex++) {
          ObjectStoreInfo& info = mObjectStores[storeIndex];
          if (info.name == string) {
            if (!storesToOpen.AppendElement(info)) {
              NS_ERROR("Out of memory?");
              return NS_ERROR_OUT_OF_MEMORY;
            }
            found = true;
            break;
          }
        }
        if (!found) {
          return NS_ERROR_ILLEGAL_VALUE;
        }
      }
    } break;

    default:
      return NS_ERROR_ILLEGAL_VALUE;
  }

  nsRefPtr<IDBTransactionRequest> transaction =
    IDBTransactionRequest::Create(this, storesToOpen, aMode,
                                  kDefaultDatabaseTimeoutSeconds);
  NS_ENSURE_TRUE(transaction, NS_ERROR_FAILURE);

  transaction.forget(_retval);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::ObjectStore(const nsAString& aName,
                                PRUint16 aMode,
                                PRUint8 aOptionalArgCount,
                                nsIIDBObjectStoreRequest** _retval)
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

  PRInt64 id;
  if (!IdForObjectStoreName(aName, &id)) {
    NS_NOTYETIMPLEMENTED("Need right return code");
    return NS_ERROR_INVALID_ARG;
  }

  nsTArray<ObjectStoreInfo> objectStores;
  if (!objectStores.AppendElement(mObjectStores[id])) {
    NS_ERROR("Out of memory");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  nsRefPtr<IDBTransactionRequest> transaction =
    IDBTransactionRequest::Create(this, objectStores, aMode,
                                  kDefaultDatabaseTimeoutSeconds);
  NS_ENSURE_TRUE(transaction, NS_ERROR_FAILURE);

  nsresult rv = transaction->ObjectStore(aName, _retval);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseRequest::Observe(nsISupports* aSubject,
                            const char* aTopic,
                            const PRUnichar* aData)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  NS_ENSURE_FALSE(strcmp(aTopic, IDLE_THREAD_TOPIC), NS_ERROR_UNEXPECTED);

  FireCloseConnectionRunnable();

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
SetVersionHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  mDatabase->OnVersionSet(mVersion);

  return AsyncConnectionHelper::OnSuccess(aTarget);
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
CreateObjectStoreHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  nsTArray<ObjectStoreInfo> objectStores;
  ObjectStoreInfo* info = objectStores.AppendElement();
  if (!info) {
    NS_ERROR("Out of memory?!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  info->name = mName;
  info->id = mId;
  info->keyPath = mKeyPath;
  info->autoIncrement = mAutoIncrement;

  mDatabase->OnObjectStoreCreated(*info);

  nsRefPtr<IDBTransactionRequest> transaction =
    IDBTransactionRequest::Create(mDatabase, objectStores,
                                  nsIIDBTransaction::READ_WRITE,
                                  kDefaultDatabaseTimeoutSeconds);
  NS_ASSERTION(objectStores.IsEmpty(), "Should have swapped!");

  nsCOMPtr<nsIIDBObjectStoreRequest> result;
  nsresult rv = transaction->ObjectStore(mName, getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");

  nsCOMPtr<nsIWritableVariant> variant =
    do_CreateInstance(NS_VARIANT_CONTRACTID);
  if (!variant) {
    NS_ERROR("Couldn't create variant!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  variant->SetAsISupports(result);

  if (NS_FAILED(variant->SetWritable(PR_FALSE))) {
    NS_ERROR("Failed to make variant readonly!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  nsCOMPtr<nsIDOMEvent> event =
    IDBSuccessEvent::Create(mRequest, variant, transaction);
  if (!event) {
    NS_ERROR("Failed to create event!");
    return nsIIDBDatabaseException::UNKNOWN_ERR;
  }

  AutoTransactionRequestNotifier notifier(transaction);

  PRBool dummy;
  aTarget->DispatchEvent(event, &dummy);
  return OK;
}

#if 0
PRUint16
OpenObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  // TODO pull this up to the connection and cache it so opening these is
  // cheaper.
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "SELECT id, key_path, auto_increment "
    "FROM object_store "
    "WHERE name = :name"
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mName);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  PRBool hasResult;
  rv = stmt->ExecuteStep(&hasResult);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);
  NS_ENSURE_TRUE(hasResult, nsIIDBDatabaseException::NOT_FOUND_ERR);

  mId = stmt->AsInt64(0);
  (void)stmt->GetString(1, mKeyPath);
  mAutoIncrement = !!stmt->AsInt32(2);

  return OK;
}

PRUint16
OpenObjectStoreHelper::OnSuccess(nsIDOMEventTarget* aTarget)
{
  ObjectStoreInfo info(mName, mId);
  mObjectStore =
    IDBObjectStoreRequest::Create(mDatabase, info, mKeyPath, mAutoIncrement,
                                  mMode);
  NS_ENSURE_TRUE(mObjectStore, nsIIDBDatabaseException::UNKNOWN_ERR);
  return AsyncConnectionHelper::OnSuccess(aTarget);
}

PRUint16
OpenObjectStoreHelper::GetSuccessResult(nsIWritableVariant* aResult)
{
  nsCOMPtr<nsISupports> result =
    do_QueryInterface(static_cast<nsIIDBObjectStoreRequest*>(mObjectStore));
  NS_ASSERTION(result, "Failed to QI!");

  aResult->SetAsISupports(result);
  return OK;
}
#endif

PRUint16
RemoveObjectStoreHelper::DoDatabaseWork(mozIStorageConnection* aConnection)
{
  nsCOMPtr<mozIStorageStatement> stmt;
  nsresult rv = aConnection->CreateStatement(NS_LITERAL_CSTRING(
    "DELETE FROM object_store "
    "WHERE name = :name "
  ), getter_AddRefs(stmt));
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  rv = stmt->BindStringByName(NS_LITERAL_CSTRING("name"), mStore.name);
  NS_ENSURE_SUCCESS(rv, nsIIDBDatabaseException::UNKNOWN_ERR);

  if (NS_FAILED(stmt->Execute())) {
    return nsIIDBDatabaseException::NOT_FOUND_ERR;
  }

  return OK;
}

PRUint16
RemoveObjectStoreHelper::GetSuccessResult(nsIWritableVariant* /* aResult */)
{
  mDatabase->OnObjectStoreRemoved(mStore);
  return OK;
}
