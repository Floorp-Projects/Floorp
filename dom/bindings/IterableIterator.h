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
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/dom/IterableIteratorBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/WeakPtr.h"

namespace mozilla::dom {

namespace binding_details {

// JS::MagicValue(END_OF_ITERATION) is the value we use for
// https://webidl.spec.whatwg.org/#end-of-iteration. It shouldn't be returned to
// JS, because AsyncIterableIteratorBase::NextSteps will detect it and will
// return the result of CreateIterResultObject(undefined, true) instead
// (discarding the magic value).
static const JSWhyMagic END_OF_ITERATION = JS_GENERIC_MAGIC;

}  // namespace binding_details

namespace iterator_utils {

void DictReturn(JSContext* aCx, JS::MutableHandle<JSObject*> aResult,
                bool aDone, JS::Handle<JS::Value> aValue, ErrorResult& aRv);

void KeyAndValueReturn(JSContext* aCx, JS::Handle<JS::Value> aKey,
                       JS::Handle<JS::Value> aValue,
                       JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv);

inline void ResolvePromiseForFinished(Promise* aPromise) {
  aPromise->MaybeResolve(JS::MagicValue(binding_details::END_OF_ITERATION));
}

template <typename Key, typename Value>
void ResolvePromiseWithKeyAndValue(Promise* aPromise, const Key& aKey,
                                   const Value& aValue) {
  aPromise->MaybeResolve(std::make_tuple(aKey, aValue));
}

}  // namespace iterator_utils

class IterableIteratorBase {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(IterableIteratorBase)
  NS_DECL_CYCLE_COLLECTION_NATIVE_CLASS(IterableIteratorBase)

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
class IterableIterator : public IterableIteratorBase {
 public:
  IterableIterator(T* aIterableObj, IteratorType aIteratorType)
      : mIterableObj(aIterableObj), mIteratorType(aIteratorType), mIndex(0) {
    MOZ_ASSERT(mIterableObj);
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
  // Current index of iteration.
  uint32_t mIndex;
};

namespace binding_detail {

class AsyncIterableNextImpl;
class AsyncIterableReturnImpl;

}  // namespace binding_detail

class AsyncIterableIteratorBase : public IterableIteratorBase {
 public:
  IteratorType GetIteratorType() { return mIteratorType; }

 protected:
  explicit AsyncIterableIteratorBase(IteratorType aIteratorType)
      : mIteratorType(aIteratorType) {}

  void UnlinkHelper() override {
    AsyncIterableIteratorBase* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mOngoingPromise);
  }

  void TraverseHelper(nsCycleCollectionTraversalCallback& cb) override {
    AsyncIterableIteratorBase* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mOngoingPromise);
  }

 private:
  friend class binding_detail::AsyncIterableNextImpl;
  friend class binding_detail::AsyncIterableReturnImpl;

  // 3.7.10.1. Default asynchronous iterator objects
  // Target is in AsyncIterableIterator
  // Kind
  IteratorType mIteratorType;
  // Ongoing promise
  RefPtr<Promise> mOngoingPromise;
  // Is finished
  bool mIsFinished = false;
};

template <typename T>
class AsyncIterableIterator : public AsyncIterableIteratorBase {
 private:
  using IteratorData = typename T::IteratorData;

 public:
  AsyncIterableIterator(T* aIterableObj, IteratorType aIteratorType)
      : AsyncIterableIteratorBase(aIteratorType), mIterableObj(aIterableObj) {
    MOZ_ASSERT(mIterableObj);
  }

  IteratorData& Data() { return mData; }

 protected:
  // We'd prefer to use ImplCycleCollectionTraverse/ImplCycleCollectionUnlink on
  // the iterator data, but unfortunately that doesn't work because it's
  // dependent on the template parameter. Instead we detect if the data
  // structure has Traverse and Unlink functions and call those.
  template <typename Data>
  auto TraverseData(Data& aData, nsCycleCollectionTraversalCallback& aCallback,
                    int) -> decltype(aData.Traverse(aCallback)) {
    return aData.Traverse(aCallback);
  }
  template <typename Data>
  void TraverseData(Data& aData, nsCycleCollectionTraversalCallback& aCallback,
                    double) {}

  template <typename Data>
  auto UnlinkData(Data& aData, int) -> decltype(aData.Unlink()) {
    return aData.Unlink();
  }
  template <typename Data>
  void UnlinkData(Data& aData, double) {}

  // Since we're templated on a binding, we need to possibly CC it, but can't do
  // that through macros. So it happens here.
  void UnlinkHelper() final {
    AsyncIterableIteratorBase::UnlinkHelper();

    AsyncIterableIterator<T>* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_UNLINK(mIterableObj);
    UnlinkData(tmp->mData, 0);
  }

