/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Storage.h"

#include "mozilla/dom/StorageBinding.h"
#include "nsIPrincipal.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Storage, mWindow, mPrincipal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Storage)
NS_IMPL_CYCLE_COLLECTING_RELEASE(Storage)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Storage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMStorage)
  NS_INTERFACE_MAP_ENTRY(nsIDOMStorage)
NS_INTERFACE_MAP_END

Storage::Storage(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal)
  : mWindow(aWindow)
  , mPrincipal(aPrincipal)
  , mIsSessionOnly(false)
{
  MOZ_ASSERT(aPrincipal);
}

Storage::~Storage()
{}

bool
Storage::CanUseStorage(nsIPrincipal& aSubjectPrincipal)
{
  // This method is responsible for correct setting of mIsSessionOnly.
  if (!mozilla::Preferences::GetBool(kStorageEnabled)) {
    return false;
  }

  nsContentUtils::StorageAccess access =
    nsContentUtils::StorageAllowedForPrincipal(Principal());

  if (access == nsContentUtils::StorageAccess::eDeny) {
    return false;
  }

  mIsSessionOnly = access <= nsContentUtils::StorageAccess::eSessionScoped;

  return aSubjectPrincipal.Subsumes(mPrincipal);
}

/* virtual */ JSObject*
Storage::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  return StorageBinding::Wrap(aCx, this, aGivenProto);
}

namespace {

class StorageNotifierRunnable : public Runnable
{
public:
  StorageNotifierRunnable(nsISupports* aSubject, const char16_t *aStorageType,
                          bool aPrivateBrowsing)
    : Runnable("StorageNotifierRunnable")
    , mSubject(aSubject)
    , mStorageType(aStorageType)
    , mPrivateBrowsing(aPrivateBrowsing)
  {}

  NS_IMETHOD
  Run() override
  {
    nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();
    if (observerService) {
      observerService->NotifyObservers(mSubject,
                                       mPrivateBrowsing
                                         ? "dom-private-storage2-changed"
                                         : "dom-storage2-changed",
                                       mStorageType);
    }
    return NS_OK;
  }

private:
  nsCOMPtr<nsISupports> mSubject;
  const char16_t* mStorageType;
  const bool mPrivateBrowsing;
};

} // namespace

/* static */ void
Storage::NotifyChange(Storage* aStorage, nsIPrincipal* aPrincipal,
                      const nsAString& aKey,
                      const nsAString& aOldValue, const nsAString& aNewValue,
                      const char16_t* aStorageType,
                      const nsAString& aDocumentURI, bool aIsPrivate,
                      bool aImmediateDispatch)
{
  StorageEventInit dict;
  dict.mBubbles = false;
  dict.mCancelable = false;
  dict.mKey = aKey;
  dict.mNewValue = aNewValue;
  dict.mOldValue = aOldValue;
  dict.mStorageArea = aStorage;
  dict.mUrl = aDocumentURI;

  // Note, this DOM event should never reach JS. It is cloned later in
  // nsGlobalWindow.
  RefPtr<StorageEvent> event =
    StorageEvent::Constructor(nullptr, NS_LITERAL_STRING("storage"), dict);

  event->SetPrincipal(aPrincipal);

  RefPtr<StorageNotifierRunnable> r =
    new StorageNotifierRunnable(event, aStorageType, aIsPrivate);

  if (aImmediateDispatch) {
    Unused << r->Run();
  } else {
    NS_DispatchToMainThread(r, NS_DISPATCH_NORMAL);
  }
}

} // namespace dom
} // namespace mozilla
