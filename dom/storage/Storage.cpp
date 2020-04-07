/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Storage.h"
#include "StorageNotifierService.h"

#include "mozilla/dom/StorageBinding.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/StorageAccess.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

static const char kStorageEnabled[] = "dom.storage.enabled";

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(Storage, mWindow, mPrincipal,
                                      mStoragePrincipal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(Storage)
NS_IMPL_CYCLE_COLLECTING_RELEASE_WITH_LAST_RELEASE(Storage, LastRelease())

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(Storage)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

Storage::Storage(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
                 nsIPrincipal* aStoragePrincipal)
    : mWindow(aWindow),
      mPrincipal(aPrincipal),
      mStoragePrincipal(aStoragePrincipal),
      mIsSessionOnly(false) {
  MOZ_ASSERT(aPrincipal);

  if (mPrincipal->IsSystemPrincipal()) {
    mIsSessionOnly = false;
  } else if (mWindow) {
    uint32_t rejectedReason = 0;
    StorageAccess access = StorageAllowedForWindow(mWindow, &rejectedReason);

    mIsSessionOnly = access <= StorageAccess::eSessionScoped;
  }
}

Storage::~Storage() = default;

/* static */
bool Storage::StoragePrefIsEnabled() {
  return mozilla::Preferences::GetBool(kStorageEnabled);
}

bool Storage::CanUseStorage(nsIPrincipal& aSubjectPrincipal) {
  if (!StoragePrefIsEnabled()) {
    return false;
  }

  return aSubjectPrincipal.Subsumes(mPrincipal);
}

/* virtual */
JSObject* Storage::WrapObject(JSContext* aCx,
                              JS::Handle<JSObject*> aGivenProto) {
  return Storage_Binding::Wrap(aCx, this, aGivenProto);
}

namespace {

class StorageNotifierRunnable : public Runnable {
 public:
  StorageNotifierRunnable(nsISupports* aSubject, const char16_t* aStorageType,
                          bool aPrivateBrowsing)
      : Runnable("StorageNotifierRunnable"),
        mSubject(aSubject),
        mStorageType(aStorageType),
        mPrivateBrowsing(aPrivateBrowsing) {}

  NS_IMETHOD
  Run() override {
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

}  // namespace

/* static */
void Storage::NotifyChange(Storage* aStorage, nsIPrincipal* aPrincipal,
                           const nsAString& aKey, const nsAString& aOldValue,
                           const nsAString& aNewValue,
                           const char16_t* aStorageType,
                           const nsAString& aDocumentURI, bool aIsPrivate,
                           bool aImmediateDispatch) {
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

  // This will send the event to any registered window.
  StorageNotifierService::Broadcast(event, aStorageType, aIsPrivate,
                                    aImmediateDispatch);

  // This runnable is mainly used by devtools. Windows receive notification by
  // StorageNotifierService.

  RefPtr<StorageNotifierRunnable> r =
      new StorageNotifierRunnable(event, aStorageType, aIsPrivate);

  if (aImmediateDispatch) {
    Unused << r->Run();
  } else {
    SchedulerGroup::Dispatch(TaskCategory::Other, r.forget());
  }
}

}  // namespace dom
}  // namespace mozilla
