/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IDBKeyRange.h"

#include "Key.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IDBKeyRangeBinding.h"
#include "mozilla/dom/indexedDB/PBackgroundIDBSharedTypes.h"

namespace mozilla {
namespace dom {
namespace indexedDB {

namespace {

nsresult
GetKeyFromJSVal(JSContext* aCx,
                JS::Handle<JS::Value> aVal,
                Key& aKey,
                bool aAllowUnset = false)
{
  nsresult rv = aKey.SetFromJSVal(aCx, aVal);
  if (NS_FAILED(rv)) {
    MOZ_ASSERT(NS_ERROR_GET_MODULE(rv) == NS_ERROR_MODULE_DOM_INDEXEDDB);
    return rv;
  }

  if (aKey.IsUnset() && !aAllowUnset) {
    return NS_ERROR_DOM_INDEXEDDB_DATA_ERR;
  }

  return NS_OK;
}

} // anonymous namespace

IDBKeyRange::IDBKeyRange(nsISupports* aGlobal,
                         bool aLowerOpen,
                         bool aUpperOpen,
                         bool aIsOnly)
  : mGlobal(aGlobal)
  , mCachedLowerVal(JS::UndefinedValue())
  , mCachedUpperVal(JS::UndefinedValue())
  , mLowerOpen(aLowerOpen)
  , mUpperOpen(aUpperOpen)
  , mIsOnly(aIsOnly)
  , mHaveCachedLowerVal(false)
  , mHaveCachedUpperVal(false)
  , mRooted(false)
{
#ifdef DEBUG
  mOwningThread = PR_GetCurrentThread();
#endif
  AssertIsOnOwningThread();
}

IDBKeyRange::~IDBKeyRange()
{
  DropJSObjects();
}

#ifdef DEBUG

void
IDBKeyRange::AssertIsOnOwningThread() const
{
  MOZ_ASSERT(mOwningThread);
  MOZ_ASSERT(PR_GetCurrentThread() == mOwningThread);
}

#endif // DEBUG

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
already_AddRefed<IDBKeyRange>
IDBKeyRange::FromSerialized(const SerializedKeyRange& aKeyRange)
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

void
IDBKeyRange::ToSerialized(SerializedKeyRange& aKeyRange) const
{
  aKeyRange.lowerOpen() = LowerOpen();
  aKeyRange.upperOpen() = UpperOpen();
  aKeyRange.isOnly() = IsOnly();

  aKeyRange.lower() = Lower();
  if (!IsOnly()) {
    aKeyRange.upper() = Upper();
  }
}

void
IDBKeyRange::GetBindingClause(const nsACString& aKeyColumnName,
                              nsACString& _retval) const
{
  NS_NAMED_LITERAL_CSTRING(andStr, " AND ");
  NS_NAMED_LITERAL_CSTRING(spacecolon, " :");
  NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

  if (IsOnly()) {
    // Both keys are set and they're equal.
    _retval = andStr + aKeyColumnName + NS_LITERAL_CSTRING(" =") +
              spacecolon + lowerKey;
    return;
  }

  nsAutoCString clause;

  if (!Lower().IsUnset()) {
    // Lower key is set.
    clause.Append(andStr + aKeyColumnName);
    clause.AppendLiteral(" >");
    if (!LowerOpen()) {
      clause.Append('=');
    }
    clause.Append(spacecolon + lowerKey);
  }

  if (!Upper().IsUnset()) {
    // Upper key is set.
    clause.Append(andStr + aKeyColumnName);
    clause.AppendLiteral(" <");
    if (!UpperOpen()) {
      clause.Append('=');
    }
    clause.Append(spacecolon + NS_LITERAL_CSTRING("upper_key"));
  }

  _retval = clause;
}

nsresult
IDBKeyRange::BindToStatement(mozIStorageStatement* aStatement) const
{
  MOZ_ASSERT(aStatement);

  NS_NAMED_LITERAL_CSTRING(lowerKey, "lower_key");

  if (IsOnly()) {
    return Lower().BindToStatement(aStatement, lowerKey);
  }

  nsresult rv;

  if (!Lower().IsUnset()) {
    rv = Lower().BindToStatement(aStatement, lowerKey);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  if (!Upper().IsUnset()) {
    rv = Upper().BindToStatement(aStatement, NS_LITERAL_CSTRING("upper_key"));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
  }

  return NS_OK;
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
  mCachedLowerVal.setUndefined();
  mCachedUpperVal.setUndefined();
  mHaveCachedLowerVal = false;
  mHaveCachedUpperVal = false;
  mRooted = false;
  mozilla::DropJSObjects(this);
}

bool
IDBKeyRange::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aReflector)
{
  return IDBKeyRangeBinding::Wrap(aCx, this, aGivenProto, aReflector);
}

void
IDBKeyRange::GetLower(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                      ErrorResult& aRv)
{
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

  JS::ExposeValueToActiveJS(mCachedLowerVal);
  aResult.set(mCachedLowerVal);
}

void
IDBKeyRange::GetUpper(JSContext* aCx, JS::MutableHandle<JS::Value> aResult,
                      ErrorResult& aRv)
{
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

  JS::ExposeValueToActiveJS(mCachedUpperVal);
  aResult.set(mCachedUpperVal);
}

// static
already_AddRefed<IDBKeyRange>
IDBKeyRange::Only(const GlobalObject& aGlobal,
                  JS::Handle<JS::Value> aValue,
                  ErrorResult& aRv)
{
  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(aGlobal.GetAsSupports(), false, false, true);

  aRv = GetKeyFromJSVal(aGlobal.Context(), aValue, keyRange->Lower());
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange.forget();
}

// static
already_AddRefed<IDBKeyRange>
IDBKeyRange::LowerBound(const GlobalObject& aGlobal,
                        JS::Handle<JS::Value> aValue,
                        bool aOpen,
                        ErrorResult& aRv)
{
  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(aGlobal.GetAsSupports(), aOpen, true, false);

  aRv = GetKeyFromJSVal(aGlobal.Context(), aValue, keyRange->Lower());
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange.forget();
}

// static
already_AddRefed<IDBKeyRange>
IDBKeyRange::UpperBound(const GlobalObject& aGlobal,
                        JS::Handle<JS::Value> aValue,
                        bool aOpen,
                        ErrorResult& aRv)
{
  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(aGlobal.GetAsSupports(), true, aOpen, false);

  aRv = GetKeyFromJSVal(aGlobal.Context(), aValue, keyRange->Upper());
  if (aRv.Failed()) {
    return nullptr;
  }

  return keyRange.forget();
}

// static
already_AddRefed<IDBKeyRange>
IDBKeyRange::Bound(const GlobalObject& aGlobal,
                   JS::Handle<JS::Value> aLower,
                   JS::Handle<JS::Value> aUpper,
                   bool aLowerOpen,
                   bool aUpperOpen,
                   ErrorResult& aRv)
{
  nsRefPtr<IDBKeyRange> keyRange =
    new IDBKeyRange(aGlobal.GetAsSupports(), aLowerOpen, aUpperOpen, false);

  aRv = GetKeyFromJSVal(aGlobal.Context(), aLower, keyRange->Lower());
  if (aRv.Failed()) {
    return nullptr;
  }

  aRv = GetKeyFromJSVal(aGlobal.Context(), aUpper, keyRange->Upper());
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

} // namespace indexedDB
} // namespace dom
} // namespace mozilla
