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

#include "StorageChild.h"
#include "mozilla/dom/ContentChild.h"
#include "nsDOMError.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_1(StorageChild, mStorage)

NS_IMPL_CYCLE_COLLECTING_ADDREF(StorageChild)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StorageChild)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMETHODIMP_(nsrefcnt) StorageChild::Release(void)
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(StorageChild);
  nsISupports* base = NS_CYCLE_COLLECTION_CLASSNAME(StorageChild)::Upcast(this);
  nsrefcnt count = mRefCnt.decr(base);
  NS_LOG_RELEASE(this, count, "StorageChild");
  if (count == 1 && mIPCOpen) {
    Send__delete__(this);
    return 0;
  }
  if (count == 0) {
    mRefCnt.stabilizeForDeletion(base);
    delete this;
    return 0;
  }
  return count;
}

StorageChild::StorageChild(nsDOMStorage* aOwner)
: mStorage(aOwner)
, mIPCOpen(false)
{
}

StorageChild::StorageChild(nsDOMStorage* aOwner, StorageChild& aOther)
: DOMStorageBase(aOther)
, mStorage(aOwner)
, mIPCOpen(false)
{
}

void
StorageChild::AddIPDLReference()
{
  NS_ABORT_IF_FALSE(!mIPCOpen, "Attempting to retain multiple IPDL references");
  mIPCOpen = true;
  AddRef();
}

void
StorageChild::ReleaseIPDLReference()
{
  NS_ABORT_IF_FALSE(mIPCOpen, "Attempting to release non-existent IPDL reference");
  mIPCOpen = false;
  Release();
}

bool
StorageChild::CacheStoragePermissions()
{
  nsDOMStorage* storage = static_cast<nsDOMStorage*>(mStorage.get());
  return storage->CacheStoragePermissions();
}

void
StorageChild::InitRemote()
{
  ContentChild* child = ContentChild::GetSingleton();
  AddIPDLReference();
  child->SendPStorageConstructor(this, null_t());
  SendInit(mUseDB, mCanUseChromePersist, mSessionOnly, mDomain, mScopeDBKey,
           mQuotaDomainDBKey, mQuotaETLDplus1DomainDBKey, mStorageType);
}

void
StorageChild::InitAsSessionStorage(nsIURI* aDomainURI)
{
  DOMStorageBase::InitAsSessionStorage(aDomainURI);
  InitRemote();
}

void
StorageChild::InitAsLocalStorage(nsIURI* aDomainURI, bool aCanUseChromePersist)
{
  DOMStorageBase::InitAsLocalStorage(aDomainURI, aCanUseChromePersist);
  InitRemote();
}

void
StorageChild::InitAsGlobalStorage(const nsACString& aDomainDemanded)
{
  DOMStorageBase::InitAsGlobalStorage(aDomainDemanded);
  InitRemote();
}

nsTArray<nsString>*
StorageChild::GetKeys(bool aCallerSecure)
{
  InfallibleTArray<nsString> remoteKeys;
  SendGetKeys(aCallerSecure, &remoteKeys);
  nsTArray<nsString>* keys = new nsTArray<nsString>;
  *keys = remoteKeys;
  return keys;
}

nsresult
StorageChild::GetLength(bool aCallerSecure, PRUint32* aLength)
{
  nsresult rv;
  SendGetLength(aCallerSecure, mSessionOnly, aLength, &rv);
  return rv;
}

nsresult
StorageChild::GetKey(bool aCallerSecure, PRUint32 aIndex, nsAString& aKey)
{
  nsresult rv;
  nsString key;
  SendGetKey(aCallerSecure, mSessionOnly, aIndex, &key, &rv);
  if (NS_FAILED(rv))
    return rv;
  aKey = key;
  return NS_OK;
}

// Unlike other cross-process forwarding methods, GetValue needs to replicate
// the following behaviour of DOMStorageImpl::GetValue:
//
// - if a security error occurs, or the item isn't found, return null without
//   propogating the error.
//
// If DOMStorageImpl::GetValue ever changes its behaviour, this should be kept
// in sync.
nsIDOMStorageItem*
StorageChild::GetValue(bool aCallerSecure, const nsAString& aKey, nsresult* rv)
{
  nsresult rv2 = *rv = NS_OK;
  StorageItem storageItem;
  SendGetValue(aCallerSecure, mSessionOnly, nsString(aKey), &storageItem, &rv2);
  if (rv2 == NS_ERROR_DOM_SECURITY_ERR || rv2 == NS_ERROR_DOM_NOT_FOUND_ERR)
    return nsnull;
  *rv = rv2;
  if (NS_FAILED(*rv) || storageItem.type() == StorageItem::Tnull_t)
    return nsnull;
  const ItemData& data = storageItem.get_ItemData();
  nsIDOMStorageItem* item = new nsDOMStorageItem(this, aKey, data.value(),
                                                 data.secure());
  return item;
}

nsresult
StorageChild::SetValue(bool aCallerSecure, const nsAString& aKey,
                       const nsAString& aData, nsAString& aOldData)
{
  nsresult rv;
  nsString oldData;
  SendSetValue(aCallerSecure, mSessionOnly, nsString(aKey), nsString(aData),
               &oldData, &rv);
  if (NS_FAILED(rv))
    return rv;
  aOldData = oldData;
  return NS_OK;
}

nsresult
StorageChild::RemoveValue(bool aCallerSecure, const nsAString& aKey,
                          nsAString& aOldData)
{
  nsresult rv;
  nsString oldData;
  SendRemoveValue(aCallerSecure, mSessionOnly, nsString(aKey), &oldData, &rv);
  if (NS_FAILED(rv))
    return rv;
  aOldData = oldData;
  return NS_OK;
}

nsresult
StorageChild::Clear(bool aCallerSecure, PRInt32* aOldCount)
{
  nsresult rv;
  PRInt32 oldCount;
  SendClear(aCallerSecure, mSessionOnly, &oldCount, &rv);
  if (NS_FAILED(rv))
    return rv;
  *aOldCount = oldCount;
  return NS_OK;
}

bool
StorageChild::CanUseChromePersist()
{
  return mCanUseChromePersist;
}

nsresult
StorageChild::GetDBValue(const nsAString& aKey, nsAString& aValue,
                         bool* aSecure)
{
  nsresult rv;
  nsString value;
  SendGetDBValue(nsString(aKey), &value, aSecure, &rv);
  aValue = value;
  return rv;
}

nsresult
StorageChild::SetDBValue(const nsAString& aKey,
                         const nsAString& aValue,
                         bool aSecure)
{
  nsresult rv;
  SendSetDBValue(nsString(aKey), nsString(aValue), aSecure, &rv);
  return rv;
}

nsresult
StorageChild::SetSecure(const nsAString& aKey, bool aSecure)
{
  nsresult rv;
  SendSetSecure(nsString(aKey), aSecure, &rv);
  return rv;
}

nsresult
StorageChild::CloneFrom(bool aCallerSecure, DOMStorageBase* aThat)
{
  StorageChild* other = static_cast<StorageChild*>(aThat);
  ContentChild* child = ContentChild::GetSingleton();
  StorageClone clone(nsnull, other, aCallerSecure);
  AddIPDLReference();
  child->SendPStorageConstructor(this, clone);
  SendInit(mUseDB, mCanUseChromePersist, mSessionOnly, mDomain, mScopeDBKey,
           mQuotaDomainDBKey, mQuotaETLDplus1DomainDBKey, mStorageType);
  return NS_OK;
}

}
}