  void TraverseHelper(nsCycleCollectionTraversalCallback& cb) final {
    AsyncIterableIteratorBase::TraverseHelper(cb);

    AsyncIterableIterator<T>* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIterableObj);
    TraverseData(tmp->mData, cb, 0);
  }

  // 3.7.10.1. Default asynchronous iterator objects
  // Target
  RefPtr<T> mIterableObj;
  // Kind
  // Ongoing promise
  // Is finished
  // See AsyncIterableIteratorBase

  // Opaque data of the backing object.
  IteratorData mData;
};

namespace binding_detail {

template <typename T>
using IterableIteratorWrapFunc =
    bool (*)(JSContext* aCx, IterableIterator<T>* aObject,
             JS::MutableHandle<JSObject*> aReflector);

template <typename T, IterableIteratorWrapFunc<T> WrapFunc>
class WrappableIterableIterator final : public IterableIterator<T> {
 public:
  using IterableIterator<T>::IterableIterator;

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aObj) {
    MOZ_ASSERT(!aGivenProto);
    return (*WrapFunc)(aCx, this, aObj);
  }
};

class AsyncIterableNextImpl {
 protected:
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Next(
      JSContext* aCx, AsyncIterableIteratorBase* aObject,
      nsISupports* aGlobalObject, ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT virtual already_AddRefed<Promise> GetNextResult(
      ErrorResult& aRv) = 0;

 private:
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> NextSteps(
      JSContext* aCx, AsyncIterableIteratorBase* aObject,
      nsIGlobalObject* aGlobalObject, ErrorResult& aRv);
};

class AsyncIterableReturnImpl {
 protected:
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Return(
      JSContext* aCx, AsyncIterableIteratorBase* aObject,
      nsISupports* aGlobalObject, JS::Handle<JS::Value> aValue,
      ErrorResult& aRv);
  MOZ_CAN_RUN_SCRIPT virtual already_AddRefed<Promise> GetReturnPromise(
      JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) = 0;

 private:
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> ReturnSteps(
      JSContext* aCx, AsyncIterableIteratorBase* aObject,
      nsIGlobalObject* aGlobalObject, JS::Handle<JS::Value> aValue,
      ErrorResult& aRv);
};

template <typename T>
class AsyncIterableIteratorNoReturn : public AsyncIterableIterator<T>,
                                      public AsyncIterableNextImpl {
 public:
  using AsyncIterableIterator<T>::AsyncIterableIterator;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Next(JSContext* aCx,
                                                    ErrorResult& aRv) {
    nsCOMPtr<nsISupports> parentObject = this->mIterableObj->GetParentObject();
    return AsyncIterableNextImpl::Next(aCx, this, parentObject, aRv);
  }

 protected:
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> GetNextResult(
      ErrorResult& aRv) override {
    RefPtr<T> iterableObj(this->mIterableObj);
    return iterableObj->GetNextIterationResult(
        static_cast<AsyncIterableIterator<T>*>(this), aRv);
  }
};

template <typename T>
class AsyncIterableIteratorWithReturn : public AsyncIterableIteratorNoReturn<T>,
                                        public AsyncIterableReturnImpl {
 public:
  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> Return(
      JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
    nsCOMPtr<nsISupports> parentObject = this->mIterableObj->GetParentObject();
    return AsyncIterableReturnImpl::Return(aCx, this, parentObject, aValue,
                                           aRv);
  }

 protected:
  using AsyncIterableIteratorNoReturn<T>::AsyncIterableIteratorNoReturn;

  MOZ_CAN_RUN_SCRIPT already_AddRefed<Promise> GetReturnPromise(
      JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv) override {
    RefPtr<T> iterableObj(this->mIterableObj);
    return iterableObj->IteratorReturn(
        aCx, static_cast<AsyncIterableIterator<T>*>(this), aValue, aRv);
  }
};

template <typename T, bool NeedReturnMethod>
using AsyncIterableIteratorNative =
    std::conditional_t<NeedReturnMethod, AsyncIterableIteratorWithReturn<T>,
                       AsyncIterableIteratorNoReturn<T>>;

template <typename T, bool NeedReturnMethod>
using AsyncIterableIteratorWrapFunc = bool (*)(
    JSContext* aCx, AsyncIterableIteratorNative<T, NeedReturnMethod>* aObject,
    JS::MutableHandle<JSObject*> aReflector);

template <typename T, bool NeedReturnMethod,
          AsyncIterableIteratorWrapFunc<T, NeedReturnMethod> WrapFunc,
          typename Base = AsyncIterableIteratorNative<T, NeedReturnMethod>>
class WrappableAsyncIterableIterator final : public Base {
 public:
  using Base::Base;

  bool WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto,
                  JS::MutableHandle<JSObject*> aObj) {
    MOZ_ASSERT(!aGivenProto);
    return (*WrapFunc)(aCx, this, aObj);
  }
};

}  // namespace binding_detail

}  // namespace mozilla::dom

#endif  // mozilla_dom_IterableIterator_h
