/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The IterableIterator class is used for WebIDL interfaces that have a
 * iterable<> member defined with two types (so a pair iterator). It handles
 * the ES6 Iterator-like functions that are generated for the iterable
 * interface.
 *
 * For iterable interfaces with a pair iterator, the implementation class will
 * need to implement these two functions:
 *
 * - size_t GetIterableLength()
 *   - Returns the number of elements available to iterate over
 * - [type] GetValueAtIndex(size_t index)
 *   - Returns the value at the requested index.
 * - [type] GetKeyAtIndex(size_t index)
 *   - Returns the key at the requested index
 *
 * Examples of iterable interface implementations can be found in the bindings
 * test directory.
 */

#ifndef mozilla_dom_IterableIterator_h
#define mozilla_dom_IterableIterator_h

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsPIDOMWindow.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/ToJSValue.h"
#include "js/Wrapper.h"
#include "mozilla/dom/IterableIteratorBinding.h"

namespace mozilla {
namespace dom {

class IterableIteratorBase : public nsISupports
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(IterableIteratorBase)
  typedef enum {
    Keys = 0,
    Values,
    Entries
  } IterableIteratorType;

  IterableIteratorBase() {}

protected:
  virtual ~IterableIteratorBase() {}
  virtual void UnlinkHelper() = 0;
  virtual void TraverseHelper(nsCycleCollectionTraversalCallback& cb) = 0;
};

// Helpers to call iterator getter methods with the correct arguments, depending
// on the types they return, and convert the result to JS::Values.

// Helper for Get[Key,Value]AtIndex(uint32_t) methods, which accept an index and
// return a type supported by ToJSValue.
template <typename T, typename U>
bool
CallIterableGetter(JSContext* aCx, U (T::* aMethod)(uint32_t),
                   T* aInst, uint32_t aIndex,
                   JS::MutableHandle<JS::Value> aResult)
{
  return ToJSValue(aCx, (aInst->*aMethod)(aIndex), aResult);
}

template <typename T, typename U>
bool
CallIterableGetter(JSContext* aCx, U (T::* aMethod)(uint32_t) const,
                   const T* aInst, uint32_t aIndex,
                   JS::MutableHandle<JS::Value> aResult)
{
  return ToJSValue(aCx, (aInst->*aMethod)(aIndex), aResult);
}


// Helper for Get[Key,Value]AtIndex(JSContext*, uint32_t, MutableHandleValue)
// methods, which accept a JS context, index, and mutable result value handle,
// and return true on success or false on failure.
template <typename T>
bool
CallIterableGetter(JSContext* aCx,
                   bool (T::* aMethod)(JSContext*, uint32_t, JS::MutableHandle<JS::Value>),
                   T* aInst, uint32_t aIndex,
                   JS::MutableHandle<JS::Value> aResult)
{
  return (aInst->*aMethod)(aCx, aIndex, aResult);
}

template <typename T>
bool
CallIterableGetter(JSContext* aCx,
                   bool (T::* aMethod)(JSContext*, uint32_t, JS::MutableHandle<JS::Value>) const,
                   const T* aInst, uint32_t aIndex,
                   JS::MutableHandle<JS::Value> aResult)
{
  return (aInst->*aMethod)(aCx, aIndex, aResult);
}

template <typename T>
class IterableIterator final : public IterableIteratorBase
{
public:
  typedef bool (*WrapFunc)(JSContext* aCx,
                           IterableIterator<T>* aObject,
                           JS::Handle<JSObject*> aGivenProto,
                           JS::MutableHandle<JSObject*> aReflector);

  explicit IterableIterator(T* aIterableObj,
                            IterableIteratorType aIteratorType,
                            WrapFunc aWrapFunc)
    : mIterableObj(aIterableObj)
    , mIteratorType(aIteratorType)
    , mWrapFunc(aWrapFunc)
    , mIndex(0)
  {
    MOZ_ASSERT(mIterableObj);
    MOZ_ASSERT(mWrapFunc);
  }

