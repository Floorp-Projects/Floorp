/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/basictypes.h"

#include "IDBKeyRange.h"

#include "nsIXPConnect.h"

#include "nsJSUtils.h"
#include "nsThreadUtils.h"
#include "nsContentUtils.h"
#include "nsDOMClassInfoID.h"
#include "Key.h"

#include "mozilla/dom/IDBKeyRangeBinding.h"
#include "mozilla/dom/indexedDB/PIndexedDBIndex.h"
#include "mozilla/dom/indexedDB/PIndexedDBObjectStore.h"

using namespace mozilla;
using namespace mozilla::dom;
USING_INDEXEDDB_NAMESPACE
using namespace mozilla::dom::indexedDB::ipc;

namespace {

inline nsresult
GetKeyFromJSVal(JSContext* aCx,
                JS::Handle<JS::Value> aVal,
                Key& aKey,
                bool aAllowUnset = false)
{
  nsresult rv = aKey.SetFromJSVal(aCx, aVal);
  if (NS_FAILED(rv)) {
    NS_ASSERTION(NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM_INDEXEDDB,
                 "Bad error code!");
    return rv;
  }

  if (aKey.IsUnset() && !aAllowUnset) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  return NS_OK;
}

} // anonymous namespace

// static
nsresult
IDBKeyRange::FromJSVal(JSContext* aCx,
                       JS::Handle<JS::Value> aVal,
                       IDBKeyRange** aKeyRange)
{
  nsRefPtr<IDBKeyRange> keyRange;

  if (aVal.isNullOrUndefined()) {
    // undefined and null returns no IDBKeyRange.
    keyRange.forget(aKeyRange);
    return NS_OK;
  }

  JS::Rooted<JSObject*> obj(aCx, aVal.isObject() ? &aVal.toObject() : nullptr);
  if (aVal.isPrimitive() || JS_IsArrayObject(aCx, obj) ||
      JS_ObjectIsDate(aCx, obj)) {
    // A valid key returns an 'only' IDBKeyRange.
    keyRange = new IDBKeyRange(nullptr, false, false, true);

    nsresult rv = GetKeyFromJSVal(aCx, aVal, keyRange->Lower());
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  else {
    MOZ_ASSERT(aVal.isObject());
    // An object is not permitted unless it's another IDBKeyRange.
    if (NS_FAILED(UNWRAP_OBJECT(IDBKeyRange, obj, keyRange))) {
      return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
    }
  }

  keyRange.forget(aKeyRange);
  return NS_OK;
}

// static
template <class T>
already_AddRefed<IDBKeyRange>
IDBKeyRange::FromSerializedKeyRange(const T& aKeyRange)
{
  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(nullptr, aKeyRange.lowerOpen(), aKeyRange.upperOpen(),
                    aKeyRange.isOnly());
  keyRange->Lower() = aKeyRange.lower();
  if (!keyRange->IsOnly()) {
    keyRange->Upper() = aKeyRange.upper();
  }
  return keyRange.forget();
}

template <class T>
void
IDBKeyRange::ToSerializedKeyRange(T& aKeyRange)
{
  aKeyRange.lowerOpen() = IsLowerOpen();
  aKeyRange.upperOpen() = IsUpperOpen();
  aKeyRange.isOnly() = IsOnly();

  aKeyRange.lower() = Lower();
  if (!IsOnly()) {
    aKeyRange.upper() = Upper();
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(IDBKeyRange)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IDBKeyRange)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(IDBKeyRange)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedLowerVal)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JSVAL_MEMBER_CALLBACK(mCachedUpperVal)
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

void
IDBKeyRange::DropJSObjects()
{
  if (!mRooted) {
    return;
  }
  mCachedLowerVal = JS::UndefinedValue();
  mCachedUpperVal = JS::UndefinedValue();
  mHaveCachedLowerVal = false;
  mHaveCachedUpperVal = false;
  mRooted = false;
  mozilla::DropJSObjects(this);
}

IDBKeyRange::~IDBKeyRange()
{
  DropJSObjects();
}

JSObject*
IDBKeyRange::WrapObject(JSContext* aCx)
{
  return IDBKeyRangeBinding::Wrap(aCx, this);
}

JS::Value
IDBKeyRange::GetLower(JSContext* aCx, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveCachedLowerVal) {
    if (!mRooted) {
      mozilla::HoldJSObjects(this);
      mRooted = true;
    }

    aRv = Lower().ToJSVal(aCx, mCachedLowerVal);
    if (aRv.Failed()) {
      return JS::UndefinedValue();
    }

    mHaveCachedLowerVal = true;
  }

  return mCachedLowerVal;
}

JS::Value
IDBKeyRange::GetUpper(JSContext* aCx, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  if (!mHaveCachedUpperVal) {
    if (!mRooted) {
      mozilla::HoldJSObjects(this);
      mRooted = true;
    }

    aRv = Upper().ToJSVal(aCx, mCachedUpperVal);
    if (aRv.Failed()) {
      return JS::UndefinedValue();
    }

    mHaveCachedUpperVal = true;
  }

  return mCachedUpperVal;
}

// static
already_AddRefed<IDBKeyRange>
IDBKeyRange::Only(const GlobalObject& aGlobal, JSContext* aCx,
                  JS::Handle<JS::Value> aValue, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(aGlobal.GetAsSupports(), false, false, true);

  aRv = GetKeyFromJSVal(aCx, aValue, keyRange->Lower());
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange.forget();
}

// static
already_AddRefed<IDBKeyRange>
IDBKeyRange::LowerBound(const GlobalObject& aGlobal, JSContext* aCx,
                        JS::Handle<JS::Value> aValue, bool aOpen,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(aGlobal.GetAsSupports(), aOpen, true, false);

  aRv = GetKeyFromJSVal(aCx, aValue, keyRange->Lower());
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange.forget();
}

// static
already_AddRefed<IDBKeyRange>
IDBKeyRange::UpperBound(const GlobalObject& aGlobal, JSContext* aCx,
                        JS::Handle<JS::Value> aValue, bool aOpen,
                        ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(aGlobal.GetAsSupports(), true, aOpen, false);

  aRv = GetKeyFromJSVal(aCx, aValue, keyRange->Upper());
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange.forget();
}

// static
already_AddRefed<IDBKeyRange>
IDBKeyRange::Bound(const GlobalObject& aGlobal, JSContext* aCx,
                   JS::Handle<JS::Value> aLower, JS::Handle<JS::Value> aUpper,
                   bool aLowerOpen, bool aUpperOpen, ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(aGlobal.GetAsSupports(), aLowerOpen, aUpperOpen, false);

  aRv = GetKeyFromJSVal(aCx, aLower, keyRange->Lower());
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = GetKeyFromJSVal(aCx, aUpper, keyRange->Upper());
  if (aRv.Failed()) {
    return nullptr;
  }

  if (keyRange->Lower() > keyRange->Upper() ||
      (keyRange->Lower() == keyRange->Upper() && (aLowerOpen || aUpperOpen))) {
    aRv.Throw(NS_ERROR_DOM_INDEXEDDB_DATA_ERR);
    return nullptr;
  }

  return keyRange.forget();
}

// Explicitly instantiate for all our key range types... Grumble.
template already_AddRefed<IDBKeyRange>
IDBKeyRange::FromSerializedKeyRange<KeyRange> (const KeyRange& aKeyRange);

template void
IDBKeyRange::ToSerializedKeyRange<KeyRange> (KeyRange& aKeyRange);
