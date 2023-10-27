/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_INDEXEDDB_INDEXEDDBCIPHERKEYMANAGER_H_
#define DOM_INDEXEDDB_INDEXEDDBCIPHERKEYMANAGER_H_

#include "mozilla/dom/quota/CipherKeyManager.h"
#include "mozilla/dom/quota/IPCStreamCipherStrategy.h"

namespace mozilla::dom {

// IndexedDBCipherKeyManager is used by IndexedDB operations to store/retrieve
// keys in private browsing mode. All data in IndexedDB must be encrypted
// using a cipher key and unique IV (Initialization Vector). While there's a
// separate cipher key for every blob file; the SQLite database gets encrypted
// using the commmon database key. All keys pertaining to a single IndexedDB
// database get stored together using quota::CipherKeyManager. So, the hashmap
// can be used to look up the common database key and blob keys using "default"
// and blob file ids respectively.

using IndexedDBCipherStrategy = mozilla::dom::quota::IPCStreamCipherStrategy;
using IndexedDBCipherKeyManager =
    mozilla::dom::quota::CipherKeyManager<IndexedDBCipherStrategy>;
using CipherKey = IndexedDBCipherStrategy::KeyType;

}  // namespace mozilla::dom

#endif  // DOM_INDEXEDDB_INDEXEDDBCIPHERKEYMANAGER_H_
