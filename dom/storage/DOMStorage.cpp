/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMStorage.h"
#include "DOMStorageCache.h"
#include "DOMStorageManager.h"

#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsICookiePermission.h"

#include "mozilla/dom/StorageBinding.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/Services.h"
#include "mozilla/Preferences.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMStorage, mManager, mPrincipal, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMStorage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMStorage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMStorage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

DOMStorage::DOMStorage(nsIDOMWindow* aWindow,
                       DOMStorageManager* aManager,
                       DOMStorageCache* aCache,
                       const nsAString& aDocumentURI,
                       nsIPrincipal* aPrincipal,
                       bool aIsPrivate)
: mWindow(aWindow)
, mManager(aManager)
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

/* virtual */ JSObject*
DOMStorage::WrapObject(JSContext* aCx)
{
  return StorageBinding::Wrap(aCx, this);
}

uint32_t
DOMStorage::GetLength(ErrorResult& aRv)
{
  if (!CanUseStorage(this)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return 0;
  }

  uint32_t length;
  aRv = mCache->GetLength(this, &length);
  return length;
}

void
DOMStorage::Key(uint32_t aIndex, nsAString& aResult, ErrorResult& aRv)
{
  if (!CanUseStorage(this)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->GetKey(this, aIndex, aResult);
}

void
DOMStorage::GetItem(const nsAString& aKey, nsAString& aResult, ErrorResult& aRv)
{
  if (!CanUseStorage(this)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->GetItem(this, aKey, aResult);
}

void
DOMStorage::SetItem(const nsAString& aKey, const nsAString& aData,
                    ErrorResult& aRv)
{
  if (!CanUseStorage(this)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  Telemetry::Accumulate(GetType() == LocalStorage
      ? Telemetry::LOCALDOMSTORAGE_KEY_SIZE_BYTES
      : Telemetry::SESSIONDOMSTORAGE_KEY_SIZE_BYTES, aKey.Length());
  Telemetry::Accumulate(GetType() == LocalStorage
      ? Telemetry::LOCALDOMSTORAGE_VALUE_SIZE_BYTES
      : Telemetry::SESSIONDOMSTORAGE_VALUE_SIZE_BYTES, aData.Length());

  nsString data;
  bool ok = data.Assign(aData, fallible_t());
  if (!ok) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  nsString old;
  aRv = mCache->SetItem(this, aKey, data, old);
  if (aRv.Failed()) {
    return;
  }

  if (aRv.ErrorCode() != NS_SUCCESS_DOM_NO_OPERATION) {
    BroadcastChangeNotification(aKey, old, aData);
  }
}

void
DOMStorage::RemoveItem(const nsAString& aKey, ErrorResult& aRv)
{
  if (!CanUseStorage(this)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  nsAutoString old;
  aRv = mCache->RemoveItem(this, aKey, old);
  if (aRv.Failed()) {
    return;
  }

  if (aRv.ErrorCode() != NS_SUCCESS_DOM_NO_OPERATION) {
    BroadcastChangeNotification(aKey, old, NullString());
  }
}

void
DOMStorage::Clear(ErrorResult& aRv)
{
  if (!CanUseStorage(this)) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return;
  }

  aRv = mCache->Clear(this);
  if (NS_WARN_IF(aRv.Failed())) {
    return;
  }

  if (aRv.ErrorCode() != NS_SUCCESS_DOM_NO_OPERATION) {
    BroadcastChangeNotification(NullString(), NullString(), NullString());
  }
}

namespace {

class StorageNotifierRunnable : public nsRunnable
{
public:
  StorageNotifierRunnable(nsISupports* aSubject, const char16_t* aType)
    : mSubject(aSubject), mType(aType)
  { }

  NS_DECL_NSIRUNNABLE

private:
  nsCOMPtr<nsISupports> mSubject;
  const char16_t* mType;
};

NS_IMETHODIMP
StorageNotifierRunnable::Run()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(mSubject, "dom-storage2-changed", mType);
  }
  return NS_OK;
}

} // anonymous namespace

void
DOMStorage::BroadcastChangeNotification(const nsSubstring& aKey,
                                        const nsSubstring& aOldValue,
                                        const nsSubstring& aNewValue)
{
  StorageEventInit dict;
  dict.mBubbles = false;
  dict.mCancelable = false;
  dict.mKey = aKey;
  dict.mNewValue = aNewValue;
  dict.mOldValue = aOldValue;
  dict.mStorageArea = this;
  dict.mUrl = mDocumentURI;

  // Note, this DOM event should never reach JS. It is cloned later in
  // nsGlobalWindow.
  nsRefPtr<StorageEvent> event =
    StorageEvent::Constructor(nullptr, NS_LITERAL_STRING("storage"), dict);

  nsRefPtr<StorageNotifierRunnable> r =
    new StorageNotifierRunnable(event,
                                GetType() == LocalStorage
                                  ? MOZ_UTF16("localStorage")
                                  : MOZ_UTF16("sessionStorage"));
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
  nsCOMPtr<nsIPrincipal> subjectPrincipal =
    nsContentUtils::SubjectPrincipal();
  if (nsContentUtils::IsSystemPrincipal(subjectPrincipal)) {
    return true;
  }

  nsCOMPtr<nsIPermissionManager> permissionManager =
    services::GetPermissionManager();
  if (!permissionManager) {
    return false;
  }

  uint32_t perm;
  permissionManager->TestPermissionFromPrincipal(subjectPrincipal,
                                                 kPermissionType, &perm);

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

DOMStorage::StorageType
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
  return !aPrincipal || aPrincipal->Subsumes(mPrincipal);
}

void
DOMStorage::GetSupportedNames(unsigned, nsTArray<nsString>& aKeys)
{
  if (!CanUseStorage(this)) {
    // return just an empty array
    aKeys.Clear();
    return;
  }

  mCache->GetKeys(this, aKeys);
}

} // ::dom
} // ::mozilla
