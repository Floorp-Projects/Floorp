/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_idbfactory_h__
#define mozilla_dom_indexeddb_idbfactory_h__

#include "mozilla/dom/BindingDeclarations.h" // for Optional
#include "mozilla/dom/StorageTypeBinding.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/dom/quota/StoragePrivilege.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

class mozIStorageConnection;
class nsIFile;
class nsIFileURL;
class nsIPrincipal;
class nsPIDOMWindow;
template<typename> class nsRefPtr;

namespace mozilla {
class ErrorResult;

namespace dom {
class nsIContentParent;
struct IDBOpenDBOptions;

namespace indexedDB {

struct DatabaseInfo;
class IDBDatabase;
class IDBOpenDBRequest;
class IndexedDBChild;
class IndexedDBParent;

struct ObjectStoreInfo;

class IDBFactory MOZ_FINAL : public nsISupports,
                             public nsWrapperCache
{
  typedef mozilla::dom::nsIContentParent nsIContentParent;
  typedef mozilla::dom::quota::PersistenceType PersistenceType;
  typedef nsTArray<nsRefPtr<ObjectStoreInfo> > ObjectStoreInfoArray;
  typedef mozilla::dom::quota::StoragePrivilege StoragePrivilege;

public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBFactory)

  // Called when using IndexedDB from a window in a different process.
  static nsresult Create(nsPIDOMWindow* aWindow,
                         const nsACString& aGroup,
                         const nsACString& aASCIIOrigin,
                         nsIContentParent* aContentParent,
                         IDBFactory** aFactory);

  // Called when using IndexedDB from a window in the current process.
  static nsresult Create(nsPIDOMWindow* aWindow,
                         nsIContentParent* aContentParent,
                         IDBFactory** aFactory)
  {
    return Create(aWindow, EmptyCString(), EmptyCString(), aContentParent,
                  aFactory);
  }

  // Called when using IndexedDB from a JS component or a JSM in the current
  // process.
  static nsresult Create(JSContext* aCx,
                         JS::Handle<JSObject*> aOwningObject,
                         nsIContentParent* aContentParent,
                         IDBFactory** aFactory);

  // Called when using IndexedDB from a JS component or a JSM in a different
  // process or from a C++ component.
  static nsresult Create(nsIContentParent* aContentParent,
                         IDBFactory** aFactory);

  static already_AddRefed<nsIFileURL>
  GetDatabaseFileURL(nsIFile* aDatabaseFile,
                     PersistenceType aPersistenceType,
                     const nsACString& aGroup,
                     const nsACString& aOrigin);

  static already_AddRefed<mozIStorageConnection>
  GetConnection(const nsAString& aDatabaseFilePath,
                PersistenceType aPersistenceType,
                const nsACString& aGroup,
                const nsACString& aOrigin);

  static nsresult
  SetDefaultPragmas(mozIStorageConnection* aConnection);

  static nsresult
  LoadDatabaseInformation(mozIStorageConnection* aConnection,
                          const nsACString& aDatabaseId,
                          uint64_t* aVersion,
                          ObjectStoreInfoArray& aObjectStores);

  static nsresult
  SetDatabaseMetadata(DatabaseInfo* aDatabaseInfo,
                      uint64_t aVersion,
                      ObjectStoreInfoArray& aObjectStores);

  nsresult
  OpenInternal(const nsAString& aName,
               int64_t aVersion,
               PersistenceType aPersistenceType,
               const nsACString& aGroup,
               const nsACString& aASCIIOrigin,
               StoragePrivilege aStoragePrivilege,
               bool aDeleting,
               IDBOpenDBRequest** _retval);

  nsresult
  OpenInternal(const nsAString& aName,
               int64_t aVersion,
               PersistenceType aPersistenceType,
               bool aDeleting,
               IDBOpenDBRequest** _retval)
  {
    return OpenInternal(aName, aVersion, aPersistenceType, mGroup, mASCIIOrigin,
                        mPrivilege, aDeleting, _retval);
  }

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

  bool
  FromIPC()
  {
    return !!mContentParent;
  }

  // nsWrapperCache
  virtual JSObject*
  WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  // WebIDL
  nsPIDOMWindow*
  GetParentObject() const
  {
    return mWindow;
  }

  already_AddRefed<IDBOpenDBRequest>
  Open(const nsAString& aName, uint64_t aVersion, ErrorResult& aRv)
  {
    return Open(nullptr, aName, Optional<uint64_t>(aVersion),
                Optional<mozilla::dom::StorageType>(), false, aRv);
  }

  already_AddRefed<IDBOpenDBRequest>
  Open(const nsAString& aName, const IDBOpenDBOptions& aOptions,
       ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  DeleteDatabase(const nsAString& aName, const IDBOpenDBOptions& aOptions,
                 ErrorResult& aRv);

  int16_t
  Cmp(JSContext* aCx, JS::Handle<JS::Value> aFirst,
      JS::Handle<JS::Value> aSecond, ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  OpenForPrincipal(nsIPrincipal* aPrincipal, const nsAString& aName,
                   uint64_t aVersion, ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  OpenForPrincipal(nsIPrincipal* aPrincipal, const nsAString& aName,
                   const IDBOpenDBOptions& aOptions, ErrorResult& aRv);

  already_AddRefed<IDBOpenDBRequest>
  DeleteForPrincipal(nsIPrincipal* aPrincipal, const nsAString& aName,
                     const IDBOpenDBOptions& aOptions, ErrorResult& aRv);

private:
  IDBFactory();
  ~IDBFactory();

  already_AddRefed<IDBOpenDBRequest>
  Open(nsIPrincipal* aPrincipal, const nsAString& aName,
       const Optional<uint64_t>& aVersion,
       const Optional<mozilla::dom::StorageType>& aStorageType, bool aDelete,
       ErrorResult& aRv);

  nsCString mGroup;
  nsCString mASCIIOrigin;
  StoragePrivilege mPrivilege;
  PersistenceType mDefaultPersistenceType;

  // If this factory lives on a window then mWindow must be non-null. Otherwise
  // mOwningObject must be non-null.
  nsCOMPtr<nsPIDOMWindow> mWindow;
  JS::Heap<JSObject*> mOwningObject;

  IndexedDBChild* mActorChild;
  IndexedDBParent* mActorParent;

  mozilla::dom::nsIContentParent* mContentParent;

  bool mRootedOwningObject;
};

} // namespace indexedDB
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_indexeddb_idbfactory_h__
