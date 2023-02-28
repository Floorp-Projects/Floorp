/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INDEXEDDB_INDEXEDDBCIPHERKEYMANAGER_H_
#define DOM_INDEXEDDB_INDEXEDDBCIPHERKEYMANAGER_H_

#include "mozilla/DataMutex.h"
#include "mozilla/dom/quota/IPCStreamCipherStrategy.h"
#include "nsTHashMap.h"

namespace mozilla::dom::indexedDB {

using IndexedDBCipherStrategy = quota::IPCStreamCipherStrategy;
using CipherKey = IndexedDBCipherStrategy::KeyType;

class IndexedDBCipherKeyManager {
  // This helper class is used by IndexedDB operations to store/retrieve cipher
  // keys in private browsing mode. All data in IndexedDB must be encrypted
  // using a cipher key and unique IV (Initialization Vector). While there's a
  // separate cipher key for every blob file; the SQLite database gets encrypted
  // using the commmon database key. All keys pertaining to a single IndexedDB
  // database get stored together in a hashmap. So the hashmap can be used to
  // to look up the common database key and blob keys using "default" and blob
  // file ids respectively.

 public:
  IndexedDBCipherKeyManager() : mCipherKeys("IndexedDBCipherKeyManager"){};

  Maybe<CipherKey> Get(const nsACString& aKeyId = "default"_ns);

  CipherKey Ensure(const nsACString& aKeyId = "default"_ns);

 private:
  // XXX Maybe we can avoid a mutex here by moving all accesses to the
  // background thread.
  DataMutex<nsTHashMap<nsCStringHashKey, CipherKey>> mCipherKeys;
};

}  // namespace mozilla::dom::indexedDB

#endif  // DOM_INDEXEDDB_INDEXEDDBCIPHERKEYMANAGER_H_
