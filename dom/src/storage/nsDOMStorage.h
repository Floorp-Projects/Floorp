/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMStorage_h___
#define nsDOMStorage_h___

#include "nscore.h"
#include "nsAutoPtr.h"
#include "nsIDOMStorageObsolete.h"
#include "nsIDOMStorage.h"
#include "nsIDOMStorageItem.h"
#include "nsIPermissionManager.h"
#include "nsIPrivacyTransitionObserver.h"
#include "nsInterfaceHashtable.h"
#include "nsVoidArray.h"
#include "nsTArray.h"
#include "nsPIDOMStorage.h"
#include "nsIDOMToString.h"
#include "nsDOMEvent.h"
#include "nsIDOMStorageEvent.h"
#include "nsIDOMStorageManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWeakReference.h"
#include "nsIInterfaceRequestor.h"

#include "nsDOMStorageDBWrapper.h"

#define IS_PERMISSION_ALLOWED(perm) \
      ((perm) != nsIPermissionManager::UNKNOWN_ACTION && \
      (perm) != nsIPermissionManager::DENY_ACTION)

class nsDOMStorage;
class nsIDOMStorage;
class nsDOMStorageItem;
class nsDOMStoragePersistentDB;

namespace mozilla {
namespace dom {
class StorageParent;
}
}
using mozilla::dom::StorageParent;

class DOMStorageImpl;

class nsDOMStorageEntry : public nsPtrHashKey<const void>
{
public:
  nsDOMStorageEntry(KeyTypePointer aStr);
  nsDOMStorageEntry(const nsDOMStorageEntry& aToCopy);
  ~nsDOMStorageEntry();

  // weak reference so that it can be deleted when no longer used
  DOMStorageImpl* mStorage;
};

class nsSessionStorageEntry : public nsStringHashKey
{
public:
  nsSessionStorageEntry(KeyTypePointer aStr);
  nsSessionStorageEntry(const nsSessionStorageEntry& aToCopy);
  ~nsSessionStorageEntry();

  nsRefPtr<nsDOMStorageItem> mItem;
};

class nsDOMStorageManager : public nsIDOMStorageManager
                          , public nsIObserver
                          , public nsSupportsWeakReference
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMStorageManager
  NS_DECL_NSIDOMSTORAGEMANAGER

  // nsIObserver
  NS_DECL_NSIOBSERVER

  nsDOMStorageManager();

  void AddToStoragesHash(DOMStorageImpl* aStorage);
  void RemoveFromStoragesHash(DOMStorageImpl* aStorage);

  nsresult ClearAllStorages();

  static nsresult Initialize();
  static nsDOMStorageManager* GetInstance();
  static void Shutdown();
  static void ShutdownDB();

  /**
   * Checks whether there is any data waiting to be flushed from a temp table.
   */
  bool UnflushedDataExists();

  static nsDOMStorageManager* gStorageManager;

protected:

  nsTHashtable<nsDOMStorageEntry> mStorages;
};

class DOMStorageBase : public nsIPrivacyTransitionObserver
{
public:
  DOMStorageBase();
  DOMStorageBase(DOMStorageBase&);

  virtual void InitAsSessionStorage(nsIURI* aDomainURI, bool aPrivate);
  virtual void InitAsLocalStorage(nsIURI* aDomainURI, bool aCanUseChromePersist, bool aPrivate);

  virtual nsTArray<nsString>* GetKeys(bool aCallerSecure) = 0;
  virtual nsresult GetLength(bool aCallerSecure, PRUint32* aLength) = 0;
  virtual nsresult GetKey(bool aCallerSecure, PRUint32 aIndex, nsAString& aKey) = 0;
  virtual nsIDOMStorageItem* GetValue(bool aCallerSecure, const nsAString& aKey,
                                      nsresult* rv) = 0;
  virtual nsresult SetValue(bool aCallerSecure, const nsAString& aKey,
                            const nsAString& aData, nsAString& aOldValue) = 0;
  virtual nsresult RemoveValue(bool aCallerSecure, const nsAString& aKey,
                               nsAString& aOldValue) = 0;
  virtual nsresult Clear(bool aCallerSecure, PRInt32* aOldCount) = 0;

  // Call nsDOMStorage::CanUseStorage with |this|
  bool CanUseStorage();

