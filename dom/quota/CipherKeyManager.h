/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CIPHERKEYMANAGER_H_
#define DOM_QUOTA_CIPHERKEYMANAGER_H_

#include "mozilla/DataMutex.h"
#include "mozilla/dom/FlippedOnce.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "nsTHashMap.h"

namespace mozilla::dom::quota {

using mozilla::FlippedOnce;

template <typename CipherStrategy>
class CipherKeyManager {
  // This helper class is used by quota clients to store/retrieve cipher
  // keys in private browsing mode. All data in private mode must be encrypted
  // using a cipher key and unique IV (Initialization Vector).

  // This class uses hashmap (represented by mCipherKeys) to store cipher keys
  // and is currently used by IndexedDB and Cache quota clients. At any given
  // time, IndexedDB may contain multiple instances of this class where each is
  // used to cipherkeys relevant to a particular database. Unlike IndexedDB,
  // CacheAPI only  has one physical sqlite db  per origin, so all cipher keys
  // corresponding to an origin in cacheAPI gets stored together in this
  // hashmap.

  // Bug1859558: It could be better if QuotaManager owns cipherKeys for
  // all the quota clients and exposes, methods like
  // GetOrCreateCipherManager(aOrigin, aDatabaseName, aClientType) for
  // clients to access their respective cipherKeys scoped.
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CipherKeyManager)

  using CipherKey = typename CipherStrategy::KeyType;

 public:
  explicit CipherKeyManager(const char* aName) : mCipherKeys(aName){};

  Maybe<CipherKey> Get(const nsACString& aKeyId = "default"_ns) {
    auto lockedCipherKeys = mCipherKeys.Lock();

    MOZ_ASSERT(!mInvalidated);

    return lockedCipherKeys->MaybeGet(aKeyId);
  }

  CipherKey Ensure(const nsACString& aKeyId = "default"_ns) {
    auto lockedCipherKeys = mCipherKeys.Lock();

    MOZ_ASSERT(!mInvalidated);

    return lockedCipherKeys->LookupOrInsertWith(aKeyId, [] {
      // Generate a new key if one corresponding to keyStoreId does not exist
      // already.

      QM_TRY_RETURN(CipherStrategy::GenerateKey(), [](const auto&) {
        // Bug1800110 Propagate the error to the caller rather than asserting.
        MOZ_RELEASE_ASSERT(false);

        return CipherKey{};
      });
    });
  }

  bool Invalidated() {
    auto lockedCipherKeys = mCipherKeys.Lock();

    return mInvalidated;
  }

  // After calling this method, callers should not call any more methods on this
  // class.
  void Invalidate() {
    auto lockedCipherKeys = mCipherKeys.Lock();

    mInvalidated.Flip();

    lockedCipherKeys->Clear();
  }

 private:
  ~CipherKeyManager() = default;
  // XXX Maybe we can avoid a mutex here by moving all accesses to the
  // background thread.
  DataMutex<nsTHashMap<nsCStringHashKey, CipherKey>> mCipherKeys;

  FlippedOnce<false> mInvalidated;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_CIPHERKEYMANAGER_H_
