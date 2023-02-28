/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddbcipherKeyManager_h
#define mozilla_dom_indexeddbcipherKeyManager_h

#include "mozilla/dom/quota/IPCStreamCipherStrategy.h"
#include "mozilla/DataMutex.h"
#include "nsTHashMap.h"

namespace mozilla::dom::indexedDB {

namespace {

using IndexedDBCipherStrategy = quota::IPCStreamCipherStrategy;
using CipherKey = IndexedDBCipherStrategy::KeyType;

class IndexedDBCipherKeyManager {
  // This helper class is used by IndexedDB operations to store/retrieve cipher
  // keys in private browsing mode. All data in IndexedDB must be encrypted
  // using a cipher key and unique IV (Initialization Vector). While there's a
  // separate cipher key for every blob file; any normal key/value pairs get
  // encrypted using the commmon database key. All keys pertaining to a single
  // database get stored together using the database id in a hashmap. We are
  // using a 2-level hashmap here; keys get stored effectively at level 2.
  // Looking up at level 1 using database id gives us access to hashmap of keys
  // corresponding to that database which we could use to look up the common
  // database key and blob keys using "default" and blob file ids respectively.

 public:
  using PrivateBrowsingInfoHashtable =
      nsTHashMap<nsCStringHashKey, nsTHashMap<nsCStringHashKey, CipherKey>>;

  IndexedDBCipherKeyManager()
      : mPrivateBrowsingInfoHashTable("IndexedDBCipherKeyManager"){};
  Maybe<CipherKey> Get(const nsCString& aDatabaseID,
                       const nsCString& keyStoreID = "default"_ns);
  CipherKey Ensure(const nsCString& aDatabaseID,
                   const nsCString& keyStoreID = "default"_ns);
  bool Remove(const nsCString& aDatabaseID);

 private:
  DataMutex<PrivateBrowsingInfoHashtable> mPrivateBrowsingInfoHashTable;
};

}  // namespace
}  // namespace mozilla::dom::indexedDB

#endif  // IndexedDBCipherKeyManager_h
