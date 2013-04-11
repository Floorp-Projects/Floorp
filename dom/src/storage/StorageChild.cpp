/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageChild.h"
#include "mozilla/dom/ContentChild.h"
#include "nsError.h"

#include "GeckoProfiler.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_1(StorageChild, mStorage)

NS_IMPL_CYCLE_COLLECTING_ADDREF(StorageChild)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(StorageChild)
  NS_INTERFACE_MAP_ENTRY(nsIPrivacyTransitionObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPrivacyTransitionObserver)
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
    mRefCnt.stabilizeForDeletion();
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
  SendInit(mUseDB, mSessionOnly, mInPrivateBrowsing, mScopeDBKey,
           mQuotaDBKey, mStorageType);
}

void
StorageChild::InitAsSessionStorage(nsIPrincipal* aPrincipal, bool aPrivate)
{
  DOMStorageBase::InitAsSessionStorage(aPrincipal, aPrivate);
  InitRemote();
}

void
StorageChild::InitAsLocalStorage(nsIPrincipal* aPrincipal, bool aPrivate)
{
  DOMStorageBase::InitAsLocalStorage(aPrincipal, aPrivate);
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
StorageChild::GetLength(bool aCallerSecure, uint32_t* aLength)
{
  nsresult rv;
  SendGetLength(aCallerSecure, mSessionOnly, aLength, &rv);
  return rv;
}

nsresult
StorageChild::GetKey(bool aCallerSecure, uint32_t aIndex, nsAString& aKey)
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
  PROFILER_LABEL("StorageChild", "GetValue");
  nsresult rv2 = *rv = NS_OK;
  StorageItem storageItem;
  SendGetValue(aCallerSecure, mSessionOnly, nsString(aKey), &storageItem, &rv2);
  if (rv2 == NS_ERROR_DOM_SECURITY_ERR || rv2 == NS_ERROR_DOM_NOT_FOUND_ERR)
    return nullptr;
  *rv = rv2;
  if (NS_FAILED(*rv) || storageItem.type() == StorageItem::Tnull_t)
    return nullptr;
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
StorageChild::Clear(bool aCallerSecure, int32_t* aOldCount)
{
  nsresult rv;
  int32_t oldCount;
  SendClear(aCallerSecure, mSessionOnly, &oldCount, &rv);
  if (NS_FAILED(rv))
    return rv;
  *aOldCount = oldCount;
  return NS_OK;
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
  StorageClone clone(nullptr, other, aCallerSecure);
  AddIPDLReference();
  child->SendPStorageConstructor(this, clone);
  SendInit(mUseDB, mSessionOnly, mInPrivateBrowsing,
           mScopeDBKey, mQuotaDBKey, mStorageType);
  return NS_OK;
}

NS_IMETHODIMP
StorageChild::PrivateModeChanged(bool enabled)
{
  mInPrivateBrowsing = enabled;
  SendUpdatePrivateState(enabled);
  return NS_OK;
}

}
}