  bool
  GetKeyAtIndex(JSContext* aCx, uint32_t aIndex,
                JS::MutableHandle<JS::Value> aResult)
  {
    return CallIterableGetter(aCx, &T::GetKeyAtIndex,
                              mIterableObj.get(),
                              aIndex, aResult);
  }

  bool
  GetValueAtIndex(JSContext* aCx, uint32_t aIndex,
                  JS::MutableHandle<JS::Value> aResult)
  {
    return CallIterableGetter(aCx, &T::GetValueAtIndex,
                              mIterableObj.get(),
                              aIndex, aResult);
  }

  void
  Next(JSContext* aCx, JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv)
  {
    JS::Rooted<JS::Value> value(aCx, JS::UndefinedValue());
    if (mIndex >= this->mIterableObj->GetIterableLength()) {
      DictReturn(aCx, aResult, true, value, aRv);
      return;
    }
    switch (mIteratorType) {
    case IterableIteratorType::Keys:
    {
      if (!GetKeyAtIndex(aCx, mIndex, &value)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      DictReturn(aCx, aResult, false, value, aRv);
      break;
    }
    case IterableIteratorType::Values:
    {
      if (!GetValueAtIndex(aCx, mIndex, &value)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      DictReturn(aCx, aResult, false, value, aRv);
      break;
    }
    case IterableIteratorType::Entries:
    {
      JS::Rooted<JS::Value> key(aCx);
      if (!GetKeyAtIndex(aCx, mIndex, &key)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      if (!GetValueAtIndex(aCx, mIndex, &value)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      KeyAndValueReturn(aCx, key, value, aResult, aRv);
      break;
    }
    default:
      MOZ_CRASH("Invalid iterator type!");
    }
    ++mIndex;
  }

  bool
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aObj)
  {
    return (*mWrapFunc)(aCx, this, aGivenProto, aObj);
  }

protected:
  static void
  DictReturn(JSContext* aCx, JS::MutableHandle<JSObject*> aResult,
             bool aDone, JS::Handle<JS::Value> aValue, ErrorResult& aRv)
  {
    RootedDictionary<IterableKeyOrValueResult> dict(aCx);
    dict.mDone = aDone;
    dict.mValue = aValue;
    JS::Rooted<JS::Value> dictValue(aCx);
    if (!ToJSValue(aCx, dict, &dictValue)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    aResult.set(&dictValue.toObject());
  }

  static void
  KeyAndValueReturn(JSContext* aCx, JS::Handle<JS::Value> aKey,
                    JS::Handle<JS::Value> aValue,
                    JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv)
  {
    RootedDictionary<IterableKeyAndValueResult> dict(aCx);
    dict.mDone = false;
    // Dictionary values are a Sequence, which is a FallibleTArray, so we need
    // to check returns when appending.
    if (!dict.mValue.AppendElement(aKey, mozilla::fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    if (!dict.mValue.AppendElement(aValue, mozilla::fallible)) {
      aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }
    JS::Rooted<JS::Value> dictValue(aCx);
    if (!ToJSValue(aCx, dict, &dictValue)) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    aResult.set(&dictValue.toObject());
  }

protected:
  virtual ~IterableIterator() {}

  // Since we're templated on a binding, we need to possibly CC it, but can't do
  // that through macros. So it happens here.
  void UnlinkHelper() final
  {
    mIterableObj = nullptr;
  }

  virtual void TraverseHelper(nsCycleCollectionTraversalCallback& cb) override
  {
    IterableIterator<T>* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIterableObj);
  }

  // Binding Implementation object that we're iterating over.
  RefPtr<T> mIterableObj;
  // Tells whether this is a key, value, or entries iterator.
  IterableIteratorType mIteratorType;
  // Function pointer to binding-type-specific Wrap() call for this iterator.
  WrapFunc mWrapFunc;
  // Current index of iteration.
  uint32_t mIndex;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IterableIterator_h
