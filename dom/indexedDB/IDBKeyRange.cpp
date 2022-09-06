/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBKeyRange.h"

#include "Key.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IDBKeyRangeBinding.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"

namespace mozilla::dom {

using namespace mozilla::dom::indexedDB;

namespace {

void GetKeyFromJSVal(JSContext* aCx, JS::Handle<JS::Value> aVal, Key& aKey,
                     ErrorResult& aRv) {
  auto result = aKey.SetFromJSVal(aCx, aVal);
  if (result.isErr()) {
    aRv = result.unwrapErr().ExtractErrorResult(
        InvalidMapsTo<NS_ERROR_DOM_INDEXEDDB_DATA_ERR>);
    return;
  }

  if (aKey.IsUnset()) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
  }
}

}  // namespace

IDBKeyRange::IDBKeyRange(nsISupports* aGlobal, bool aLowerOpen, bool aUpperOpen,
                         bool aIsOnly)
    : mGlobal(aGlobal),
      mCachedLowerVal(JS::UndefinedValue()),
      mCachedUpperVal(JS::UndefinedValue()),
      mLowerOpen(aLowerOpen),
      mUpperOpen(aUpperOpen),
      mIsOnly(aIsOnly),
      mHaveCachedLowerVal(false),
      mHaveCachedUpperVal(false),
      mRooted(false) {
  AssertIsOnOwningThread();
}

IDBKeyRange::~IDBKeyRange() { DropJSObjects(); }

IDBLocaleAwareKeyRange::IDBLocaleAwareKeyRange(nsISupports* aGlobal,
                                               bool aLowerOpen, bool aUpperOpen,
                                               bool aIsOnly)
    : IDBKeyRange(aGlobal, aLowerOpen, aUpperOpen, aIsOnly) {
  AssertIsOnOwningThread();
}

IDBLocaleAwareKeyRange::~IDBLocaleAwareKeyRange() { DropJSObjects(); }

// static
void IDBKeyRange::FromJSVal(JSContext* aCx, JS::Handle<JS::Value> aVal,
                            RefPtr<IDBKeyRange>* aKeyRange, ErrorResult& aRv) {
  MOZ_ASSERT_IF(!aCx, aVal.isUndefined());
  MOZ_ASSERT(aKeyRange);

  RefPtr<IDBKeyRange> keyRange;

  if (aVal.isNullOrUndefined()) {
    // undefined and null returns no IDBKeyRange.
    *aKeyRange = std::move(keyRange);
    return;
  }

  JS::Rooted<JSObject*> obj(aCx, aVal.isObject() ? &aVal.toObject() : nullptr);

  // Unwrap an IDBKeyRange object if possible.
  if (obj && NS_SUCCEEDED(UNWRAP_OBJECT(IDBKeyRange, obj, keyRange))) {
    MOZ_ASSERT(keyRange);
    *aKeyRange = std::move(keyRange);
    return;
  }

  // A valid key returns an 'only' IDBKeyRange.
  keyRange = new IDBKeyRange(nullptr, false, false, true);
  GetKeyFromJSVal(aCx, aVal, keyRange->Lower(), aRv);
  if (!aRv.Failed()) {
    *aKeyRange = std::move(keyRange);
  }
}

// static
RefPtr<IDBKeyRange> IDBKeyRange::FromSerialized(
    const SerializedKeyRange& aKeyRange) {
  RefPtr<IDBKeyRange> keyRange =
      new IDBKeyRange(nullptr, aKeyRange.lowerOpen(), aKeyRange.upperOpen(),
                      aKeyRange.isOnly());
  keyRange->Lower() = aKeyRange.lower();
  if (!keyRange->IsOnly()) {
    keyRange->Upper() = aKeyRange.upper();
  }
  return keyRange;
}

void IDBKeyRange::ToSerialized(SerializedKeyRange& aKeyRange) const {
  aKeyRange.lowerOpen() = LowerOpen();
  aKeyRange.upperOpen() = UpperOpen();
  aKeyRange.isOnly() = IsOnly();

  aKeyRange.lower() = Lower();
  if (!IsOnly()) {
    aKeyRange.upper() = Upper();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBKeyRange)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBKeyRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBKeyRange)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCachedLowerVal)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mCachedUpperVal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IDBKeyRange)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  tmp->DropJSObjects();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDBKeyRange)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(IDBKeyRange)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IDBKeyRange)

void IDBKeyRange::DropJSObjects() {
  if (!mRooted) {
    return;
  }
  mHaveCachedLowerVal = false;
  mHaveCachedUpperVal = false;
  mRooted = false;
  mozilla::DropJSObjects(this);
}

bool IDBKeyRange::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                             JS::MutableHandle<JSObject*> aReflector) {
  return IDBKeyRange_Binding::Wrap(aCx, this, aGivenProto, aReflector);
}

bool IDBLocaleAwareKeyRange::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
    JS::MutableHandle<JSObject*> aReflector) {
  return IDBLocaleAwareKeyRange_Binding::Wrap(aCx, this, aGivenProto,
                                              aReflector);
}

