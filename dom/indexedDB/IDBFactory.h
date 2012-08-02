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

class nsIAtom;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {
class ContentParent;
}
}

BEGIN_INDEXEDDB_NAMESPACE

struct DatabaseInfo;
class IDBDatabase;
class IDBOpenDBRequest;
class IndexedDBChild;
class IndexedDBParent;

struct ObjectStoreInfo;

class IDBFactory MOZ_FINAL : public nsIIDBFactory
{
  typedef mozilla::dom::ContentParent ContentParent;
  typedef nsTArray<nsRefPtr<ObjectStoreInfo> > ObjectStoreInfoArray;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBFactory)
  NS_DECL_NSIIDBFACTORY

  // Called when using IndexedDB from a window in a different process.
  static nsresult Create(nsPIDOMWindow* aWindow,
                         const nsACString& aASCIIOrigin,
                         ContentParent* aContentParent,
                         IDBFactory** aFactory);

  // Called when using IndexedDB from a window in the current process.
  static nsresult Create(nsPIDOMWindow* aWindow,
                         ContentParent* aContentParent,
                         nsIIDBFactory** aFactory)
  {
    nsRefPtr<IDBFactory> factory;
    nsresult rv =
      Create(aWindow, EmptyCString(), aContentParent, getter_AddRefs(factory));
    NS_ENSURE_SUCCESS(rv, rv);

    factory.forget(aFactory);
    return NS_OK;
  }

  // Called when using IndexedDB from a JS component or a JSM in the current
  // process.
  static nsresult Create(JSContext* aCx,
                         JSObject* aOwningObject,
                         ContentParent* aContentParent,
                         IDBFactory** aFactory);

  // Called when using IndexedDB from a JS component or a JSM in a different
  // process.
  static nsresult Create(ContentParent* aContentParent,
                         IDBFactory** aFactory);

  static already_AddRefed<mozIStorageConnection>
  GetConnection(const nsAString& aDatabaseFilePath);

  static nsresult
  LoadDatabaseInformation(mozIStorageConnection* aConnection,
                          nsIAtom* aDatabaseId,
                          PRUint64* aVersion,
                          ObjectStoreInfoArray& aObjectStores);

  static nsresult
  SetDatabaseMetadata(DatabaseInfo* aDatabaseInfo,
                      PRUint64 aVersion,
                      ObjectStoreInfoArray& aObjectStores);

  nsresult
  OpenCommon(const nsAString& aName,
             PRInt64 aVersion,
             bool aDeleting,
             JSContext* aCallingCx,
             IDBOpenDBRequest** _retval);

  void
  SetActor(IndexedDBChild* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  SetActor(IndexedDBParent* aActorParent)
  {
    NS_ASSERTION(!aActorParent || !mActorParent, "Shouldn't have more than one!");
    mActorParent = aActorParent;
  }

  const nsCString&
  GetASCIIOrigin() const
  {
    return mASCIIOrigin;
  }

private:
  IDBFactory();
  ~IDBFactory();

  nsCString mASCIIOrigin;

  // If this factory lives on a window then mWindow must be non-null. Otherwise
  // mOwningObject must be non-null.
  nsCOMPtr<nsPIDOMWindow> mWindow;
  JSObject* mOwningObject;

  IndexedDBChild* mActorChild;
  IndexedDBParent* mActorParent;

  mozilla::dom::ContentParent* mContentParent;

  bool mRootedOwningObject;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbfactory_h__
