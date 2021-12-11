/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_keypath_h__
#define mozilla_dom_indexeddb_keypath_h__

#include <new>
#include <utility>
#include "js/TypeDecls.h"
#include "mozilla/Result.h"
#include "nsISupports.h"
#include "nsError.h"
#include "nsString.h"
#include "nsTArray.h"

namespace JS {
template <class T>
class Heap;
}

namespace mozilla {
namespace dom {

class OwningStringOrStringSequence;
template <typename T>
class Sequence;
template <typename T>
struct Nullable;

namespace indexedDB {

class IndexMetadata;
class Key;
class ObjectStoreMetadata;

class KeyPath {
  // This private constructor is only to be used by IPDL-generated classes.
  friend class IndexMetadata;
  friend class ObjectStoreMetadata;

  KeyPath() : mType(KeyPathType::NonExistent) { MOZ_COUNT_CTOR(KeyPath); }

 public:
  enum class KeyPathType { NonExistent, String, Array, EndGuard };

  void SetType(KeyPathType aType);

  bool AppendStringWithValidation(const nsAString& aString);

  explicit KeyPath(int aDummy) : mType(KeyPathType::NonExistent) {
    MOZ_COUNT_CTOR(KeyPath);
  }

  KeyPath(KeyPath&& aOther) {
    MOZ_COUNT_CTOR(KeyPath);
    *this = std::move(aOther);
  }
  KeyPath& operator=(KeyPath&&) = default;

  KeyPath(const KeyPath& aOther) {
    MOZ_COUNT_CTOR(KeyPath);
    *this = aOther;
  }
  KeyPath& operator=(const KeyPath&) = default;

  MOZ_COUNTED_DTOR(KeyPath)

  static Result<KeyPath, nsresult> Parse(const nsAString& aString);

  static Result<KeyPath, nsresult> Parse(const Sequence<nsString>& aStrings);

  static Result<KeyPath, nsresult> Parse(
      const Nullable<OwningStringOrStringSequence>& aValue);

  nsresult ExtractKey(JSContext* aCx, const JS::Value& aValue, Key& aKey) const;

  nsresult ExtractKeyAsJSVal(JSContext* aCx, const JS::Value& aValue,
                             JS::Value* aOutVal) const;

  using ExtractOrCreateKeyCallback = nsresult (*)(JSContext*, void*);

  nsresult ExtractOrCreateKey(JSContext* aCx, const JS::Value& aValue,
                              Key& aKey, ExtractOrCreateKeyCallback aCallback,
                              void* aClosure) const;

  inline bool IsValid() const { return mType != KeyPathType::NonExistent; }

  inline bool IsArray() const { return mType == KeyPathType::Array; }

  inline bool IsString() const { return mType == KeyPathType::String; }

  inline bool IsEmpty() const {
    return mType == KeyPathType::String && mStrings[0].IsEmpty();
  }

  bool operator==(const KeyPath& aOther) const {
    return mType == aOther.mType && mStrings == aOther.mStrings;
  }

  nsAutoString SerializeToString() const;
  static KeyPath DeserializeFromString(const nsAString& aString);

  nsresult ToJSVal(JSContext* aCx, JS::MutableHandle<JS::Value> aValue) const;
  nsresult ToJSVal(JSContext* aCx, JS::Heap<JS::Value>& aValue) const;

  bool IsAllowedForObjectStore(bool aAutoIncrement) const;

  KeyPathType mType;

  CopyableTArray<nsString> mStrings;
};

}  // namespace indexedDB
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_indexeddb_keypath_h__
