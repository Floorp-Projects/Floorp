/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PartitionedLocalStorage_h
#define mozilla_dom_PartitionedLocalStorage_h

#include "Storage.h"

class nsIPrincipal;

namespace mozilla {
namespace dom {

class SessionStorageCache;

// PartitionedLocalStorage is a in-memory-only storage exposed to trackers. It
// doesn't share data with other contexts.

class PartitionedLocalStorage final : public Storage {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(PartitionedLocalStorage, Storage)

  PartitionedLocalStorage(nsPIDOMWindowInner* aWindow, nsIPrincipal* aPrincipal,
                          nsIPrincipal* aStoragePrincipal);

  StorageType Type() const override { return ePartitionedLocalStorage; }

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
  ~PartitionedLocalStorage();

  RefPtr<SessionStorageCache> mCache;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PartitionedLocalStorage_h