  // If true, the contents of the storage should be stored in the
  // database, otherwise this storage should act like a session
  // storage.
  // This call relies on mSessionOnly, and should only be used
  // after a CacheStoragePermissions() call.  See the comments
  // for mSessionOnly below.
  bool UseDB() {
    return mUseDB;
  }

  bool IsPrivate() {
    return mInPrivateBrowsing;
  }

  // retrieve the value and secure state corresponding to a key out of storage.
  virtual nsresult
  GetDBValue(const nsAString& aKey,
             nsAString& aValue,
             bool* aSecure) = 0;

  // set the value corresponding to a key in the storage. If
  // aSecure is false, then attempts to modify a secure value
  // throw NS_ERROR_DOM_INVALID_ACCESS_ERR
  virtual nsresult
  SetDBValue(const nsAString& aKey,
             const nsAString& aValue,
             bool aSecure) = 0;

  // set the value corresponding to a key as secure.
  virtual nsresult
  SetSecure(const nsAString& aKey, bool aSecure) = 0;

  virtual nsresult
  CloneFrom(bool aCallerSecure, DOMStorageBase* aThat) = 0;

  // e.g. "moc.rab.oof.:" or "moc.rab.oof.:http:80" depending
  // on association with a domain (globalStorage) or
  // an origin (localStorage).
  nsCString& GetScopeDBKey() {return mScopeDBKey;}

  // e.g. "moc.rab.%" - reversed eTLD+1 subpart of the domain or
  // reversed offline application allowed domain.
  nsCString& GetQuotaDomainDBKey(bool aOfflineAllowed)
  {
    return aOfflineAllowed ? mQuotaDomainDBKey : mQuotaETLDplus1DomainDBKey;
  }

  virtual bool CacheStoragePermissions() = 0;

protected:
  friend class nsDOMStorageManager;
  friend class nsDOMStorage;

  nsPIDOMStorage::nsDOMStorageType mStorageType;
  
  // true if the storage database should be used for values
  bool mUseDB;

  // true if the preferences indicates that this storage should be
  // session only. This member is updated by
  // CacheStoragePermissions(), using the current principal.
  // CacheStoragePermissions() must be called at each entry point to
  // make sure this stays up to date.
  bool mSessionOnly;

  // domain this store is associated with
  nsCString mDomain;

  // keys are used for database queries.
  // see comments of the getters bellow.
  nsCString mScopeDBKey;
  nsCString mQuotaETLDplus1DomainDBKey;
  nsCString mQuotaDomainDBKey;

  bool mCanUseChromePersist;
  bool mInPrivateBrowsing;
};

class DOMStorageImpl : public DOMStorageBase
                     , public nsSupportsWeakReference
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(DOMStorageImpl, nsIPrivacyTransitionObserver)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIPRIVACYTRANSITIONOBSERVER

  DOMStorageImpl(nsDOMStorage*);
  DOMStorageImpl(nsDOMStorage*, DOMStorageImpl&);
  ~DOMStorageImpl();

  virtual void InitAsSessionStorage(nsIURI* aDomainURI, bool aPrivate);
  virtual void InitAsLocalStorage(nsIURI* aDomainURI, bool aCanUseChromePersist, bool aPrivate);

  bool SessionOnly() {
    return mSessionOnly;
  }

  virtual nsTArray<nsString>* GetKeys(bool aCallerSecure);
  virtual nsresult GetLength(bool aCallerSecure, PRUint32* aLength);
  virtual nsresult GetKey(bool aCallerSecure, PRUint32 aIndex, nsAString& aKey);
  virtual nsIDOMStorageItem* GetValue(bool aCallerSecure, const nsAString& aKey,
                                      nsresult* rv);
  virtual nsresult SetValue(bool aCallerSecure, const nsAString& aKey,
                            const nsAString& aData, nsAString& aOldValue);
  virtual nsresult RemoveValue(bool aCallerSecure, const nsAString& aKey,
                               nsAString& aOldValue);
  virtual nsresult Clear(bool aCallerSecure, PRInt32* aOldCount);

  // cache the keys from the database for faster lookup
  nsresult CacheKeysFromDB();

  PRUint64 CachedVersion() { return mItemsCachedVersion; }
  void SetCachedVersion(PRUint64 version) { mItemsCachedVersion = version; }
  
  // Some privileged internal pages can use a persistent storage even in
  // session-only or private-browsing modes.
  bool CanUseChromePersist();

  // retrieve the value and secure state corresponding to a key out of storage
  // that has been cached in mItems hash table.
  nsresult
  GetCachedValue(const nsAString& aKey,
                 nsAString& aValue,
                 bool* aSecure);

  // retrieve the value and secure state corresponding to a key out of storage.
  virtual nsresult
  GetDBValue(const nsAString& aKey,
             nsAString& aValue,
             bool* aSecure);

  // set the value corresponding to a key in the storage. If
  // aSecure is false, then attempts to modify a secure value
  // throw NS_ERROR_DOM_INVALID_ACCESS_ERR
  virtual nsresult
  SetDBValue(const nsAString& aKey,
             const nsAString& aValue,
             bool aSecure);

  // set the value corresponding to a key as secure.
  virtual nsresult
  SetSecure(const nsAString& aKey, bool aSecure);

  // clear all values from the store
  void ClearAll();

  virtual nsresult
  CloneFrom(bool aCallerSecure, DOMStorageBase* aThat);

  virtual bool CacheStoragePermissions();

