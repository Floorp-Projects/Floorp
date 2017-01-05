/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Storage.h"
#include "StorageCache.h"
#include "StorageManager.h"

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
#include "mozilla/EnumSet.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Storage, mManager, mPrincipal, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Storage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Storage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Storage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

Storage::Storage(nsPIDOMWindowInner* aWindow,
                 StorageManagerBase* aManager,
                 StorageCache* aCache,
                 const nsAString& aDocumentURI,
                 nsIPrincipal* aPrincipal)
  : mWindow(aWindow)
  , mManager(aManager)
  , mCache(aCache)
  , mDocumentURI(aDocumentURI)
  , mPrincipal(aPrincipal)
  , mIsSessionOnly(false)
{
  mCache->Preload();
}

Storage::~Storage()
{
  mCache->KeepAlive();
}

/* virtual */ JSObject*
Storage::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return StorageBinding::Wrap(aCx, this, aGivenProto);
}

uint32_t
Storage::GetLength(nsIPrincipal& aSubjectPrincipal,
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
Storage::Key(uint32_t aIndex, nsAString& aResult,
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
Storage::GetItem(const nsAString& aKey, nsAString& aResult,
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
Storage::SetItem(const nsAString& aKey, const nsAString& aData,
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
Storage::RemoveItem(const nsAString& aKey, nsIPrincipal& aSubjectPrincipal,
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
Storage::Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv)
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

namespace {

class StorageNotifierRunnable : public Runnable
{
public:
  StorageNotifierRunnable(nsISupports* aSubject, const char16_t* aType,
                          bool aPrivateBrowsing)
    : mSubject(aSubject), mType(aType), mPrivateBrowsing(aPrivateBrowsing)
  { }

  NS_DECL_NSIRUNNABLE

private:
  nsCOMPtr<nsISupports> mSubject;
  const char16_t* mType;
  const bool mPrivateBrowsing;
};

NS_IMETHODIMP
StorageNotifierRunnable::Run()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();
  if (observerService) {
    observerService->NotifyObservers(mSubject,
                                     mPrivateBrowsing
                                       ? "dom-private-storage2-changed"
                                       : "dom-storage2-changed",
                                     mType);
  }
  return NS_OK;
}

} // namespace

void
Storage::BroadcastChangeNotification(const nsSubstring& aKey,
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
  RefPtr<StorageEvent> event =
    StorageEvent::Constructor(nullptr, NS_LITERAL_STRING("storage"), dict);

  RefPtr<StorageNotifierRunnable> r =
    new StorageNotifierRunnable(event,
                                GetType() == LocalStorage
                                  ? u"localStorage"
                                  : u"sessionStorage",
                                IsPrivate());
  NS_DispatchToMainThread(r);
}

static const char kPermissionType[] = "cookie";
static const char kStorageEnabled[] = "dom.storage.enabled";

bool
Storage::CanUseStorage(nsIPrincipal& aSubjectPrincipal)
{
  // This method is responsible for correct setting of mIsSessionOnly.

  if (!mozilla::Preferences::GetBool(kStorageEnabled)) {
    return false;
  }

  nsContentUtils::StorageAccess access =
    nsContentUtils::StorageAllowedForPrincipal(mPrincipal);

  if (access == nsContentUtils::StorageAccess::eDeny) {
    return false;
  }

  mIsSessionOnly = access <= nsContentUtils::StorageAccess::eSessionScoped;
  return CanAccess(&aSubjectPrincipal);
}

Storage::StorageType
Storage::GetType() const
{
  return mManager->Type();
}

nsIPrincipal*
Storage::GetPrincipal()
{
  return mPrincipal;
}

// Defined in StorageManager.cpp
extern bool
PrincipalsEqual(nsIPrincipal* aObjectPrincipal,
                nsIPrincipal* aSubjectPrincipal);

bool
Storage::PrincipalEquals(nsIPrincipal* aPrincipal)
{
  return PrincipalsEqual(mPrincipal, aPrincipal);
}

bool
Storage::IsPrivate() const
{
  uint32_t privateBrowsingId = 0;
  nsresult rv = mPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if (NS_FAILED(rv)) {
    return false;
  }
  return privateBrowsingId > 0;
}

bool
Storage::CanAccess(nsIPrincipal* aPrincipal)
{
  return !aPrincipal || aPrincipal->Subsumes(mPrincipal);
}

void
Storage::GetSupportedNames(nsTArray<nsString>& aKeys)
{
  if (!CanUseStorage(*nsContentUtils::SubjectPrincipal())) {
    // return just an empty array
    aKeys.Clear();
    return;
  }

  mCache->GetKeys(this, aKeys);
}

} // namespace dom
} // namespace mozilla
