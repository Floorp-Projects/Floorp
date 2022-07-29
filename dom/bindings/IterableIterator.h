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

#include "js/RootingAPI.h"
#include "js/TypeDecls.h"
#include "js/Value.h"
#include "nsISupports.h"
#include "mozilla/dom/IterableIteratorBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/WeakPtr.h"

namespace mozilla::dom {

namespace iterator_utils {
void DictReturn(JSContext* aCx, JS::MutableHandle<JSObject*> aResult,
                bool aDone, JS::Handle<JS::Value> aValue, ErrorResult& aRv);

void KeyAndValueReturn(JSContext* aCx, JS::Handle<JS::Value> aKey,
                       JS::Handle<JS::Value> aValue,
                       JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv);

void ResolvePromiseForFinished(JSContext* aCx, Promise* aPromise,
                               ErrorResult& aRv);

void ResolvePromiseWithKeyOrValue(JSContext* aCx, Promise* aPromise,
                                  JS::Handle<JS::Value> aKeyOrValue,
                                  ErrorResult& aRv);

void ResolvePromiseWithKeyAndValue(JSContext* aCx, Promise* aPromise,
                                   JS::Handle<JS::Value> aKey,
                                   JS::Handle<JS::Value> aValue,
                                   ErrorResult& aRv);
}  // namespace iterator_utils

class IterableIteratorBase : public nsISupports {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(IterableIteratorBase)
  typedef enum { Keys = 0, Values, Entries } IteratorType;

  IterableIteratorBase() = default;

 protected:
  virtual ~IterableIteratorBase() = default;
  virtual void UnlinkHelper() = 0;
  virtual void TraverseHelper(nsCycleCollectionTraversalCallback& cb) = 0;
};

// Helpers to call iterator getter methods with the correct arguments, depending
// on the types they return, and convert the result to JS::Values.

// Helper for Get[Key,Value]AtIndex(uint32_t) methods, which accept an index and
// return a type supported by ToJSValue.
template <typename T, typename U>
bool CallIterableGetter(JSContext* aCx, U (T::*aMethod)(uint32_t), T* aInst,
                        uint32_t aIndex, JS::MutableHandle<JS::Value> aResult) {
  return ToJSValue(aCx, (aInst->*aMethod)(aIndex), aResult);
}

template <typename T, typename U>
bool CallIterableGetter(JSContext* aCx, U (T::*aMethod)(uint32_t) const,
                        const T* aInst, uint32_t aIndex,
                        JS::MutableHandle<JS::Value> aResult) {
  return ToJSValue(aCx, (aInst->*aMethod)(aIndex), aResult);
}

// Helper for Get[Key,Value]AtIndex(JSContext*, uint32_t, MutableHandleValue)
// methods, which accept a JS context, index, and mutable result value handle,
// and return true on success or false on failure.
template <typename T>
bool CallIterableGetter(JSContext* aCx,
                        bool (T::*aMethod)(JSContext*, uint32_t,
                                           JS::MutableHandle<JS::Value>),
                        T* aInst, uint32_t aIndex,
                        JS::MutableHandle<JS::Value> aResult) {
  return (aInst->*aMethod)(aCx, aIndex, aResult);
}

template <typename T>
bool CallIterableGetter(JSContext* aCx,
                        bool (T::*aMethod)(JSContext*, uint32_t,
                                           JS::MutableHandle<JS::Value>) const,
                        const T* aInst, uint32_t aIndex,
                        JS::MutableHandle<JS::Value> aResult) {
  return (aInst->*aMethod)(aCx, aIndex, aResult);
}

template <typename T>
class IterableIterator final : public IterableIteratorBase {
 public:
  typedef bool (*WrapFunc)(JSContext* aCx, IterableIterator<T>* aObject,
                           JS::Handle<JSObject*> aGivenProto,
                           JS::MutableHandle<JSObject*> aReflector);

  explicit IterableIterator(T* aIterableObj, IteratorType aIteratorType,
                            WrapFunc aWrapFunc)
      : mIterableObj(aIterableObj),
        mIteratorType(aIteratorType),
        mWrapFunc(aWrapFunc),
        mIndex(0) {
    MOZ_ASSERT(mIterableObj);
    MOZ_ASSERT(mWrapFunc);
  }

  bool GetKeyAtIndex(JSContext* aCx, uint32_t aIndex,
                     JS::MutableHandle<JS::Value> aResult) {
    return CallIterableGetter(aCx, &T::GetKeyAtIndex, mIterableObj.get(),
                              aIndex, aResult);
  }

  bool GetValueAtIndex(JSContext* aCx, uint32_t aIndex,
                       JS::MutableHandle<JS::Value> aResult) {
    return CallIterableGetter(aCx, &T::GetValueAtIndex, mIterableObj.get(),
                              aIndex, aResult);
  }

