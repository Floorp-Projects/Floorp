/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LocalStorage.h"
#include "LocalStorageCache.h"
#include "LocalStorageManager.h"
#include "StorageUtils.h"

#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsICookiePermission.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/PermissionMessageUtils.h"
#include "mozilla/dom/StorageBinding.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "mozilla/EnumSet.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {

using namespace ipc;

namespace dom {

NS_IMPL_CYCLE_COLLECTION_INHERITED(LocalStorage, Storage, mManager);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(LocalStorage)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_INHERITING(Storage)

NS_IMPL_ADDREF_INHERITED(LocalStorage, Storage)
NS_IMPL_RELEASE_INHERITED(LocalStorage, Storage)

LocalStorage::LocalStorage(nsPIDOMWindowInner* aWindow,
                           LocalStorageManager* aManager,
                           LocalStorageCache* aCache,
                           const nsAString& aDocumentURI,
                           nsIPrincipal* aPrincipal,
                           bool aIsPrivate)
  : Storage(aWindow, aPrincipal)
  , mManager(aManager)
  , mCache(aCache)
  , mDocumentURI(aDocumentURI)
  , mIsPrivate(aIsPrivate)
{
  mCache->Preload();
}

LocalStorage::~LocalStorage()
{
}

int64_t
LocalStorage::GetOriginQuotaUsage() const
{
  return mCache->GetOriginQuotaUsage(this);
}

uint32_t
LocalStorage::GetLength(nsIPrincipal& aSubjectPrincipal,
                        ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  uint32_t length;
  aRv = mCache->GetLength(this, &length);
  return length;
}

void
LocalStorage::Key(uint32_t aIndex, nsAString& aResult,
                  nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->GetKey(this, aIndex, aResult);
}

void
LocalStorage::GetItem(const nsAString& aKey, nsAString& aResult,
                      nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->GetItem(this, aKey, aResult);
}

void
LocalStorage::SetItem(const nsAString& aKey, const nsAString& aData,
                      nsIPrincipal& aSubjectPrincipal,
                      ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsString data;
  bool ok = data.Assign(aData, fallible);
  if (!ok) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsString old;
  aRv = mCache->SetItem(this, aKey, data, old);
  if (aRv.Failed()) {
    return;
  }

  if (!aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION)) {
    BroadcastChangeNotification(aKey, old, aData);
  }
}

void
LocalStorage::RemoveItem(const nsAString& aKey, nsIPrincipal& aSubjectPrincipal,
                         ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsAutoString old;
  aRv = mCache->RemoveItem(this, aKey, old);
  if (aRv.Failed()) {
    return;
  }

  if (!aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION)) {
    BroadcastChangeNotification(aKey, old, NullString());
  }
}

void
LocalStorage::Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv)
{
  if (!CanUseStorage(aSubjectPrincipal)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->Clear(this);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (!aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION)) {
    BroadcastChangeNotification(NullString(), NullString(), NullString());
  }
}

void
LocalStorage::BroadcastChangeNotification(const nsAString& aKey,
                                          const nsAString& aOldValue,
                                          const nsAString& aNewValue)
{
  if (!XRE_IsParentProcess() && Principal()) {
    // If we are in a child process, we want to send a message to the parent in
    // order to broadcast the StorageEvent correctly to any child process.
    dom::ContentChild* cc = dom::ContentChild::GetSingleton();
    Unused << NS_WARN_IF(!cc->SendBroadcastLocalStorageChange(
      mDocumentURI, nsString(aKey), nsString(aOldValue), nsString(aNewValue),
      IPC::Principal(Principal()), mIsPrivate));
  }

  DispatchStorageEvent(mDocumentURI, aKey, aOldValue, aNewValue,
                       Principal(), mIsPrivate, this, false);
}

/* static */ void
LocalStorage::DispatchStorageEvent(const nsAString& aDocumentURI,
                                   const nsAString& aKey,
                                   const nsAString& aOldValue,
                                   const nsAString& aNewValue,
                                   nsIPrincipal* aPrincipal,
                                   bool aIsPrivate,
                                   Storage* aStorage,
                                   bool aImmediateDispatch)
{
  NotifyChange(aStorage, aPrincipal, aKey, aOldValue, aNewValue,
               u"localStorage", aDocumentURI, aIsPrivate, aImmediateDispatch);

  // If we are in the parent process and we have the principal, we want to
  // broadcast this event to every other process.
  if (XRE_IsParentProcess() && aPrincipal) {
    for (auto* cp : ContentParent::AllProcesses(ContentParent::eLive)) {
      Unused << cp->SendDispatchLocalStorageChange(
        nsString(aDocumentURI), nsString(aKey), nsString(aOldValue),
        nsString(aNewValue), IPC::Principal(aPrincipal), aIsPrivate);
    }
  }
}

void
LocalStorage::ApplyEvent(StorageEvent* aStorageEvent)
{
  MOZ_ASSERT(aStorageEvent);

  nsAutoString key;
  nsAutoString old;
  nsAutoString value;

  aStorageEvent->GetKey(key);
  aStorageEvent->GetNewValue(value);

  // No key means clearing the full storage.
  if (key.IsVoid()) {
    MOZ_ASSERT(value.IsVoid());
    mCache->Clear(this, LocalStorageCache::E10sPropagated);
    return;
  }

  // No new value means removing the key.
  if (value.IsVoid()) {
    mCache->RemoveItem(this, key, old, LocalStorageCache::E10sPropagated);
    return;
  }

  // Otherwise, we set the new value.
  mCache->SetItem(this, key, value, old, LocalStorageCache::E10sPropagated);
}

static const char kPermissionType[] = "cookie";
static const char kStorageEnabled[] = "dom.storage.enabled";

bool
LocalStorage::PrincipalEquals(nsIPrincipal* aPrincipal)
{
  return StorageUtils::PrincipalsEqual(mPrincipal, aPrincipal);
}

void
LocalStorage::GetSupportedNames(nsTArray<nsString>& aKeys)
{
  if (!CanUseStorage(*nsContentUtils::SubjectPrincipal())) {
    // return just an empty array
    aKeys.Clear();
    return;
  }

  mCache->GetKeys(this, aKeys);
}

bool
LocalStorage::IsForkOf(const Storage* aOther) const
{
  MOZ_ASSERT(aOther);
  if (aOther->Type() != eLocalStorage) {
    return false;
  }

  return mCache == static_cast<const LocalStorage*>(aOther)->mCache;
}

} // namespace dom
} // namespace mozilla
