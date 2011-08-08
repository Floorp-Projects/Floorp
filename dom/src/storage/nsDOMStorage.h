/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Neil Deakin <enndeakin@sympatico.ca>
 *   Johnny Stenback <jst@mozilla.com>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsDOMStorage_h___
#define nsDOMStorage_h___

#include "nscore.h"
#include "nsAutoPtr.h"
#include "nsIDOMStorageObsolete.h"
#include "nsIDOMStorage.h"
#include "nsIDOMStorageList.h"
#include "nsIDOMStorageItem.h"
#include "nsIPermissionManager.h"
#include "nsInterfaceHashtable.h"
#include "nsVoidArray.h"
#include "nsTArray.h"
#include "nsPIDOMStorage.h"
#include "nsIDOMToString.h"
#include "nsDOMEvent.h"
#include "nsIDOMStorageEvent.h"
#include "nsIDOMStorageEventObsolete.h"
#include "nsIDOMStorageManager.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIObserver.h"
#include "nsITimer.h"
#include "nsWeakReference.h"

#define NS_DOMSTORAGE_FLUSH_TIMER_OBSERVER "domstorage-flush-timer"

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

class nsDOMStorageEntry : public nsVoidPtrHashKey
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

  PRBool InPrivateBrowsingMode() { return mInPrivateBrowsing; }

  static nsresult Initialize();
  static nsDOMStorageManager* GetInstance();
  static void Shutdown();

  /**
   * Checks whether there is any data waiting to be flushed from a temp table.
   */
  PRBool UnflushedDataExists();

  static nsDOMStorageManager* gStorageManager;

protected:

  nsTHashtable<nsDOMStorageEntry> mStorages;
  PRBool mInPrivateBrowsing;
};

class DOMStorageBase : public nsISupports
{
public:
  DOMStorageBase();
  DOMStorageBase(DOMStorageBase&);

  virtual void InitAsSessionStorage(nsIURI* aDomainURI);
  virtual void InitAsLocalStorage(nsIURI* aDomainURI, bool aCanUseChromePersist);
  virtual void InitAsGlobalStorage(const nsACString& aDomainDemanded);

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
  
  // If true, the contents of the storage should be stored in the
  // database, otherwise this storage should act like a session
  // storage.
  // This call relies on mSessionOnly, and should only be used
  // after a CacheStoragePermissions() call.  See the comments
  // for mSessionOnly below.
  PRBool UseDB() {
    return mUseDB;
  }

  // retrieve the value and secure state corresponding to a key out of storage.
  virtual nsresult
  GetDBValue(const nsAString& aKey,
             nsAString& aValue,
             PRBool* aSecure) = 0;

  // set the value corresponding to a key in the storage. If
  // aSecure is false, then attempts to modify a secure value
  // throw NS_ERROR_DOM_INVALID_ACCESS_ERR
  virtual nsresult
  SetDBValue(const nsAString& aKey,
             const nsAString& aValue,
             PRBool aSecure) = 0;

  // set the value corresponding to a key as secure.
  virtual nsresult
  SetSecure(const nsAString& aKey, PRBool aSecure) = 0;

  virtual nsresult
  CloneFrom(bool aCallerSecure, DOMStorageBase* aThat) = 0;

  // e.g. "moc.rab.oof.:" or "moc.rab.oof.:http:80" depending
  // on association with a domain (globalStorage) or
  // an origin (localStorage).
  nsCString& GetScopeDBKey() {return mScopeDBKey;}

  // e.g. "moc.rab.%" - reversed eTLD+1 subpart of the domain or
  // reversed offline application allowed domain.
  nsCString& GetQuotaDomainDBKey(PRBool aOfflineAllowed)
  {
    return aOfflineAllowed ? mQuotaDomainDBKey : mQuotaETLDplus1DomainDBKey;
  }

  virtual bool CacheStoragePermissions() = 0;

protected:
  friend class nsDOMStorageManager;
  friend class nsDOMStorage;

  nsPIDOMStorage::nsDOMStorageType mStorageType;
  
  // true if the storage database should be used for values
  PRPackedBool mUseDB;

  // true if the preferences indicates that this storage should be
  // session only. This member is updated by
  // CacheStoragePermissions(), using the current principal.
  // CacheStoragePermissions() must be called at each entry point to
  // make sure this stays up to date.
  PRPackedBool mSessionOnly;

  // domain this store is associated with
  nsCString mDomain;

  // keys are used for database queries.
  // see comments of the getters bellow.
  nsCString mScopeDBKey;
  nsCString mQuotaETLDplus1DomainDBKey;
  nsCString mQuotaDomainDBKey;

  bool mCanUseChromePersist;
};

class DOMStorageImpl : public DOMStorageBase

{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(DOMStorageImpl)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  DOMStorageImpl(nsDOMStorage*);
  DOMStorageImpl(nsDOMStorage*, DOMStorageImpl&);
  ~DOMStorageImpl();

