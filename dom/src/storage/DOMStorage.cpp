/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMStorage.h"
#include "DOMStorageCache.h"
#include "DOMStorageManager.h"

#include "nsIDOMStorageEvent.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsICookiePermission.h"

#include "nsDOMClassInfoID.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "GeneratedEvents.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"

DOMCI_DATA(Storage, mozilla::dom::DOMStorage)

namespace mozilla {
namespace dom {

NS_IMPL_ADDREF(DOMStorage)
NS_IMPL_RELEASE(DOMStorage)

NS_INTERFACE_MAP_BEGIN(DOMStorage)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsPIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(Storage)
NS_INTERFACE_MAP_END

DOMStorage::DOMStorage(DOMStorageManager* aManager,
                       DOMStorageCache* aCache,
                       const nsAString& aDocumentURI,
                       nsIPrincipal* aPrincipal,
                       bool aIsPrivate)
: mManager(aManager)
, mCache(aCache)
, mDocumentURI(aDocumentURI)
, mPrincipal(aPrincipal)
, mIsPrivate(aIsPrivate)
, mIsSessionOnly(false)
{
  mCache->Preload();
}

DOMStorage::~DOMStorage()
{
  mCache->KeepAlive();
}

// nsIDOMStorage (web content public API implementation)

NS_IMETHODIMP
DOMStorage::GetLength(uint32_t* aLength)
{
  if (!CanUseStorage(this)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return mCache->GetLength(this, aLength);
}

NS_IMETHODIMP
DOMStorage::Key(uint32_t aIndex, nsAString& aRetval)
{
  if (!CanUseStorage(this)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return mCache->GetKey(this, aIndex, aRetval);
}

NS_IMETHODIMP
DOMStorage::GetItem(const nsAString& aKey, nsAString& aRetval)
{
  if (!CanUseStorage(this)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  return mCache->GetItem(this, aKey, aRetval);
}

NS_IMETHODIMP
DOMStorage::SetItem(const nsAString& aKey, const nsAString& aData)
{
  if (!CanUseStorage(this)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  Telemetry::Accumulate(GetType() == LocalStorage
      ? Telemetry::LOCALDOMSTORAGE_KEY_SIZE_BYTES
      : Telemetry::SESSIONDOMSTORAGE_KEY_SIZE_BYTES, aKey.Length());
  Telemetry::Accumulate(GetType() == LocalStorage
      ? Telemetry::LOCALDOMSTORAGE_VALUE_SIZE_BYTES
      : Telemetry::SESSIONDOMSTORAGE_VALUE_SIZE_BYTES, aData.Length());

  nsString old;
  nsresult rv = mCache->SetItem(this, aKey, nsString(aData), old);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (rv != NS_SUCCESS_DOM_NO_OPERATION) {
    BroadcastChangeNotification(aKey, old, aData);
  }

  return NS_OK;
}

NS_IMETHODIMP
DOMStorage::RemoveItem(const nsAString& aKey)
{
  if (!CanUseStorage(this)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsAutoString old;
  nsresult rv = mCache->RemoveItem(this, aKey, old);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (rv != NS_SUCCESS_DOM_NO_OPERATION) {
    BroadcastChangeNotification(aKey, old, NullString());
  }

  return NS_OK;
}

NS_IMETHODIMP
DOMStorage::Clear()
{
  if (!CanUseStorage(this)) {
    return NS_ERROR_DOM_SECURITY_ERR;
  }

  nsresult rv = mCache->Clear(this);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (rv != NS_SUCCESS_DOM_NO_OPERATION) {
    BroadcastChangeNotification(NullString(), NullString(), NullString());
  }

  return NS_OK;
}

namespace {

class StorageNotifierRunnable : public nsRunnable
{
public:
  StorageNotifierRunnable(nsISupports* aSubject)
    : mSubject(aSubject)
  { }

  NS_DECL_NSIRUNNABLE

private:
  nsCOMPtr<nsISupports> mSubject;
};

NS_IMETHODIMP
StorageNotifierRunnable::Run()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(mSubject, "dom-storage2-changed", nullptr);
  }
  return NS_OK;
}

} // anonymous namespace

void
DOMStorage::BroadcastChangeNotification(const nsSubstring& aKey,
                                        const nsSubstring& aOldValue,
                                        const nsSubstring& aNewValue)
{
  nsCOMPtr<nsIDOMEvent> domEvent;
  // Note, this DOM event should never reach JS. It is cloned later in
  // nsGlobalWindow.
  NS_NewDOMStorageEvent(getter_AddRefs(domEvent), nullptr, nullptr, nullptr);

  nsCOMPtr<nsIDOMStorageEvent> event = do_QueryInterface(domEvent);
  nsresult rv = event->InitStorageEvent(NS_LITERAL_STRING("storage"),
                                        false,
                                        false,
                                        aKey,
                                        aOldValue,
                                        aNewValue,
                                        mDocumentURI,
                                        static_cast<nsIDOMStorage*>(this));
  if (NS_FAILED(rv)) {
    return;
  }

  nsRefPtr<StorageNotifierRunnable> r = new StorageNotifierRunnable(event);
  NS_DispatchToMainThread(r);
}

static const uint32_t ASK_BEFORE_ACCEPT = 1;
static const uint32_t ACCEPT_SESSION = 2;
static const uint32_t BEHAVIOR_REJECT = 2;

static const char kPermissionType[] = "cookie";
static const char kStorageEnabled[] = "dom.storage.enabled";
static const char kCookiesBehavior[] = "network.cookie.cookieBehavior";
static const char kCookiesLifetimePolicy[] = "network.cookie.lifetimePolicy";

// static, public
bool
DOMStorage::CanUseStorage(DOMStorage* aStorage)
{
  // This method is responsible for correct setting of mIsSessionOnly.
  // It doesn't work with mIsPrivate flag at all, since it is checked
  // regardless mIsSessionOnly flag in DOMStorageCache code.
  if (aStorage) {
    aStorage->mIsSessionOnly = false;
  }

  if (!mozilla::Preferences::GetBool(kStorageEnabled)) {
    return false;
  }

  // chrome can always use aStorage regardless of permission preferences
  if (nsContentUtils::IsCallerChrome()) {
    return true;
  }

  nsCOMPtr<nsIPrincipal> subjectPrincipal;
  nsresult rv = nsContentUtils::GetSecurityManager()->
                  GetSubjectPrincipal(getter_AddRefs(subjectPrincipal));
  NS_ENSURE_SUCCESS(rv, false);

  // if subjectPrincipal were null we'd have returned after
  // IsCallerChrome().

  nsCOMPtr<nsIPermissionManager> permissionManager =
    do_GetService(NS_PERMISSIONMANAGER_CONTRACTID);
  if (!permissionManager) {
    return false;
  }

  uint32_t perm;
  rv = permissionManager->TestPermissionFromPrincipal(subjectPrincipal,
                                                      kPermissionType, &perm);
  NS_ENSURE_SUCCESS(rv, false);

  if (perm == nsIPermissionManager::DENY_ACTION) {
    return false;
  }

  if (perm == nsICookiePermission::ACCESS_SESSION) {
    if (aStorage) {
      aStorage->mIsSessionOnly = true;
    }
  } else if (perm != nsIPermissionManager::ALLOW_ACTION) {
    uint32_t cookieBehavior = Preferences::GetUint(kCookiesBehavior);
    uint32_t lifetimePolicy = Preferences::GetUint(kCookiesLifetimePolicy);

    // Treat "ask every time" as "reject always".
    if ((cookieBehavior == BEHAVIOR_REJECT || lifetimePolicy == ASK_BEFORE_ACCEPT)) {
      return false;
    }

    if (lifetimePolicy == ACCEPT_SESSION && aStorage) {
      aStorage->mIsSessionOnly = true;
    }
  }

  if (aStorage) {
    return aStorage->CanAccess(subjectPrincipal);
  }

  return true;
}

// nsPIDOMStorage

nsPIDOMStorage::StorageType
DOMStorage::GetType() const
{
  return mManager->Type();
}

nsIPrincipal*
DOMStorage::GetPrincipal()
{
  return mPrincipal;
}

// Defined in DOMStorageManager.cpp
extern bool
PrincipalsEqual(nsIPrincipal* aObjectPrincipal, nsIPrincipal* aSubjectPrincipal);

bool
DOMStorage::PrincipalEquals(nsIPrincipal* aPrincipal)
{
  return PrincipalsEqual(mPrincipal, aPrincipal);
}

bool
DOMStorage::CanAccess(nsIPrincipal* aPrincipal)
{
  // Allow C++ callers to access the storage
  if (!aPrincipal) {
    return true;
  }

  // For content, either the code base or domain must be the same.  When code
  // base is the same, this is enough to say it is safe for a page to access
  // this storage.

  bool subsumes;
  nsresult rv = aPrincipal->SubsumesIgnoringDomain(mPrincipal, &subsumes);
  if (NS_FAILED(rv)) {
    return false;
  }

  if (!subsumes) {
    nsresult rv = aPrincipal->Subsumes(mPrincipal, &subsumes);
    if (NS_FAILED(rv)) {
      return false;
    }
  }

  return subsumes;
}

nsTArray<nsString>*
DOMStorage::GetKeys()
{
  if (!CanUseStorage(this)) {
    return new nsTArray<nsString>(); // return just an empty array
  }

  return mCache->GetKeys(this);
}

} // ::dom
} // ::mozilla
