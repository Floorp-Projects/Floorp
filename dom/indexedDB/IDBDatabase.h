/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbdatabase_h__
#define mozilla_dom_indexeddb_idbdatabase_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "nsIDocument.h"
#include "nsIFileStorage.h"
#include "nsIOfflineStorage.h"

#include "mozilla/Attributes.h"
#include "mozilla/dom/IDBObjectStoreBinding.h"
#include "mozilla/dom/IDBTransactionBinding.h"
#include "nsDOMEventTargetHelper.h"

#include "mozilla/dom/indexedDB/FileManager.h"
#include "mozilla/dom/indexedDB/IDBRequest.h"
#include "mozilla/dom/indexedDB/IDBWrapperCache.h"

class nsIScriptContext;
class nsPIDOMWindow;

namespace mozilla {
namespace dom {
class ContentParent;
namespace quota {
class Client;
}
}
}

BEGIN_INDEXEDDB_NAMESPACE

class AsyncConnectionHelper;
struct DatabaseInfo;
class IDBFactory;
class IDBIndex;
class IDBObjectStore;
class IDBTransaction;
class IndexedDatabaseManager;
class IndexedDBDatabaseChild;
class IndexedDBDatabaseParent;
struct ObjectStoreInfoGuts;

class IDBDatabase : public IDBWrapperCache,
                    public nsIOfflineStorage
{
  friend class AsyncConnectionHelper;
  friend class IndexedDatabaseManager;
  friend class IndexedDBDatabaseChild;

public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIFILESTORAGE
  NS_DECL_NSIOFFLINESTORAGE

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(IDBDatabase, IDBWrapperCache)

  static already_AddRefed<IDBDatabase>
  Create(IDBWrapperCache* aOwnerCache,
         IDBFactory* aFactory,
         already_AddRefed<DatabaseInfo> aDatabaseInfo,
         const nsACString& aASCIIOrigin,
         FileManager* aFileManager,
         mozilla::dom::ContentParent* aContentParent);

  static IDBDatabase*
  FromStorage(nsIOfflineStorage* aStorage);

  static IDBDatabase*
  FromStorage(nsIFileStorage* aStorage)
  {
    nsCOMPtr<nsIOfflineStorage> storage = do_QueryInterface(aStorage);
    return storage ? FromStorage(storage) : nullptr;
  }

  // nsIDOMEventTarget
  virtual nsresult PostHandleEvent(nsEventChainPostVisitor& aVisitor) MOZ_OVERRIDE;

  DatabaseInfo* Info() const
  {
    return mDatabaseInfo;
  }

  const nsString& Name() const
  {
    return mName;
  }

  const nsString& FilePath() const
  {
    return mFilePath;
  }

  already_AddRefed<nsIDocument> GetOwnerDocument()
  {
    if (!GetOwner()) {
      return nullptr;
    }

    nsCOMPtr<nsIDocument> doc = GetOwner()->GetExtantDoc();
    return doc.forget();
  }

  void DisconnectFromActorParent();

  void CloseInternal(bool aIsDead);

  void EnterSetVersionTransaction();
  void ExitSetVersionTransaction();

  // Called when a versionchange transaction is aborted to reset the
  // DatabaseInfo.
  void RevertToPreviousState();

  FileManager* Manager() const
  {
    return mFileManager;
  }

  void
  SetActor(IndexedDBDatabaseChild* aActorChild)
  {
    NS_ASSERTION(!aActorChild || !mActorChild, "Shouldn't have more than one!");
    mActorChild = aActorChild;
  }

  void
  SetActor(IndexedDBDatabaseParent* aActorParent)
  {
    NS_ASSERTION(!aActorParent || !mActorParent,
                 "Shouldn't have more than one!");
    mActorParent = aActorParent;
  }

  IndexedDBDatabaseChild*
  GetActorChild() const
  {
    return mActorChild;
  }

  IndexedDBDatabaseParent*
  GetActorParent() const
  {
    return mActorParent;
  }

  mozilla::dom::ContentParent*
  GetContentParent() const
  {
    return mContentParent;
  }

  already_AddRefed<IDBObjectStore>
  CreateObjectStoreInternal(IDBTransaction* aTransaction,
                            const ObjectStoreInfoGuts& aInfo,
                            ErrorResult& aRv);

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope) MOZ_OVERRIDE;

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    return GetOwner();
  }

  void
  GetName(nsString& aName) const
  {
    NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
    aName.Assign(mName);
  }

  uint64_t
  Version() const;

  already_AddRefed<nsIDOMDOMStringList>
  GetObjectStoreNames(ErrorResult& aRv) const;

  already_AddRefed<IDBObjectStore>
  CreateObjectStore(JSContext* aCx, const nsAString& aName,
                    const IDBObjectStoreParameters& aOptionalParameters,
                    ErrorResult& aRv);

  void
  DeleteObjectStore(const nsAString& name, ErrorResult& aRv);

  already_AddRefed<indexedDB::IDBTransaction>
  Transaction(const nsAString& aStoreName, IDBTransactionMode aMode,
              ErrorResult& aRv)
  {
    Sequence<nsString> list;
    list.AppendElement(aStoreName);
    return Transaction(list, aMode, aRv);
  }

  already_AddRefed<indexedDB::IDBTransaction>
  Transaction(const Sequence<nsString>& aStoreNames, IDBTransactionMode aMode,
              ErrorResult& aRv);

  IMPL_EVENT_HANDLER(abort)
  IMPL_EVENT_HANDLER(error)
  IMPL_EVENT_HANDLER(versionchange)

  already_AddRefed<IDBRequest>
  MozCreateFileHandle(const nsAString& aName, const Optional<nsAString>& aType,
                      ErrorResult& aRv);

  virtual void LastRelease() MOZ_OVERRIDE;
private:
  IDBDatabase();
  ~IDBDatabase();

  void OnUnlink();

  // The factory must be kept alive when IndexedDB is used in multiple
  // processes. If it dies then the entire actor tree will be destroyed with it
  // and the world will explode.
  nsRefPtr<IDBFactory> mFactory;

  nsRefPtr<DatabaseInfo> mDatabaseInfo;

  // Set to a copy of the existing DatabaseInfo when starting a versionchange
  // transaction.
  nsRefPtr<DatabaseInfo> mPreviousDatabaseInfo;
  nsCOMPtr<nsIAtom> mDatabaseId;
  nsString mName;
  nsString mFilePath;
  nsCString mASCIIOrigin;

  nsRefPtr<FileManager> mFileManager;

  IndexedDBDatabaseChild* mActorChild;
  IndexedDBDatabaseParent* mActorParent;

  mozilla::dom::ContentParent* mContentParent;

  nsRefPtr<mozilla::dom::quota::Client> mQuotaClient;

  bool mInvalidated;
  bool mRegistered;
  bool mClosed;
  bool mRunningVersionChange;
};

END_INDEXEDDB_NAMESPACE

#endif // mozilla_dom_indexeddb_idbdatabase_h__
