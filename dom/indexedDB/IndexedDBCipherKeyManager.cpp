/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IndexedDBCipherKeyManager.h"

#include "mozilla/dom/quota/QuotaCommon.h"

namespace mozilla::dom::indexedDB {

Maybe<CipherKey> IndexedDBCipherKeyManager::Get(const nsACString& aKeyId) {
  auto lockedCipherKeys = mCipherKeys.Lock();

  return lockedCipherKeys->MaybeGet(aKeyId);
}

CipherKey IndexedDBCipherKeyManager::Ensure(const nsACString& aKeyId) {
  auto lockedCipherKeys = mCipherKeys.Lock();

  return lockedCipherKeys->LookupOrInsertWith(aKeyId, [] {
    // Generate a new key if one corresponding to keyStoreId does not exist
    // already.

    QM_TRY_RETURN(IndexedDBCipherStrategy::GenerateKey(), [](const auto&) {
      // Bug1800110 Propagate the error to the caller rather than asserting.
      MOZ_RELEASE_ASSERT(false);

      return CipherKey{};
    });
  });
}

}  // namespace mozilla::dom::indexedDB
