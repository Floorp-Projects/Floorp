/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbfactory_h__
#define mozilla_dom_indexeddb_idbfactory_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozIStorageConnection.h"
#include "nsIIDBFactory.h"

#include "nsCycleCollectionParticipant.h"
#include "nsXULAppAPI.h"

class nsIAtom;
class nsPIDOMWindow;

BEGIN_INDEXEDDB_NAMESPACE

struct DatabaseInfo;
class IDBDatabase;
struct ObjectStoreInfo;

class IDBFactory MOZ_FINAL : public nsIIDBFactory
{
  typedef nsTArray<nsRefPtr<ObjectStoreInfo> > ObjectStoreInfoArray;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBFactory)
  NS_DECL_NSIIDBFACTORY

  static already_AddRefed<nsIIDBFactory> Create(nsPIDOMWindow* aWindow);

  static already_AddRefed<nsIIDBFactory> Create(JSContext* aCx,
                                                JSObject* aOwningObject);

  static already_AddRefed<mozIStorageConnection>
  GetConnection(const nsAString& aDatabaseFilePath);

  // Called when a process uses an IndexedDB factory. We only allow
  // a single process type to use IndexedDB - the chrome/single process
  // in Firefox, and the child process in Fennec - so access by more
  // than one process type is a very serious error.
  static void
  NoteUsedByProcessType(GeckoProcessType aProcessType);

  static nsresult
  LoadDatabaseInformation(mozIStorageConnection* aConnection,
                          nsIAtom* aDatabaseId,
                          PRUint64* aVersion,
                          ObjectStoreInfoArray& aObjectStores);

  static nsresult
  SetDatabaseMetadata(DatabaseInfo* aDatabaseInfo,
                      PRUint64 aVersion,
                      ObjectStoreInfoArray& aObjectStores);

private:
  IDBFactory();
  ~IDBFactory();

  nsresult
  OpenCommon(const nsAString& aName,
             PRInt64 aVersion,
             bool aDeleting,
             nsIIDBOpenDBRequest** _retval);

  // If this factory lives on a window then mWindow must be non-null. Otherwise
  // mOwningObject must be non-null.
  nsCOMPtr<nsPIDOMWindow> mWindow;
  JSObject* mOwningObject;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbfactory_h__
