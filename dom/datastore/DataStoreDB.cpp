/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataStoreDB.h"

#include "DataStoreCallbacks.h"
#include "mozilla/dom/IDBDatabaseBinding.h"
#include "mozilla/dom/IDBFactoryBinding.h"
#include "mozilla/dom/indexedDB/IDBDatabase.h"
#include "mozilla/dom/indexedDB/IDBEvents.h"
#include "mozilla/dom/indexedDB/IDBFactory.h"
#include "mozilla/dom/indexedDB/IDBIndex.h"
#include "mozilla/dom/indexedDB/IDBObjectStore.h"
#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "nsIDOMEvent.h"

#define DATASTOREDB_VERSION        1
#define DATASTOREDB_NAME           "DataStoreDB"
#define DATASTOREDB_REVISION_INDEX "revisionIndex"

using namespace mozilla::dom::indexedDB;

namespace mozilla {
namespace dom {

class VersionChangeListener MOZ_FINAL : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS

  VersionChangeListener(IDBDatabase* aDatabase)
    : mDatabase(aDatabase)
  {}

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent)
  {
    nsString type;
    nsresult rv = aEvent->GetType(type);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (!type.EqualsASCII("versionchange")) {
      MOZ_ASSERT_UNREACHABLE("Expected a versionchange event");
      return NS_ERROR_FAILURE;
    }

    rv = mDatabase->RemoveEventListener(NS_LITERAL_STRING("versionchange"),
                                        this, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

#ifdef DEBUG
    nsCOMPtr<IDBVersionChangeEvent> event = do_QueryInterface(aEvent);
    MOZ_ASSERT(event);

    Nullable<uint64_t> version = event->GetNewVersion();
    MOZ_ASSERT(version.IsNull());
#endif

    return mDatabase->Close();
  }

private:
  IDBDatabase* mDatabase;

  ~VersionChangeListener() {}
};

NS_IMPL_ISUPPORTS(VersionChangeListener, nsIDOMEventListener)

NS_IMPL_ISUPPORTS(DataStoreDB, nsIDOMEventListener)

DataStoreDB::DataStoreDB(const nsAString& aManifestURL, const nsAString& aName)
  : mState(Inactive)
{
  mDatabaseName.Assign(aName);
  mDatabaseName.Append('|');
  mDatabaseName.Append(aManifestURL);
}

DataStoreDB::~DataStoreDB()
{
}

nsresult
DataStoreDB::CreateFactoryIfNeeded()
{
  if (!mFactory) {
    nsresult rv = IDBFactory::Create(nullptr, getter_AddRefs(mFactory));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
}

nsresult
DataStoreDB::Open(IDBTransactionMode aMode, const Sequence<nsString>& aDbs,
                  DataStoreDBCallback* aCallback)
{
  MOZ_ASSERT(mState == Inactive);

  nsresult rv = CreateFactoryIfNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  ErrorResult error;
  mRequest = mFactory->Open(mDatabaseName, DATASTOREDB_VERSION, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.ErrorCode();
  }

  rv = AddEventListeners();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mState = Active;
  mTransactionMode = aMode;
  mObjectStores = aDbs;
  mCallback = aCallback;
  return NS_OK;
}

NS_IMETHODIMP
DataStoreDB::HandleEvent(nsIDOMEvent* aEvent)
{
  nsString type;
  nsresult rv = aEvent->GetType(type);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (type.EqualsASCII("success")) {
    RemoveEventListeners();
    mState = Inactive;

    rv = DatabaseOpened();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      mCallback->Run(this, false);
    } else {
      mCallback->Run(this, true);
    }

    mRequest = nullptr;
    return NS_OK;
  }

  if (type.EqualsASCII("upgradeneeded")) {
    return UpgradeSchema();
  }

  if (type.EqualsASCII("error") || type.EqualsASCII("blocked")) {
    RemoveEventListeners();
    mState = Inactive;
    mCallback->Run(this, false);
    mRequest = nullptr;
    return NS_OK;
  }

  MOZ_CRASH("This should not happen");
}

nsresult
DataStoreDB::UpgradeSchema()
{
  MOZ_ASSERT(NS_IsMainThread());

  AutoSafeJSContext cx;

  ErrorResult error;
  JS::Rooted<JS::Value> result(cx);
  mRequest->GetResult(&result, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.ErrorCode();
  }

  MOZ_ASSERT(result.isObject());

  IDBDatabase* database = nullptr;
  nsresult rv = UNWRAP_OBJECT(IDBDatabase, &result.toObject(), database);
  if (NS_FAILED(rv)) {
    NS_WARNING("Didn't get the object we expected!");
    return rv;
  }

  {
    RootedDictionary<IDBObjectStoreParameters> params(cx);
    params.Init(NS_LITERAL_STRING("{ \"autoIncrement\": true }"));
    nsRefPtr<IDBObjectStore> store =
      database->CreateObjectStore(cx, NS_LITERAL_STRING(DATASTOREDB_NAME),
                                  params, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.ErrorCode();
    }
  }

  nsRefPtr<IDBObjectStore> store;

  {
    RootedDictionary<IDBObjectStoreParameters> params(cx);
    params.Init(NS_LITERAL_STRING("{ \"autoIncrement\": true, \"keyPath\": \"internalRevisionId\" }"));

    store =
      database->CreateObjectStore(cx, NS_LITERAL_STRING(DATASTOREDB_REVISION),
                                  params, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.ErrorCode();
    }
  }

  {
    RootedDictionary<IDBIndexParameters> params(cx);
    params.Init(NS_LITERAL_STRING("{ \"unique\": true }"));
    nsRefPtr<IDBIndex> index =
      store->CreateIndex(cx, NS_LITERAL_STRING(DATASTOREDB_REVISION_INDEX),
                         NS_LITERAL_STRING("revisionId"), params, error);
    if (NS_WARN_IF(error.Failed())) {
      return error.ErrorCode();
    }
  }

  return NS_OK;
}

nsresult
DataStoreDB::DatabaseOpened()
{
  MOZ_ASSERT(NS_IsMainThread());

  AutoSafeJSContext cx;

  ErrorResult error;
  JS::Rooted<JS::Value> result(cx);
  mRequest->GetResult(&result, error);
  if (NS_WARN_IF(error.Failed())) {
    return error.ErrorCode();
  }

  MOZ_ASSERT(result.isObject());

  nsresult rv = UNWRAP_OBJECT(IDBDatabase, &result.toObject(), mDatabase);
  if (NS_FAILED(rv)) {
    NS_WARNING("Didn't get the object we expected!");
    return rv;
  }

  nsRefPtr<VersionChangeListener> listener =
    new VersionChangeListener(mDatabase);
  rv = mDatabase->EventTarget::AddEventListener(
    NS_LITERAL_STRING("versionchange"), listener, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsRefPtr<IDBTransaction> txn = mDatabase->Transaction(mObjectStores,
                                                        mTransactionMode,
                                                        error);
  if (NS_WARN_IF(error.Failed())) {
    return error.ErrorCode();
  }

  mTransaction = txn.forget();
  return NS_OK;
}

nsresult
DataStoreDB::Delete()
{
  MOZ_ASSERT(mState == Inactive);

  nsresult rv = CreateFactoryIfNeeded();
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  mTransaction = nullptr;

  if (mDatabase) {
    rv = mDatabase->Close();
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    mDatabase = nullptr;
  }

  ErrorResult error;
  nsRefPtr<IDBOpenDBRequest> request =
    mFactory->DeleteDatabase(mDatabaseName, IDBOpenDBOptions(), error);
  if (NS_WARN_IF(error.Failed())) {
    return error.ErrorCode();
  }

  return NS_OK;
}

indexedDB::IDBTransaction*
DataStoreDB::Transaction() const
{
  MOZ_ASSERT(mTransaction);
  MOZ_ASSERT(mTransaction->IsOpen());
  return mTransaction;
}

nsresult
DataStoreDB::AddEventListeners()
{
  nsresult rv;
  rv = mRequest->EventTarget::AddEventListener(NS_LITERAL_STRING("success"),
                                               this, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mRequest->EventTarget::AddEventListener(NS_LITERAL_STRING("upgradeneeded"),
                                               this, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mRequest->EventTarget::AddEventListener(NS_LITERAL_STRING("error"),
                                               this, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mRequest->EventTarget::AddEventListener(NS_LITERAL_STRING("blocked"),
                                               this, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

nsresult
DataStoreDB::RemoveEventListeners()
{
  nsresult rv;
  rv = mRequest->RemoveEventListener(NS_LITERAL_STRING("success"),
                                     this, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mRequest->RemoveEventListener(NS_LITERAL_STRING("upgradeneeded"),
                                     this, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mRequest->RemoveEventListener(NS_LITERAL_STRING("error"),
                                     this, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  rv = mRequest->RemoveEventListener(NS_LITERAL_STRING("blocked"),
                                     this, false);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  return NS_OK;
}

} // namespace dom
} // namespace mozilla
