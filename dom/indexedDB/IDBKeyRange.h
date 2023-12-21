/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_idbkeyrange_h__
#define mozilla_dom_idbkeyrange_h__

#include "js/RootingAPI.h"
#include "js/Value.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/IndexedDatabaseManager.h"
#include "mozilla/dom/indexedDB/Key.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsString.h"

class mozIStorageStatement;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

namespace indexedDB {
class SerializedKeyRange;
}  // namespace indexedDB

class IDBKeyRange : public nsISupports {
 protected:
  nsCOMPtr<nsISupports> mGlobal;
  indexedDB::Key mLower;
  indexedDB::Key mUpper;
  JS::Heap<JS::Value> mCachedLowerVal;
  JS::Heap<JS::Value> mCachedUpperVal;

  const bool mLowerOpen : 1;
  const bool mUpperOpen : 1;
  const bool mIsOnly : 1;
  bool mHaveCachedLowerVal : 1;
  bool mHaveCachedUpperVal : 1;
  bool mRooted : 1;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(IDBKeyRange)

  // aCx is allowed to be null, but only if aVal.isUndefined().
  static void FromJSVal(JSContext* aCx, JS::Handle<JS::Value> aVal,
                        RefPtr<IDBKeyRange>* aKeyRange, ErrorResult& aRv);

  [[nodiscard]] static RefPtr<IDBKeyRange> FromSerialized(
      const indexedDB::SerializedKeyRange& aKeyRange);

  [[nodiscard]] static RefPtr<IDBKeyRange> Only(const GlobalObject& aGlobal,
                                                JS::Handle<JS::Value> aValue,
                                                ErrorResult& aRv);

  [[nodiscard]] static RefPtr<IDBKeyRange> LowerBound(
      const GlobalObject& aGlobal, JS::Handle<JS::Value> aValue, bool aOpen,
      ErrorResult& aRv);

  [[nodiscard]] static RefPtr<IDBKeyRange> UpperBound(
      const GlobalObject& aGlobal, JS::Handle<JS::Value> aValue, bool aOpen,
      ErrorResult& aRv);

  [[nodiscard]] static RefPtr<IDBKeyRange> Bound(const GlobalObject& aGlobal,
                                                 JS::Handle<JS::Value> aLower,
                                                 JS::Handle<JS::Value> aUpper,
                                                 bool aLowerOpen,
                                                 bool aUpperOpen,
                                                 ErrorResult& aRv);

  void AssertIsOnOwningThread() const { NS_ASSERT_OWNINGTHREAD(IDBKeyRange); }

  void ToSerialized(indexedDB::SerializedKeyRange& aKeyRange) const;

  const indexedDB::Key& Lower() const { return mLower; }

  indexedDB::Key& Lower() { return mLower; }

  const indexedDB::Key& Upper() const { return mIsOnly ? mLower : mUpper; }

  indexedDB::Key& Upper() { return mIsOnly ? mLower : mUpper; }

  bool Includes(JSContext* aCx, JS::Handle<JS::Value> aValue,
                ErrorResult& aRv) const;

  bool IsOnly() const { return mIsOnly; }

  void DropJSObjects();

  // WebIDL
  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aReflector);

  nsISupports* GetParentObject() const { return mGlobal; }

  void GetLower(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                ErrorResult& aRv);

  void GetUpper(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                ErrorResult& aRv);

  bool LowerOpen() const { return mLowerOpen; }

  bool UpperOpen() const { return mUpperOpen; }

 protected:
  IDBKeyRange(nsISupports* aGlobal, bool aLowerOpen, bool aUpperOpen,
              bool aIsOnly);

  virtual ~IDBKeyRange();
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_idbkeyrange_h__
