/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Storage_h
#define mozilla_dom_Storage_h

#include "mozilla/Attributes.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/Maybe.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsTArrayForwardDeclare.h"
#include "nsString.h"

class nsIPrincipal;
class nsPIDOMWindowInner;

namespace mozilla::dom {

class Storage : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(Storage)

  Storage(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
          nsIPrincipal* aStoragePrincipal);

  static bool StoragePrefIsEnabled();

  enum StorageType {
    eSessionStorage,
    eLocalStorage,
    ePartitionedLocalStorage,
  };

  virtual StorageType Type() const = 0;

  virtual bool IsForkOf(const Storage* aStorage) const = 0;

  virtual int64_t GetOriginQuotaUsage() const = 0;

  virtual void Disconnect() {}

  nsIPrincipal* Principal() const { return mPrincipal; }

  nsIPrincipal* StoragePrincipal() const { return mStoragePrincipal; }

  bool IsPrivateBrowsing() const { return mPrivateBrowsing; }

  bool IsSessionScopedOrLess() const { return mSessionScopedOrLess; }

  // WebIDL
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  virtual uint32_t GetLength(nsIPrincipal& aSubjectPrincipal,
                             ErrorResult& aRv) = 0;

  virtual void Key(uint32_t aIndex, nsAString& aResult,
                   nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  virtual void GetItem(const nsAString& aKey, nsAString& aResult,
                       nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  virtual void GetSupportedNames(nsTArray<nsString>& aKeys) = 0;

  void NamedGetter(const nsAString& aKey, bool& aFound, nsAString& aResult,
                   nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
    GetItem(aKey, aResult, aSubjectPrincipal, aRv);
    aFound = !aResult.IsVoid();
  }

  virtual void SetItem(const nsAString& aKey, const nsAString& aValue,
                       nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  void NamedSetter(const nsAString& aKey, const nsAString& aValue,
                   nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
    SetItem(aKey, aValue, aSubjectPrincipal, aRv);
  }

  virtual void RemoveItem(const nsAString& aKey,
                          nsIPrincipal& aSubjectPrincipal,
                          ErrorResult& aRv) = 0;

  void NamedDeleter(const nsAString& aKey, bool& aFound,
                    nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {
    RemoveItem(aKey, aSubjectPrincipal, aRv);

    aFound = !aRv.ErrorCodeIs(NS_SUCCESS_DOM_NO_OPERATION);
  }

  virtual void Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) = 0;

  // The attribute in the WebIDL interface has rather confusing name. So we
  // shouldn't use this method internally. IsSessionScopedOrLess should be used
  // directly.
  bool IsSessionOnly() const { return IsSessionScopedOrLess(); }

  //////////////////////////////////////////////////////////////////////////////
  // Testing Methods:
  //
  // These methods are exposed on the `Storage` WebIDL interface behind a
  // preference for the benefit of automated-tests.  They are not exposed to
  // content.  See `Storage.webidl` for more details.

  virtual void Open(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {}

  virtual void Close(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) {}

  virtual void BeginExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                                     ErrorResult& aRv) {}

  virtual void CheckpointExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                                          ErrorResult& aRv) {}

  virtual void EndExplicitSnapshot(nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aRv) {}

  virtual bool GetHasSnapshot(nsIPrincipal& aSubjectPrincipal,
                              ErrorResult& aRv) {
    return false;
  }

  virtual int64_t GetSnapshotUsage(nsIPrincipal& aSubjectPrincipal,
                                   ErrorResult& aRv);

  //////////////////////////////////////////////////////////////////////////////

  // Dispatch storage notification events on all impacted pages in the current
  // process as well as for consumption by devtools.  Pages receive the
  // notification via StorageNotifierService (not observers like in the past),
  // while devtools does receive the notification via the observer service.
  //
  // aStorage can be null if this method is called by LocalStorageCacheChild.
  //
  // aImmediateDispatch is for use by child IPC code (LocalStorageCacheChild)
  // so that PBackground ordering can be maintained.  Without this, the event
  // would be enqueued and run in a future turn of the event loop, potentially
  // allowing other PBackground Recv* methods to trigger script that wants to
  // assume our localstorage changes have already been applied.  This is the
  // case for message manager messages which are used by ContentTask testing
  // logic and webextensions.
  static void NotifyChange(Storage* aStorage, nsIPrincipal* aPrincipal,
                           const nsAString& aKey, const nsAString& aOldValue,
                           const nsAString& aNewValue,
                           const char16_t* aStorageType,
                           const nsAString& aDocumentURI, bool aIsPrivate,
                           bool aImmediateDispatch);

 protected:
  virtual ~Storage();

  // The method checks whether the caller can use a storage.
  bool CanUseStorage(nsIPrincipal& aSubjectPrincipal);

  virtual void LastRelease() {}

 private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIPrincipal> mStoragePrincipal;

  bool mPrivateBrowsing : 1;

  // Whether storage is set to persist data only per session, may change
  // dynamically and is set by CanUseStorage function that is called
  // before any operation on the storage.
  bool mSessionScopedOrLess : 1;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_Storage_h
