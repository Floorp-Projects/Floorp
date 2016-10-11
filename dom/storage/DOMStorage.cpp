/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "nsPIDOMWindow.h"

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

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMStorage, mManager, mPrincipal, mWindow)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMStorage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMStorage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMStorage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

DOMStorage::DOMStorage(nsPIDOMWindowInner* aWindow,
                       DOMStorageManager* aManager,
                       DOMStorageCache* aCache,
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

DOMStorage::~DOMStorage()
{
  mCache->KeepAlive();
}

/* virtual */ JSObject*
DOMStorage::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return StorageBinding::Wrap(aCx, this, aGivenProto);
}

uint32_t
DOMStorage::GetLength(nsIPrincipal& aSubjectPrincipal,
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
DOMStorage::Key(uint32_t aIndex, nsAString& aResult,
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
DOMStorage::GetItem(const nsAString& aKey, nsAString& aResult,
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
DOMStorage::SetItem(const nsAString& aKey, const nsAString& aData,
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
DOMStorage::RemoveItem(const nsAString& aKey,
                       nsIPrincipal& aSubjectPrincipal,
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
DOMStorage::Clear(nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aRv)
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

} // namespace

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
  RefPtr<StorageEvent> event =
    StorageEvent::Constructor(nullptr, NS_LITERAL_STRING("storage"), dict);

  RefPtr<StorageNotifierRunnable> r =
    new StorageNotifierRunnable(event,
                                GetType() == LocalStorage
                                  ? u"localStorage"
                                  : u"sessionStorage");
  NS_DispatchToMainThread(r);
}

static const char kPermissionType[] = "cookie";
static const char kStorageEnabled[] = "dom.storage.enabled";

bool
DOMStorage::CanUseStorage(nsIPrincipal& aSubjectPrincipal)
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
DOMStorage::IsPrivate() const
{
  uint32_t privateBrowsingId = 0;
  nsresult rv = mPrincipal->GetPrivateBrowsingId(&privateBrowsingId);
  if (NS_FAILED(rv)) {
    return false;
  }
  return privateBrowsingId > 0;
}

bool
DOMStorage::CanAccess(nsIPrincipal* aPrincipal)
{
  return !aPrincipal || aPrincipal->Subsumes(mPrincipal);
}

void
DOMStorage::GetSupportedNames(nsTArray<nsString>& aKeys)
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