private:
  static nsDOMStorageDBWrapper* gStorageDB;
  friend class nsDOMStorageManager;
  friend class nsDOMStoragePersistentDB;
  friend class StorageParent;

  void Init(nsDOMStorage*);

  // Cross-process storage implementations never have InitAs(Session|Local|Global)Storage
  // called, so the appropriate initialization needs to happen from the child.
  void InitFromChild(bool aUseDB, bool aCanUseChromePersist, bool aSessionOnly,
                     bool aPrivate, const nsACString& aDomain,
                     const nsACString& aScopeDBKey,
                     const nsACString& aQuotaDomainDBKey,
                     const nsACString& aQuotaETLDplus1DomainDBKey,
                     PRUint32 aStorageType);
  void SetSessionOnly(bool aSessionOnly);

  static nsresult InitDB();

  // 0 initially or a positive data version number assigned by gStorageDB
  // after keys have been cached from the database
  PRUint64 mItemsCachedVersion;

  // the key->value item pairs
  nsTHashtable<nsSessionStorageEntry> mItems;

  // Weak reference to the owning storage instance
  nsDOMStorage* mOwner;
};

class nsDOMStorage2;

class nsDOMStorage : public nsIDOMStorageObsolete,
                     public nsPIDOMStorage,
                     public nsIInterfaceRequestor
{
public:
  nsDOMStorage();
  nsDOMStorage(nsDOMStorage& aThat);
  virtual ~nsDOMStorage();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorage, nsIDOMStorageObsolete)

  NS_DECL_NSIDOMSTORAGEOBSOLETE
  NS_DECL_NSIINTERFACEREQUESTOR

  // Helpers for implementing nsIDOMStorage
  nsresult GetItem(const nsAString& key, nsAString& aData);
  nsresult Clear();

  // nsPIDOMStorage
  virtual nsresult InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                        bool aPrivate);
  virtual nsresult InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                      bool aPrivate);
  virtual already_AddRefed<nsIDOMStorage> Clone();
  virtual already_AddRefed<nsIDOMStorage> Fork(const nsSubstring &aDocumentURI);
  virtual bool IsForkOf(nsIDOMStorage* aThat);
  virtual nsTArray<nsString> *GetKeys();
  virtual nsIPrincipal* Principal();
  virtual bool CanAccess(nsIPrincipal *aPrincipal);
  virtual nsDOMStorageType StorageType();

  // Check whether storage may be used by the caller, and whether it
  // is session only.  Returns true if storage may be used.
  static bool
  CanUseStorage(DOMStorageBase* aStorage = nsnull);

  // Check whether this URI can use chrome persist storage.  This kind of
  // storage can bypass cookies limits, private browsing and uses the offline
  // apps quota.
  static bool
  URICanUseChromePersist(nsIURI* aURI);
  
  // Check whether storage may be used.  Updates mSessionOnly based on
  // the result of CanUseStorage.
  bool
  CacheStoragePermissions();

  nsIDOMStorageItem* GetNamedItem(const nsAString& aKey, nsresult* aResult);

  nsresult SetSecure(const nsAString& aKey, bool aSecure)
  {
    return mStorageImpl->SetSecure(aKey, aSecure);
  }

  nsresult CloneFrom(nsDOMStorage* aThat);

 protected:
  friend class nsDOMStorage2;
  friend class nsDOMStoragePersistentDB;

  nsRefPtr<DOMStorageBase> mStorageImpl;

  bool CanAccessSystem(nsIPrincipal *aPrincipal);

  // document URI of the document this storage is bound to
  nsString mDocumentURI;

  // true if this storage was initialized as a localStorage object.  localStorage
  // objects are scoped to scheme/host/port in the database, while globalStorage
  // objects are scoped just to host.  this flag also tells the manager to map
  // this storage also in mLocalStorages hash table.
  nsDOMStorageType mStorageType;

  friend class nsIDOMStorage2;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsDOMStorage2* mEventBroadcaster;
};

