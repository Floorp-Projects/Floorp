/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DataStoreDB_h
#define mozilla_dom_DataStoreDB_h

#include "mozilla/dom/IDBTransactionBinding.h"
#include "nsAutoPtr.h"
#include "nsIDOMEventListener.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

#define DATASTOREDB_REVISION       "revision"

namespace mozilla {
namespace dom {

namespace indexedDB {
class IDBDatabase;
class IDBFactory;
class IDBObjectStore;
class IDBOpenDBRequest;
class IDBTransaction;
}

class DataStoreDBCallback;

class DataStoreDB MOZ_FINAL : public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS

  DataStoreDB(const nsAString& aManifestURL, const nsAString& aName);
  ~DataStoreDB();

  nsresult Open(IDBTransactionMode aMode, const Sequence<nsString>& aDb,
                DataStoreDBCallback* aCallback);

  nsresult Delete();

  indexedDB::IDBTransaction* Transaction() const;

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

private:
  nsresult CreateFactoryIfNeeded();

  nsresult UpgradeSchema();

  nsresult DatabaseOpened();

  nsresult AddEventListeners();

  nsresult RemoveEventListeners();

  nsString mDatabaseName;

  nsRefPtr<indexedDB::IDBFactory> mFactory;
  nsRefPtr<indexedDB::IDBOpenDBRequest> mRequest;
  nsRefPtr<indexedDB::IDBDatabase> mDatabase;
  nsRefPtr<indexedDB::IDBTransaction> mTransaction;

  nsRefPtr<DataStoreDBCallback> mCallback;

  // Internal state to avoid strange use of this class.
  enum StateType {
    Inactive,
    Active
  } mState;

  IDBTransactionMode mTransactionMode;
  Sequence<nsString> mObjectStores;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_DataStoreDB_h
