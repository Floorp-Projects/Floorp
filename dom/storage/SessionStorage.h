/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_SessionStorage_h
#define mozilla_dom_SessionStorage_h

#include "Storage.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class SessionStorageCache;
class SessionStorageManager;

class SessionStorage final : public Storage {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(SessionStorage, Storage)

  SessionStorage(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
                 nsIPrincipal* aStoragePrincipal, SessionStorageCache* aCache,
                 SessionStorageManager* aManager, const nsAString& aDocumentURI,
                 bool aIsPrivate);

  StorageType Type() const override { return eSessionStorage; }

  SessionStorageManager* Manager() const { return mManager; }

  SessionStorageCache* Cache() const { return mCache; }

  int64_t GetOriginQuotaUsage() const override;

  bool IsForkOf(const Storage* aStorage) const override;

  // WebIDL
  uint32_t GetLength(nsIPrincipal& aSubjectPrincipal,
                     ErrorResult& aRv) override;

  void Key(uint32_t aIndex, nsAString& aResult, nsIPrincipal& aSubjectPrincipal,
           ErrorResult& aRv) override;

  void GetItem(const nsAString& aKey, nsAString& aResult,
               nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) override;

  void GetSupportedNames(nsTArray<nsString>& aKeys) override;

  void SetItem(const nsAString& aKey, const nsAString& aValue,
               nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) override;

  void RemoveItem(const nsAString& aKey, nsIPrincipal& aSubjectPrincipal,
                  ErrorResult& aRv) override;

  void Clear(nsIPrincipal& aSubjectPrincipal, ErrorResult& aRv) override;

 private:
  ~SessionStorage();

  void BroadcastChangeNotification(const nsAString& aKey,
                                   const nsAString& aOldValue,
                                   const nsAString& aNewValue);

  RefPtr<SessionStorageCache> mCache;
  RefPtr<SessionStorageManager> mManager;

  nsString mDocumentURI;
  bool mIsPrivate;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_SessionStorage_h
