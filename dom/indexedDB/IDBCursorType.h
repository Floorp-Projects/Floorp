/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbcursortype_h__
#define mozilla_dom_idbcursortype_h__

#include "IndexedDatabase.h"
#include "mozilla/dom/indexedDB/Key.h"

namespace mozilla::dom {
namespace indexedDB {
class ObjectStoreCursorResponse;
class ObjectStoreKeyCursorResponse;
class IndexCursorResponse;
class IndexKeyCursorResponse;
}  // namespace indexedDB

enum struct IDBCursorType {
  ObjectStore,
  ObjectStoreKey,
  Index,
  IndexKey,
};

template <IDBCursorType CursorType>
struct CursorTypeTraits;

class IDBIndex;
class IDBObjectStore;

class IDBIndexCursor;
class IDBIndexKeyCursor;
class IDBObjectStoreCursor;
class IDBObjectStoreKeyCursor;

template <>
struct CursorTypeTraits<IDBCursorType::Index> {
  using Type = IDBIndexCursor;
  using ResponseType = indexedDB::IndexCursorResponse;
  static constexpr bool IsObjectStoreCursor = false;
  static constexpr bool IsKeyOnlyCursor = false;
};

template <>
struct CursorTypeTraits<IDBCursorType::IndexKey> {
  using Type = IDBIndexKeyCursor;
  using ResponseType = indexedDB::IndexKeyCursorResponse;
  static constexpr bool IsObjectStoreCursor = false;
  static constexpr bool IsKeyOnlyCursor = true;
};

template <>
struct CursorTypeTraits<IDBCursorType::ObjectStore> {
  using Type = IDBObjectStoreCursor;
  using ResponseType = indexedDB::ObjectStoreCursorResponse;
  static constexpr bool IsObjectStoreCursor = true;
  static constexpr bool IsKeyOnlyCursor = false;
};

template <>
struct CursorTypeTraits<IDBCursorType::ObjectStoreKey> {
  using Type = IDBObjectStoreKeyCursor;
  using ResponseType = indexedDB::ObjectStoreKeyCursorResponse;
  static constexpr bool IsObjectStoreCursor = true;
  static constexpr bool IsKeyOnlyCursor = true;
};

template <IDBCursorType CursorType>
using CursorSourceType =
    std::conditional_t<CursorTypeTraits<CursorType>::IsObjectStoreCursor,
                       IDBObjectStore, IDBIndex>;

using Key = indexedDB::Key;
using StructuredCloneReadInfoChild = indexedDB::StructuredCloneReadInfoChild;

struct CommonCursorDataBase {
  CommonCursorDataBase() = delete;

  explicit CommonCursorDataBase(Key aKey);

  Key mKey;  ///< The current key, i.e. the key representing the cursor's
             ///< position
             ///< (https://w3c.github.io/IndexedDB/#cursor-position).
};

template <IDBCursorType CursorType>
struct CursorData;

struct ObjectStoreCursorDataBase : CommonCursorDataBase {
  using CommonCursorDataBase::CommonCursorDataBase;

  const Key& GetSortKey(const bool aIsLocaleAware) const {
    MOZ_ASSERT(!aIsLocaleAware);
    return GetObjectStoreKey();
  }
  const Key& GetObjectStoreKey() const { return mKey; }
  static constexpr const char* GetObjectStoreKeyForLogging() { return "NA"; }
};

struct IndexCursorDataBase : CommonCursorDataBase {
  IndexCursorDataBase(Key aKey, Key aLocaleAwareKey, Key aObjectStoreKey);

  const Key& GetSortKey(const bool aIsLocaleAware) const {
    return aIsLocaleAware ? mLocaleAwareKey : mKey;
  }
  const Key& GetObjectStoreKey() const { return mObjectStoreKey; }
  const char* GetObjectStoreKeyForLogging() const {
    return GetObjectStoreKey().GetBuffer().get();
  }

  Key mLocaleAwareKey;  ///< If the index's mLocale is set, this is mKey
                        ///< converted to mLocale. Otherwise, it is unset.
  Key mObjectStoreKey;  ///< The key representing the cursor's object store
                        ///< position
  ///< (https://w3c.github.io/IndexedDB/#cursor-object-store-position).
};

struct ValueCursorDataBase {
  explicit ValueCursorDataBase(StructuredCloneReadInfoChild&& aCloneInfo);

  StructuredCloneReadInfoChild mCloneInfo;
};

template <>
struct CursorData<IDBCursorType::ObjectStoreKey> : ObjectStoreCursorDataBase {
  using ObjectStoreCursorDataBase::ObjectStoreCursorDataBase;
};

template <>
struct CursorData<IDBCursorType::ObjectStore> : ObjectStoreCursorDataBase,
                                                ValueCursorDataBase {
  CursorData(Key aKey, StructuredCloneReadInfoChild&& aCloneInfo);
};

template <>
struct CursorData<IDBCursorType::IndexKey> : IndexCursorDataBase {
  using IndexCursorDataBase::IndexCursorDataBase;
};

template <>
struct CursorData<IDBCursorType::Index> : IndexCursorDataBase,
                                          ValueCursorDataBase {
  CursorData(Key aKey, Key aLocaleAwareKey, Key aObjectStoreKey,
             StructuredCloneReadInfoChild&& aCloneInfo);
};

}  // namespace mozilla::dom

#endif
