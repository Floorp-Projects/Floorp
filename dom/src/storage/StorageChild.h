/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_StorageChild_h
#define mozilla_dom_StorageChild_h

#include "mozilla/dom/PStorageChild.h"
#include "nsDOMStorage.h"
#include "nsCycleCollectionParticipant.h"

namespace mozilla {
namespace dom {

class StorageChild : public PStorageChild
                   , public DOMStorageBase
{
public:
  NS_DECL_CYCLE_COLLECTION_CLASS(StorageChild)
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  
  StorageChild(nsDOMStorage* aOwner);
  StorageChild(nsDOMStorage* aOwner, StorageChild& aOther);

  virtual void InitAsSessionStorage(nsIURI* aDomainURI);
  virtual void InitAsLocalStorage(nsIURI* aDomainURI, bool aCanUseChromePersist);

  virtual bool CacheStoragePermissions();
  
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

  virtual bool CanUseChromePersist();

  virtual nsresult GetDBValue(const nsAString& aKey,
                              nsAString& aValue,
                              bool* aSecure);
  virtual nsresult SetDBValue(const nsAString& aKey,
                              const nsAString& aValue,
                              bool aSecure);
  virtual nsresult SetSecure(const nsAString& aKey, bool aSecure);

  virtual nsresult CloneFrom(bool aCallerSecure, DOMStorageBase* aThat);

  void AddIPDLReference();
  void ReleaseIPDLReference();

private:
  void InitRemote();

  // Unimplemented
  StorageChild(const StorageChild&);

  nsCOMPtr<nsIDOMStorageObsolete> mStorage;
  bool mIPCOpen;
};

}
}

#endif
