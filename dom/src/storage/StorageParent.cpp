/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* vim: set sw=4 ts=8 et tw=80 ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "StorageParent.h"
#include "mozilla/dom/PContentParent.h"
#include "mozilla/unused.h"
#include "nsDOMString.h"

using mozilla::unused;

namespace mozilla {
namespace dom {

StorageParent::StorageParent(const StorageConstructData& aData)
{
  if (aData.type() == StorageConstructData::Tnull_t) {
    mStorage = new DOMStorageImpl(nullptr);
  } else {
    const StorageClone& clone = aData.get_StorageClone();
    StorageParent* other = static_cast<StorageParent*>(clone.actorParent());
    mStorage = new DOMStorageImpl(nullptr, *other->mStorage.get());
    mStorage->CloneFrom(clone.callerSecure(), other->mStorage);
  }
}

bool
StorageParent::RecvInit(const bool& aUseDB,
                        const bool& aCanUseChromePersist,
                        const bool& aSessionOnly,
                        const bool& aPrivate,
                        const nsCString& aDomain,
                        const nsCString& aScopeDBKey,
                        const nsCString& aQuotaDomainDBKey,
                        const nsCString& aQuotaETLDplus1DomainDBKey,
                        const PRUint32& aStorageType)
{
  mStorage->InitFromChild(aUseDB, aCanUseChromePersist, aSessionOnly, aPrivate, aDomain,
                          aScopeDBKey, aQuotaDomainDBKey, aQuotaETLDplus1DomainDBKey,
                          aStorageType);
  return true;
}

bool
StorageParent::RecvUpdatePrivateState(const bool& aEnabled)
{
  mStorage->PrivateModeChanged(aEnabled);
  return true;
}

bool
StorageParent::RecvGetKeys(const bool& aCallerSecure, InfallibleTArray<nsString>* aKeys)
{
  // Callers are responsible for deallocating the array returned by mStorage->GetKeys
  nsAutoPtr<nsTArray<nsString> > keys(mStorage->GetKeys(aCallerSecure));
  aKeys->SwapElements(*keys);
  return true;
}

bool
StorageParent::RecvGetLength(const bool& aCallerSecure, const bool& aSessionOnly,
                             PRUint32* aLength, nsresult* rv)
{
  mStorage->SetSessionOnly(aSessionOnly);
  *rv = mStorage->GetLength(aCallerSecure, aLength);
  return true;
}

bool
StorageParent::RecvGetKey(const bool& aCallerSecure, const bool& aSessionOnly,
                          const PRUint32& aIndex, nsString* aKey, nsresult* rv)
{
  mStorage->SetSessionOnly(aSessionOnly);
  *rv = mStorage->GetKey(aCallerSecure, aIndex, *aKey);
  return true;
}

bool
StorageParent::RecvGetValue(const bool& aCallerSecure, const bool& aSessionOnly,
                            const nsString& aKey, StorageItem* aItem,
                            nsresult* rv)
{
  mStorage->SetSessionOnly(aSessionOnly);

  // We need to ensure that a proper null representation is sent to the child
  // if no item is found or an error occurs.

  *rv = NS_OK;
  nsCOMPtr<nsIDOMStorageItem> item = mStorage->GetValue(aCallerSecure, aKey, rv);
  if (NS_FAILED(*rv) || !item) {
    *aItem = null_t();
    return true;
  }

  ItemData data(EmptyString(), false);
  nsDOMStorageItem* internalItem = static_cast<nsDOMStorageItem*>(item.get());
  data.value() = internalItem->GetValueInternal();
  if (aCallerSecure)
    data.secure() = internalItem->IsSecure();
  *aItem = data;
  return true;
}

bool
StorageParent::RecvSetValue(const bool& aCallerSecure, const bool& aSessionOnly,
                            const nsString& aKey, const nsString& aData,
                            nsString* aOldValue, nsresult* rv)
{
  mStorage->SetSessionOnly(aSessionOnly);
  *rv = mStorage->SetValue(aCallerSecure, aKey, aData, *aOldValue);
  return true;
}

bool
StorageParent::RecvRemoveValue(const bool& aCallerSecure, const bool& aSessionOnly,
                               const nsString& aKey, nsString* aOldValue,
                               nsresult* rv)
{
  mStorage->SetSessionOnly(aSessionOnly);
  *rv = mStorage->RemoveValue(aCallerSecure, aKey, *aOldValue);
  return true;
}

bool
StorageParent::RecvClear(const bool& aCallerSecure, const bool& aSessionOnly,
                         PRInt32* aOldCount, nsresult* rv)
{
  mStorage->SetSessionOnly(aSessionOnly);
  *rv = mStorage->Clear(aCallerSecure, aOldCount);
  return true;
}

bool
StorageParent::RecvGetDBValue(const nsString& aKey, nsString* aValue,
                              bool* aSecure, nsresult* rv)
{
  *rv = mStorage->GetDBValue(aKey, *aValue, aSecure);
  return true;
}

bool
StorageParent::RecvSetDBValue(const nsString& aKey, const nsString& aValue,
                              const bool& aSecure, nsresult* rv)
{
  *rv = mStorage->SetDBValue(aKey, aValue, aSecure);
  return true;
}

bool
StorageParent::RecvSetSecure(const nsString& aKey, const bool& aSecure,
                             nsresult* rv)
{
  *rv = mStorage->SetSecure(aKey, aSecure);
  return true;
}

}
}