void IDBKeyRange::GetLower(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                           ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!mHaveCachedLowerVal) {
    if (!mRooted) {
      mozilla::HoldJSObjects(this);
      mRooted = true;
    }

    aRv = Lower().ToJSVal(aCx, mCachedLowerVal);
    if (aRv.Failed()) {
      return;
    }

    mHaveCachedLowerVal = true;
  }

  aResult.set(mCachedLowerVal);
}

void IDBKeyRange::GetUpper(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                           ErrorResult& aRv) {
  AssertIsOnOwningThread();

  if (!mHaveCachedUpperVal) {
    if (!mRooted) {
      mozilla::HoldJSObjects(this);
      mRooted = true;
    }

    aRv = Upper().ToJSVal(aCx, mCachedUpperVal);
    if (aRv.Failed()) {
      return;
    }

    mHaveCachedUpperVal = true;
  }

  aResult.set(mCachedUpperVal);
}

bool IDBKeyRange::Includes(JSContext* aCx, JS::Handle<JS::Value> aValue,
                           ErrorResult& aRv) const {
  Key key;
  GetKeyFromJSVal(aCx, aValue, key, aRv);
  if (aRv.Failed()) {
    return false;
  }

  MOZ_ASSERT(!(Lower().IsUnset() && Upper().IsUnset()));
  MOZ_ASSERT_IF(IsOnly(), !Lower().IsUnset() && !LowerOpen() &&
                              Lower() == Upper() && LowerOpen() == UpperOpen());

  if (!Lower().IsUnset()) {
    switch (Key::CompareKeys(Lower(), key)) {
      case 1:
        return false;
      case 0:
        // Identical keys.
        return !LowerOpen();
      case -1:
        if (IsOnly()) {
          return false;
        }
        break;
      default:
        MOZ_CRASH();
    }
  }

  if (!Upper().IsUnset()) {
    switch (Key::CompareKeys(key, Upper())) {
      case 1:
        return false;
      case 0:
        // Identical keys.
        return !UpperOpen();
      case -1:
        break;
    }
  }

  return true;
}

// static
RefPtr<IDBKeyRange> IDBKeyRange::Only(const GlobalObject& aGlobal,
                                      JS::Handle<JS::Value> aValue,
                                      ErrorResult& aRv) {
  RefPtr<IDBKeyRange> keyRange =
      new IDBKeyRange(aGlobal.GetAsSupports(), false, false, true);

  GetKeyFromJSVal(aGlobal.Context(), aValue, keyRange->Lower(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange;
}

// static
RefPtr<IDBKeyRange> IDBKeyRange::LowerBound(const GlobalObject& aGlobal,
                                            JS::Handle<JS::Value> aValue,
                                            bool aOpen, ErrorResult& aRv) {
  RefPtr<IDBKeyRange> keyRange =
      new IDBKeyRange(aGlobal.GetAsSupports(), aOpen, true, false);

  GetKeyFromJSVal(aGlobal.Context(), aValue, keyRange->Lower(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange;
}

// static
RefPtr<IDBKeyRange> IDBKeyRange::UpperBound(const GlobalObject& aGlobal,
                                            JS::Handle<JS::Value> aValue,
                                            bool aOpen, ErrorResult& aRv) {
  RefPtr<IDBKeyRange> keyRange =
      new IDBKeyRange(aGlobal.GetAsSupports(), true, aOpen, false);

  GetKeyFromJSVal(aGlobal.Context(), aValue, keyRange->Upper(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange;
}

// static
RefPtr<IDBKeyRange> IDBKeyRange::Bound(const GlobalObject& aGlobal,
                                       JS::Handle<JS::Value> aLower,
                                       JS::Handle<JS::Value> aUpper,
                                       bool aLowerOpen, bool aUpperOpen,
                                       ErrorResult& aRv) {
  RefPtr<IDBKeyRange> keyRange =
      new IDBKeyRange(aGlobal.GetAsSupports(), aLowerOpen, aUpperOpen, false);

  GetKeyFromJSVal(aGlobal.Context(), aLower, keyRange->Lower(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  GetKeyFromJSVal(aGlobal.Context(), aUpper, keyRange->Upper(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (keyRange->Lower() > keyRange->Upper() ||
      (keyRange->Lower() == keyRange->Upper() && (aLowerOpen || aUpperOpen))) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return nullptr;
  }

  return keyRange;
}

// static
RefPtr<IDBLocaleAwareKeyRange> IDBLocaleAwareKeyRange::Bound(
    const GlobalObject& aGlobal, JS::Handle<JS::Value> aLower,
    JS::Handle<JS::Value> aUpper, bool aLowerOpen, bool aUpperOpen,
    ErrorResult& aRv) {
  RefPtr<IDBLocaleAwareKeyRange> keyRange = new IDBLocaleAwareKeyRange(
      aGlobal.GetAsSupports(), aLowerOpen, aUpperOpen, false);

  GetKeyFromJSVal(aGlobal.Context(), aLower, keyRange->Lower(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  GetKeyFromJSVal(aGlobal.Context(), aUpper, keyRange->Upper(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  if (keyRange->Lower() == keyRange->Upper() && (aLowerOpen || aUpperOpen)) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return nullptr;
  }

  return keyRange;
}

}  // namespace mozilla::dom
