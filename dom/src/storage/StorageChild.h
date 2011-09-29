/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
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
 * The Original Code is Mozilla DOM Storage.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
  virtual void InitAsGlobalStorage(const nsACString& aDomainDemanded);

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
