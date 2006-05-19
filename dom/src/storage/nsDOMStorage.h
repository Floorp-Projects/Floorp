/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsIDOMStorage.h"
#include "nsIDOMStorageList.h"
#include "nsIDOMStorageItem.h"
#include "nsInterfaceHashtable.h"
#include "nsVoidArray.h"
#include "nsPIDOMStorage.h"
#include "nsIDOMToString.h"
#include "nsDOMEvent.h"
#include "nsIDOMStorageEvent.h"

#ifdef MOZ_STORAGE
#include "nsDOMStorageDB.h"
#endif

class nsDOMStorageItem;

class nsSessionStorageEntry : public nsStringHashKey
{
public:
  nsSessionStorageEntry(KeyTypePointer aStr);
  nsSessionStorageEntry(const nsSessionStorageEntry& aToCopy);
  ~nsSessionStorageEntry();

  nsRefPtr<nsDOMStorageItem> mItem;
};

class nsDOMStorage : public nsIDOMStorage,
                     public nsPIDOMStorage
{
public:
  nsDOMStorage();
  nsDOMStorage(const nsAString& aDomain, PRBool aUseDB);
  virtual ~nsDOMStorage();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMStorage
  NS_DECL_NSIDOMSTORAGE

  // nsPIDOMStorage
  virtual void Init(const nsAString& aDomain, PRBool aUseDB);
  virtual already_AddRefed<nsIDOMStorage> Clone();
  virtual nsTArray<nsString> *GetKeys();

  // cache the keys from the database for faster lookup
  nsresult
  CacheKeysFromDB();

  // retrieve the value and secure state corresponding to a key out of storage.
  nsresult
  GetDBValue(const nsAString& aKey, nsAString& aValue, PRBool* aSecure);

  // set the value corresponding to a key in the storage. If
  // aSecure is false, then attempts to modify a secure value
  // throw NS_ERROR_DOM_INVALID_ACCESS_ERR
  nsresult
  SetDBValue(const nsAString& aKey,
             const nsAString& aValue,
             PRBool aSecure);

  // set the value corresponding to a key as secure.
  nsresult
  SetSecure(const nsAString& aKey, PRBool aSecure);

protected:

  nsresult InitDB();

  void BroadcastChangeNotification();

  // true if the storage database should be used for values
  PRBool mUseDB;

  // true if items from the database are cached
  PRBool mItemsCached;

  // domain this store is associated with
  nsAutoString mDomain;

  // the key->value item pairs
  nsTHashtable<nsSessionStorageEntry> mItems;

#ifdef MOZ_STORAGE
  static nsDOMStorageDB* gStorageDB;
#endif
};

class nsDOMStorageList : public nsIDOMStorageList
{
public:
  nsDOMStorageList()
  {
    mStorages.Init();
  };

  virtual ~nsDOMStorageList() {};

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMStorageList
  NS_DECL_NSIDOMSTORAGELIST

  /**
   * Return the global nsIDOMStorage for a particular domain.
   * aNoCurrentDomainCheck may be true to skip the domain comparison;
   * this is used for chrome code so that it may retrieve data from
   * any domain.
   *
   * @param aRequestedDomain domain to return
   * @param aCurrentDomain domain of current caller
   * @param aNoCurrentDomainCheck true to skip domain comparison
   */
  nsresult
  GetStorageForDomain(const nsAString& aRequestedDomain,
                      const nsAString& aCurrentDomain,
                      PRBool aNoCurrentDomainCheck,
                      nsIDOMStorage** aStorage);

  /**
   * Convert the domain into an array of its component parts.
   */
  static PRBool
  ConvertDomainToArray(const nsAString& aDomain,
                       nsStringArray* aArray);

  /**
   * Check whether aCurrentDomain has access to aRequestedDomain
   */
  static PRBool
  CanAccessDomain(const nsAString& aRequestedDomain,
                  const nsAString& aCurrentDomain);

protected:

  nsInterfaceHashtable<nsStringHashKey, nsIDOMStorage> mStorages;
};

class nsDOMStorageItem : public nsIDOMStorageItem,
                         public nsIDOMToString
{
public:
  nsDOMStorageItem(nsDOMStorage* aStorage,
                   const nsAString& aKey,
                   PRBool aSecure);
  virtual ~nsDOMStorageItem();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMStorage
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
    NS_ASSERTION(!mStorage, "Don't call this on global storage items!");

    return mKeyOrValue;
  }

  void ClearValue()
  {
    NS_ASSERTION(!mStorage, "Don't call this on global storage items!");

    mKeyOrValue.Truncate();
  }

protected:

  // true if this value is for secure sites only
  PRBool mSecure;

  // value of the item, or key for the item if it came from the db.
  nsString mKeyOrValue;

  // If this item came from the db, mStorage points to the storage
  // object where this item came from.
  nsRefPtr<nsDOMStorage> mStorage;
};

class nsDOMStorageEvent : public nsDOMEvent,
                          public nsIDOMStorageEvent
{
public:
  nsDOMStorageEvent(const nsAString& aDomain)
    : nsDOMEvent(nsnull, nsnull), mDomain(aDomain)
  {
    if (aDomain.IsEmpty()) {
      // An empty domain means this event is for a session sotrage
      // object change. Store #session as the domain.

      mDomain = NS_LITERAL_STRING("#session");
    }
  }

  virtual ~nsDOMStorageEvent()
  {
  }

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMSTORAGEEVENT
  NS_FORWARD_NSIDOMEVENT(nsDOMEvent::)

protected:
  nsString mDomain;
};

NS_IMETHODIMP
NS_NewDOMStorage(nsISupports* aOuter, REFNSIID aIID, void** aResult);

nsresult
NS_NewDOMStorageList(nsIDOMStorageList** aResult);

#endif /* nsDOMStorage_h___ */