  virtual void InitAsSessionStorage(nsIURI* aDomainURI);
  virtual void InitAsLocalStorage(nsIURI* aDomainURI, bool aCanUseChromePersist);
  virtual void InitAsGlobalStorage(const nsACString& aDomainDemanded);

  PRBool SessionOnly() {
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
  
  // Some privileged internal pages can use a persistent storage even in
  // session-only or private-browsing modes.
  bool CanUseChromePersist();

  // retrieve the value and secure state corresponding to a key out of storage
  // that has been cached in mItems hash table.
  nsresult
  GetCachedValue(const nsAString& aKey,
                 nsAString& aValue,
                 PRBool* aSecure);

  // retrieve the value and secure state corresponding to a key out of storage.
  virtual nsresult
  GetDBValue(const nsAString& aKey,
             nsAString& aValue,
             PRBool* aSecure);

  // set the value corresponding to a key in the storage. If
  // aSecure is false, then attempts to modify a secure value
  // throw NS_ERROR_DOM_INVALID_ACCESS_ERR
  virtual nsresult
  SetDBValue(const nsAString& aKey,
             const nsAString& aValue,
             PRBool aSecure);

  // set the value corresponding to a key as secure.
  virtual nsresult
  SetSecure(const nsAString& aKey, PRBool aSecure);

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
                     const nsACString& aDomain,
                     const nsACString& aScopeDBKey,
                     const nsACString& aQuotaDomainDBKey,
                     const nsACString& aQuotaETLDplus1DomainDBKey,
                     PRUint32 aStorageType);
  void SetSessionOnly(bool aSessionOnly);

  static nsresult InitDB();

  // true if items from the database are cached
  PRPackedBool mItemsCached;

  // the key->value item pairs
  nsTHashtable<nsSessionStorageEntry> mItems;

  // Weak reference to the owning storage instance
  nsDOMStorage* mOwner;
};

class nsDOMStorage : public nsIDOMStorageObsolete,
                     public nsPIDOMStorage
{
public:
  nsDOMStorage();
  nsDOMStorage(nsDOMStorage& aThat);
  virtual ~nsDOMStorage();

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorage, nsIDOMStorageObsolete)

  NS_DECL_NSIDOMSTORAGEOBSOLETE

  // Helpers for implementing nsIDOMStorage
  nsresult GetItem(const nsAString& key, nsAString& aData);
  nsresult Clear();

  // nsPIDOMStorage
  virtual nsresult InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI);
  virtual nsresult InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI);
  virtual nsresult InitAsGlobalStorage(const nsACString &aDomainDemanded);
  virtual already_AddRefed<nsIDOMStorage> Clone();
  virtual already_AddRefed<nsIDOMStorage> Fork(const nsSubstring &aDocumentURI);
  virtual PRBool IsForkOf(nsIDOMStorage* aThat);
  virtual nsTArray<nsString> *GetKeys();
  virtual nsIPrincipal* Principal();
  virtual PRBool CanAccess(nsIPrincipal *aPrincipal);
  virtual nsDOMStorageType StorageType();
  virtual void BroadcastChangeNotification(const nsSubstring &aKey,
                                           const nsSubstring &aOldValue,
                                           const nsSubstring &aNewValue);

  // Check whether storage may be used by the caller, and whether it
  // is session only.  Returns true if storage may be used.
  static PRBool
  CanUseStorage(PRPackedBool* aSessionOnly);

  // Check whether this URI can use chrome persist storage.  This kind of
  // storage can bypass cookies limits, private browsing and uses the offline
  // apps quota.
  static PRBool
  URICanUseChromePersist(nsIURI* aURI);
  
  // Check whether storage may be used.  Updates mSessionOnly based on
  // the result of CanUseStorage.
  PRBool
  CacheStoragePermissions();

  nsIDOMStorageItem* GetNamedItem(const nsAString& aKey, nsresult* aResult);

  static nsDOMStorage* FromSupports(nsISupports* aSupports)
  {
    return static_cast<nsDOMStorage*>(static_cast<nsIDOMStorageObsolete*>(aSupports));
  }

  nsresult SetSecure(const nsAString& aKey, PRBool aSecure)
  {
    return mStorageImpl->SetSecure(aKey, aSecure);
  }

  nsresult CloneFrom(nsDOMStorage* aThat);

 protected:
  friend class nsDOMStorage2;
  friend class nsDOMStoragePersistentDB;

  nsRefPtr<DOMStorageBase> mStorageImpl;

  PRBool CanAccessSystem(nsIPrincipal *aPrincipal);

  // document URI of the document this storage is bound to
  nsString mDocumentURI;

  // true if this storage was initialized as a localStorage object.  localStorage
  // objects are scoped to scheme/host/port in the database, while globalStorage
  // objects are scoped just to host.  this flag also tells the manager to map
  // this storage also in mLocalStorages hash table.
  nsDOMStorageType mStorageType;

  friend class nsIDOMStorage2;
  nsPIDOMStorage* mSecurityChecker;
  nsPIDOMStorage* mEventBroadcaster;
};

