/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Storage_h
#define mozilla_dom_Storage_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "nsIDOMStorage.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"

class nsIPrincipal;
class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

class StorageManagerBase;
class StorageCache;

class Storage final
  : public nsIDOMStorage
  , public nsSupportsWeakReference
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(Storage,
                                                         nsIDOMStorage)

  enum StorageType {
    LocalStorage = 1,
    SessionStorage = 2
  };

  StorageType GetType() const;

  StorageManagerBase* GetManager() const
  {
    return mManager;
  }

  StorageCache const* GetCache() const
  {
    return mCache;
  }

  nsIPrincipal* GetPrincipal();
  bool PrincipalEquals(nsIPrincipal* aPrincipal);
  bool CanAccess(nsIPrincipal* aPrincipal);

  Storage(nsPIDOMWindowInner* aWindow,
          StorageManagerBase* aManager,
          StorageCache* aCache,
          const nsAString& aDocumentURI,
          nsIPrincipal* aPrincipal);

  // WebIDL
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mWindow;
  }

  uint32_t GetLength(nsIPrincipal& aSubjectPrincipal,
                     ErrorResult& aRv);

  void Key(uint32_t aIndex, nsAString& aResult,
           nsIPrincipal& aSubjectPrincipal,
           ErrorResult& aRv);

  void GetItem(const nsAString& aKey, nsAString& aResult,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aRv);

  void GetSupportedNames(nsTArray<nsString>& aKeys);

  void NamedGetter(const nsAString& aKey, bool& aFound, nsAString& aResult,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aRv)
  {
    GetItem(aKey, aResult, aSubjectPrincipal, aRv);
    aFound = !aResult.IsVoid();
  }

  void SetItem(const nsAString& aKey, const nsAString& aValue,
               nsIPrincipal& aSubjectPrincipal,
               ErrorResult& aRv);

  void NamedSetter(const nsAString& aKey, const nsAString& aValue,
                   nsIPrincipal& aSubjectPrincipal,
                   ErrorResult& aRv)
  {
    SetItem(aKey, aValue, aSubjectPrincipal, aRv);
  }

  void RemoveItem(const nsAString& aKey,
                  nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aRv);

  void NamedDeleter(const nsAString& aKey, bool& aFound,
                    nsIPrincipal& aSubjectPrincipal,
                    ErrorResult& aRv)
  {
    RemoveItem(aKey, aSubjectPrincipal, aRv);

    aFound = !aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION);
  }

  void Clear(nsIPrincipal& aSubjectPrincipal,
             ErrorResult& aRv);

  bool IsPrivate() const;
  bool IsSessionOnly() const { return mIsSessionOnly; }

  bool IsForkOf(const Storage* aOther) const
  {
    MOZ_ASSERT(aOther);
    return mCache == aOther->mCache;
  }

protected:
  // The method checks whether the caller can use a storage.
  // CanUseStorage is called before any DOM initiated operation
  // on a storage is about to happen and ensures that the storage's
  // session-only flag is properly set according the current settings.
  // It is an optimization since the privileges check and session only
  // state determination are complex and share the code (comes hand in
  // hand together).
  bool CanUseStorage(nsIPrincipal& aSubjectPrincipal);

private:
  ~Storage();

  friend class StorageManagerBase;
  friend class StorageCache;

  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<StorageManagerBase> mManager;
  RefPtr<StorageCache> mCache;
  nsString mDocumentURI;

  // Principal this Storage (i.e. localStorage or sessionStorage) has
  // been created for
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // Whether storage is set to persist data only per session, may change
  // dynamically and is set by CanUseStorage function that is called
  // before any operation on the storage.
  bool mIsSessionOnly : 1;

  void BroadcastChangeNotification(const nsSubstring& aKey,
                                   const nsSubstring& aOldValue,
                                   const nsSubstring& aNewValue);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_Storage_h