class nsDOMStorage2 : public nsIDOMStorage,
                      public nsPIDOMStorage,
                      public nsIInterfaceRequestor
{
public:
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorage2, nsIDOMStorage)

  nsDOMStorage2(nsDOMStorage2& aThat);
  nsDOMStorage2();

  NS_DECL_NSIDOMSTORAGE
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsPIDOMStorage
  virtual nsresult InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                        bool aPrivate);
  virtual nsresult InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI,
                                      bool aPrivate);
  virtual already_AddRefed<nsIDOMStorage> Clone();
  virtual already_AddRefed<nsIDOMStorage> Fork(const nsSubstring &aDocumentURI);
  virtual bool IsForkOf(nsIDOMStorage* aThat);
  virtual nsTArray<nsString> *GetKeys();
  virtual nsIPrincipal* Principal();
  virtual bool CanAccess(nsIPrincipal *aPrincipal);
  virtual nsDOMStorageType StorageType();

  void BroadcastChangeNotification(const nsSubstring &aKey,
                                   const nsSubstring &aOldValue,
                                   const nsSubstring &aNewValue);
  void InitAsSessionStorageFork(nsIPrincipal *aPrincipal,
                                const nsSubstring &aDocumentURI,
                                nsDOMStorage* aStorage);

private:
  // storages bound to an origin hold the principal to
  // make security checks against it
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // Needed for the storage event, this is address of the document this storage
  // is bound to
  nsString mDocumentURI;
  nsRefPtr<nsDOMStorage> mStorage;
};

class nsDOMStorageItem : public nsIDOMStorageItem,
                         public nsIDOMToString
{
public:
  nsDOMStorageItem(DOMStorageBase* aStorage,
                   const nsAString& aKey,
                   const nsAString& aValue,
                   bool aSecure);
  virtual ~nsDOMStorageItem();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorageItem, nsIDOMStorageItem)

  // nsIDOMStorageObsolete
  NS_DECL_NSIDOMSTORAGEITEM

  // nsIDOMToString
  NS_DECL_NSIDOMTOSTRING

  bool IsSecure()
  {
    return mSecure;
  }

  void SetSecureInternal(bool aSecure)
  {
    mSecure = aSecure;
  }

  const nsAString& GetValueInternal()
  {
    return mValue;
  }

  const void SetValueInternal(const nsAString& aValue)
  {
    mValue = aValue;
  }

  void ClearValue()
  {
    mValue.Truncate();
  }

protected:

  // true if this value is for secure sites only
  bool mSecure;

  // key for the item
  nsString mKey;

  // value of the item
  nsString mValue;

  // If this item came from the db, mStorage points to the storage
  // object where this item came from.
  nsRefPtr<DOMStorageBase> mStorage;
};

class nsDOMStorageEvent : public nsDOMEvent,
                          public nsIDOMStorageEvent
{
public:
  nsDOMStorageEvent()
    : nsDOMEvent(nsnull, nsnull)
  {
  }

  virtual ~nsDOMStorageEvent()
  {
  }

  nsresult Init();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(nsDOMStorageEvent, nsDOMEvent)

  NS_DECL_NSIDOMSTORAGEEVENT
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

  virtual nsresult InitFromCtor(const nsAString& aType,
                                JSContext* aCx, jsval* aVal);
protected:
  nsString mKey;
  nsString mOldValue;
  nsString mNewValue;
  nsString mUrl;
  nsCOMPtr<nsIDOMStorage> mStorageArea;
};

nsresult
NS_NewDOMStorage2(nsISupports* aOuter, REFNSIID aIID, void** aResult);

PRUint32
GetOfflinePermission(const nsACString &aDomain);

bool
IsOfflineAllowed(const nsACString &aDomain);

#endif /* nsDOMStorage_h___ */