  void Next(JSContext* aCx, JS::MutableHandle<JSObject*> aResult,
            ErrorResult& aRv) {
    JS::Rooted<JS::Value> value(aCx, JS::UndefinedValue());
    if (mIndex >= this->mIterableObj->GetIterableLength()) {
      iterator_utils::DictReturn(aCx, aResult, true, value, aRv);
      return;
    }
    switch (mIteratorType) {
      case IteratorType::Keys: {
        if (!GetKeyAtIndex(aCx, mIndex, &value)) {
          aRv.Throw(NS_ERROR_FAILURE);
          return;
        }
        iterator_utils::DictReturn(aCx, aResult, false, value, aRv);
        break;
      }
      case IteratorType::Values: {
        if (!GetValueAtIndex(aCx, mIndex, &value)) {
          aRv.Throw(NS_ERROR_FAILURE);
          return;
        }
        iterator_utils::DictReturn(aCx, aResult, false, value, aRv);
        break;
      }
      case IteratorType::Entries: {
        JS::Rooted<JS::Value> key(aCx);
        if (!GetKeyAtIndex(aCx, mIndex, &key)) {
          aRv.Throw(NS_ERROR_FAILURE);
          return;
        }
        if (!GetValueAtIndex(aCx, mIndex, &value)) {
          aRv.Throw(NS_ERROR_FAILURE);
          return;
        }
        iterator_utils::KeyAndValueReturn(aCx, key, value, aResult, aRv);
        break;
      }
      default:
        MOZ_CRASH("Invalid iterator type!");
    }
    ++mIndex;
  }

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aObj) {
    return (*mWrapFunc)(aCx, this, aGivenProto, aObj);
  }

 protected:
  virtual ~IterableIterator() = default;

  // Since we're templated on a binding, we need to possibly CC it, but can't do
  // that through macros. So it happens here.
  void UnlinkHelper() final { mIterableObj = nullptr; }

  virtual void TraverseHelper(nsCycleCollectionTraversalCallback& cb) override {
    IterableIterator<T>* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIterableObj);
  }

  // Binding Implementation object that we're iterating over.
  RefPtr<T> mIterableObj;
  // Tells whether this is a key, value, or entries iterator.
  IteratorType mIteratorType;
  // Function pointer to binding-type-specific Wrap() call for this iterator.
  WrapFunc mWrapFunc;
  // Current index of iteration.
  uint32_t mIndex;
};

template <typename T>
class AsyncIterableIterator final : public IterableIteratorBase,
                                    public SupportsWeakPtr {
 public:
  typedef bool (*WrapFunc)(JSContext* aCx, AsyncIterableIterator<T>* aObject,
                           JS::Handle<JSObject*> aGivenProto,
                           JS::MutableHandle<JSObject*> aReflector);

  explicit AsyncIterableIterator(T* aIterableObj, IteratorType aIteratorType,
                                 WrapFunc aWrapFunc)
      : mIterableObj(aIterableObj),
        mIteratorType(aIteratorType),
        mWrapFunc(aWrapFunc) {
    MOZ_ASSERT(mIterableObj);
    MOZ_ASSERT(mWrapFunc);
  }

  void SetData(void* aData) { mData = aData; }

  void* GetData() { return mData; }

  IteratorType GetIteratorType() { return mIteratorType; }

  void Next(JSContext* aCx, JS::MutableHandle<JSObject*> aResult,
            ErrorResult& aRv) {
    RefPtr<Promise> promise = mIterableObj->GetNextPromise(aCx, this, aRv);
    if (!promise) {
      aRv.Throw(NS_ERROR_FAILURE);
      return;
    }
    aResult.set(promise->PromiseObj());
  }

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aObj) {
    return (*mWrapFunc)(aCx, this, aGivenProto, aObj);
  }

 protected:
  virtual ~AsyncIterableIterator() {
    // As long as iterable object does not hold strong ref to its iterators,
    // iterators will not be added to CC graph, thus make sure
    // DestroyAsyncIterator still take place.
    if (mIterableObj) {
      mIterableObj->DestroyAsyncIterator(this);
    }
  }

  // Since we're templated on a binding, we need to possibly CC it, but can't do
  // that through macros. So it happens here.
  // DestroyAsyncIterator is expected to assume that its AsyncIterableIterator
  // does not need access to mData anymore. AsyncIterator does not manage mData
  // so it should be release and null out explicitly.
  void UnlinkHelper() final {
    mIterableObj->DestroyAsyncIterator(this);
    mIterableObj = nullptr;
  }

  virtual void TraverseHelper(nsCycleCollectionTraversalCallback& cb) override {
    AsyncIterableIterator<T>* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIterableObj);
  }

  // Binding Implementation object that we're iterating over.
  RefPtr<T> mIterableObj;
  // Tells whether this is a key, value, or entries iterator.
  IteratorType mIteratorType;
  // Function pointer to binding-type-specific Wrap() call for this iterator.
  WrapFunc mWrapFunc;
  // Opaque data of the backing object.
  void* mData{nullptr};
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_IterableIterator_h