class nsDOMStorage2 : public nsIDOMStorage,
                      public nsPIDOMStorage
{
public:
  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorage2, nsIDOMStorage)

  nsDOMStorage2(nsDOMStorage2& aThat);
  nsDOMStorage2();

  NS_DECL_NSIDOMSTORAGE

  // nsPIDOMStorage
  virtual nsresult InitAsSessionStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI);
  virtual nsresult InitAsLocalStorage(nsIPrincipal *aPrincipal, const nsSubstring &aDocumentURI);
  virtual nsresult InitAsGlobalStorage(const nsACString &aDomainDemanded);
  virtual already_AddRefed<nsIDOMStorage> Clone();
  virtual already_AddRefed<nsIDOMStorage> Fork(const nsSubstring &aDocumentURI);
  virtual PRBool IsForkOf(nsIDOMStorage* aThat);
  virtual nsTArray<nsString> *GetKeys();
  virtual nsIPrincipal* Principal();
  virtual PRBool CanAccess(nsIPrincipal *aPrincipal);
  virtual nsDOMStorageType StorageType();
  virtual void BroadcastChangeNotification(const nsSubstring &aKey,
                                           const nsSubstring &aOldValue,
                                           const nsSubstring &aNewValue);

  nsresult InitAsSessionStorageFork(nsIPrincipal *aPrincipal,
                                    const nsSubstring &aDocumentURI,
                                    nsIDOMStorageObsolete* aStorage);

private:
  // storages bound to an origin hold the principal to
  // make security checks against it
  nsCOMPtr<nsIPrincipal> mPrincipal;

  // Needed for the storage event, this is address of the document this storage
  // is bound to
  nsString mDocumentURI;
  nsRefPtr<nsDOMStorage> mStorage;
};

class nsDOMStorageList : public nsIDOMStorageList
{
public:
  nsDOMStorageList()
  {
    mStorages.Init();
  }

  virtual ~nsDOMStorageList() {}

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMStorageList
  NS_DECL_NSIDOMSTORAGELIST

  nsIDOMStorageObsolete* GetNamedItem(const nsAString& aDomain, nsresult* aResult);

  /**
   * Check whether aCurrentDomain has access to aRequestedDomain
   */
  static PRBool
  CanAccessDomain(const nsACString& aRequestedDomain,
                  const nsACString& aCurrentDomain);

protected:

  /**
   * Return the global nsIDOMStorageObsolete for a particular domain.
   * aNoCurrentDomainCheck may be true to skip the domain comparison;
   * this is used for chrome code so that it may retrieve data from
   * any domain.
   *
   * @param aRequestedDomain domain to return
   * @param aCurrentDomain domain of current caller
   * @param aNoCurrentDomainCheck true to skip domain comparison
   */
  nsIDOMStorageObsolete*
  GetStorageForDomain(const nsACString& aRequestedDomain,
                      const nsACString& aCurrentDomain,
                      PRBool aNoCurrentDomainCheck,
                      nsresult* aResult);

  /**
   * Convert the domain into an array of its component parts.
   */
  static PRBool
  ConvertDomainToArray(const nsACString& aDomain,
                       nsTArray<nsCString>* aArray);

  nsInterfaceHashtable<nsCStringHashKey, nsIDOMStorageObsolete> mStorages;
};

class nsDOMStorageItem : public nsIDOMStorageItem,
                         public nsIDOMToString
{
public:
  nsDOMStorageItem(DOMStorageBase* aStorage,
                   const nsAString& aKey,
                   const nsAString& aValue,
                   PRBool aSecure);
  virtual ~nsDOMStorageItem();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsDOMStorageItem, nsIDOMStorageItem)

  // nsIDOMStorageObsolete
  NS_DECL_NSIDOMSTORAGEITEM

  // nsIDOMToString
  NS_DECL_NSIDOMTOSTRING

  PRBool IsSecure()
  {
    return mSecure;
  }

  void SetSecureInternal(PRBool aSecure)
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
  PRBool mSecure;

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

protected:
  nsString mKey;
  nsString mOldValue;
  nsString mNewValue;
  nsString mUrl;
  nsCOMPtr<nsIDOMStorage> mStorageArea;
};

class nsDOMStorageEventObsolete : public nsDOMEvent,
                          public nsIDOMStorageEventObsolete
{
public:
  nsDOMStorageEventObsolete()
    : nsDOMEvent(nsnull, nsnull)
  {
  }

  virtual ~nsDOMStorageEventObsolete()
  {
  }

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEEVENTOBSOLETE
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

protected:
  nsString mDomain;
};

nsresult
NS_NewDOMStorage(nsISupports* aOuter, REFNSIID aIID, void** aResult);

nsresult
NS_NewDOMStorage2(nsISupports* aOuter, REFNSIID aIID, void** aResult);

nsresult
NS_NewDOMStorageList(nsIDOMStorageList** aResult);

PRUint32
GetOfflinePermission(const nsACString &aDomain);

PRBool
IsOfflineAllowed(const nsACString &aDomain);

#endif /* nsDOMStorage_h___ */
