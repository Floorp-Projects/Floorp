/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStorage_h___
#define nsDOMStorage_h___

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "nsIDOMStorage.h"
#include "nsAutoPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWeakReference.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"

class nsIPrincipal;
class nsIDOMWindow;

namespace mozilla {
namespace dom {

class DOMStorageManager;
class DOMStorageCache;

class DOMStorage MOZ_FINAL
  : public nsIDOMStorage
  , public nsSupportsWeakReference
  , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_AMBIGUOUS(DOMStorage,
                                                         nsIDOMStorage)

  enum StorageType {
    LocalStorage = 1,
    SessionStorage = 2
  };

  StorageType GetType() const;

  DOMStorageManager* GetManager() const
  {
    return mManager;
  }

  DOMStorageCache const* GetCache() const
  {
    return mCache;
  }

  nsIPrincipal* GetPrincipal();
  bool PrincipalEquals(nsIPrincipal* aPrincipal);
  bool CanAccess(nsIPrincipal* aPrincipal);
  bool IsPrivate()
  {
    return mIsPrivate;
  }

  DOMStorage(nsIDOMWindow* aWindow,
             DOMStorageManager* aManager,
             DOMStorageCache* aCache,
             const nsAString& aDocumentURI,
             nsIPrincipal* aPrincipal,
             bool aIsPrivate);

  // WebIDL
  JSObject* WrapObject(JSContext* aCx) MOZ_OVERRIDE;

  nsIDOMWindow* GetParentObject() const
  {
    return mWindow;
  }

  uint32_t GetLength(ErrorResult& aRv);

  void Key(uint32_t aIndex, nsAString& aResult, ErrorResult& aRv);

  void GetItem(const nsAString& aKey, nsAString& aResult, ErrorResult& aRv);

  bool NameIsEnumerable(const nsAString& aName) const
  {
    return true;
  }

  void GetSupportedNames(unsigned, nsTArray<nsString>& aKeys);

  void NamedGetter(const nsAString& aKey, bool& aFound, nsAString& aResult,
	           ErrorResult& aRv)
  {
    GetItem(aKey, aResult, aRv);
    aFound = !aResult.IsVoid();
  }

  void SetItem(const nsAString& aKey, const nsAString& aValue,
               ErrorResult& aRv);

  void NamedSetter(const nsAString& aKey, const nsAString& aValue,
                   ErrorResult& aRv)
  {
    SetItem(aKey, aValue, aRv);
  }

  void RemoveItem(const nsAString& aKey, ErrorResult& aRv);

  void NamedDeleter(const nsAString& aKey, bool& aFound, ErrorResult& aRv)
  {
    RemoveItem(aKey, aRv);

    aFound = (aRv.ErrorCode() != NS_SUCCESS_DOM_NO_OPERATION);
  }

  void Clear(ErrorResult& aRv);

  // The method checks whether the caller can use a storage.
  // CanUseStorage is called before any DOM initiated operation
  // on a storage is about to happen and ensures that the storage's
  // session-only flag is properly set according the current settings.
  // It is an optimization since the privileges check and session only
  // state determination are complex and share the code (comes hand in
  // hand together).
  static bool CanUseStorage(DOMStorage* aStorage = nullptr);

  bool IsPrivate() const { return mIsPrivate; }
  bool IsSessionOnly() const { return mIsSessionOnly; }

private:
  ~DOMStorage();

  friend class DOMStorageManager;
  friend class DOMStorageCache;

  nsCOMPtr<nsIDOMWindow> mWindow;
  nsRefPtr<DOMStorageManager> mManager;
  nsRefPtr<DOMStorageCache> mCache;
  nsString mDocumentURI;

  // Principal this DOMStorage (i.e. localStorage or sessionStorage) has
  // been created for
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // Whether this storage is running in private-browsing window.
  bool mIsPrivate : 1;

  // Whether storage is set to persist data only per session, may change
  // dynamically and is set by CanUseStorage function that is called
  // before any operation on the storage.
  bool mIsSessionOnly : 1;

  void BroadcastChangeNotification(const nsSubstring& aKey,
                                   const nsSubstring& aOldValue,
                                   const nsSubstring& aNewValue);
};

} // ::dom
} // ::mozilla

#endif /* nsDOMStorage_h___ */
