/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_indexeddb_keypath_h__
#define mozilla_dom_indexeddb_keypath_h__

#include "mozilla/dom/indexedDB/IndexedDatabase.h"

#include "mozilla/dom/BindingDeclarations.h"

BEGIN_INDEXEDDB_NAMESPACE

class Key;

class KeyPath
{
public:
  enum KeyPathType {
    NONEXISTENT,
    STRING,
    ARRAY,
    ENDGUARD
  };

  void SetType(KeyPathType aType);

  // This does not set exceptions.
  bool AppendStringWithValidation(JSContext* aCx, const nsAString& aString);

  explicit KeyPath(int aDummy)
  : mType(NONEXISTENT)
  {
    MOZ_COUNT_CTOR(KeyPath);
  }

  KeyPath(const KeyPath& aOther)
  {
    MOZ_COUNT_CTOR(KeyPath);
    *this = aOther;
  }

  ~KeyPath()
  {
    MOZ_COUNT_DTOR(KeyPath);
  }

  static nsresult
  Parse(JSContext* aCx, const nsAString& aString, KeyPath* aKeyPath);

  static nsresult
  Parse(JSContext* aCx, const Sequence<nsString>& aStrings, KeyPath* aKeyPath);

  static nsresult
  Parse(JSContext* aCx, const JS::Value& aValue, KeyPath* aKeyPath);

  nsresult
  ExtractKey(JSContext* aCx, const JS::Value& aValue, Key& aKey) const;

  nsresult
  ExtractKeyAsJSVal(JSContext* aCx, const JS::Value& aValue,
                    JS::Value* aOutVal) const;

  typedef nsresult
  (*ExtractOrCreateKeyCallback)(JSContext* aCx, void* aClosure);

  nsresult
  ExtractOrCreateKey(JSContext* aCx, const JS::Value& aValue, Key& aKey,
                     ExtractOrCreateKeyCallback aCallback,
                     void* aClosure) const;

  inline bool IsValid() const {
    return mType != NONEXISTENT;
  }

  inline bool IsArray() const {
    return mType == ARRAY;
  }

  inline bool IsString() const {
    return mType == STRING;
  }

  inline bool IsEmpty() const {
    return mType == STRING && mStrings[0].IsEmpty();
  }

  bool operator==(const KeyPath& aOther) const
  {
    return mType == aOther.mType && mStrings == aOther.mStrings;
  }

  void SerializeToString(nsAString& aString) const;
  static KeyPath DeserializeFromString(const nsAString& aString);

  nsresult ToJSVal(JSContext* aCx, JS::MutableHandle<JS::Value> aValue) const;
  nsresult ToJSVal(JSContext* aCx, JS::Heap<JS::Value>& aValue) const;

  bool IsAllowedForObjectStore(bool aAutoIncrement) const;

  KeyPathType mType;

  nsTArray<nsString> mStrings;
};

END_INDEXEDDB_NAMESPACE

#endif /* mozilla_dom_indexeddb_keypath_h__ */
