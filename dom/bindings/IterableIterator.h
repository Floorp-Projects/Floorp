/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The IterableIterator class is used for WebIDL interfaces that have a
 * iterable<> member defined. It handles the ES6 Iterator-like functions that
 * are generated for the iterable interface.
 *
 * For iterable interfaces, the implementation class will need to contain three
 * functions:
 *
 * - size_t GetIterableLength()
 *   - Returns the number of elements available to iterate over
 * - [type] GetKeyAtIndex(size_t index)
 *   - Returns the key at the requested index
 * - [type] GetValueAtIndex(size_t index)
 *   - Returns the value at the requested index, or the key again if this is
 *     a single type iterator.
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
#include "jswrapper.h"
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
  virtual void UnlinkHelper() = 0;
  virtual void TraverseHelper(nsCycleCollectionTraversalCallback& cb) = 0;
protected:

  virtual ~IterableIteratorBase() {}
};

template <typename T>
class IterableIterator final : public IterableIteratorBase
{
public:

  typedef bool (*WrapFunc)(JSContext* aCx,
                           mozilla::dom::IterableIterator<T>* aObject,
                           JS::Handle<JSObject*> aGivenProto,
                           JS::MutableHandle<JSObject*> aReflector);
  IterableIterator(T* aIterableObj, IterableIteratorType aIteratorType, WrapFunc aWrapFunc)
    : mIteratorType(aIteratorType)
    , mIterableObj(aIterableObj)
    , mIndex(0)
    , mWrapFunc(aWrapFunc)
  {
    MOZ_ASSERT(mIterableObj);
    MOZ_ASSERT(mWrapFunc);
  }

  void
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

  void
  Next(JSContext* aCx, JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv)
  {
    JS::Rooted<JS::Value> value(aCx, JS::UndefinedValue());
    if (mIndex >= mIterableObj->GetIterableLength()) {
      DictReturn(aCx, aResult, true, value, aRv);
      return;
    }
    switch (mIteratorType) {
    case IterableIteratorType::Keys:
    {
      if (!ToJSValue(aCx, mIterableObj->GetKeyAtIndex(mIndex), &value)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      DictReturn(aCx, aResult, false, value, aRv);
      break;
    }
    case IterableIteratorType::Values:
    {
      if (!ToJSValue(aCx, mIterableObj->GetValueAtIndex(mIndex), &value)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      DictReturn(aCx, aResult, false, value, aRv);
      break;
    }
    case IterableIteratorType::Entries:
    {
      JS::Rooted<JS::Value> key(aCx);
      if (!ToJSValue(aCx, mIterableObj->GetKeyAtIndex(mIndex), &key)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      if (!ToJSValue(aCx, mIterableObj->GetValueAtIndex(mIndex), &value)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      RootedDictionary<IterableKeyAndValueResult> dict(aCx);
      dict.mDone = false;
      // Dictionary values are a Sequence, which is a FallibleTArray, so we need
      // to check returns when appending.
      if (!dict.mValue.AppendElement(key, mozilla::fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
      if (!dict.mValue.AppendElement(value, mozilla::fallible)) {
        aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return;
      }
      JS::Rooted<JS::Value> dictValue(aCx);
      if (!ToJSValue(aCx, dict, &dictValue)) {
        aRv.Throw(NS_ERROR_FAILURE);
        return;
      }
      aResult.set(&dictValue.toObject());
      break;
    }
    default:
      MOZ_CRASH("Invalid iterator type!");
    }
    ++mIndex;
  }
  virtual ~IterableIterator() {}

  bool
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto, JS::MutableHandle<JSObject*> aObj)
  {
    return (*mWrapFunc)(aCx, this, aGivenProto, aObj);
  }

protected:
  // Tells whether this is a key, value, or entries iterator.
  IterableIteratorType mIteratorType;
  // Binding Implementation Object that we're iterating over.
  nsRefPtr<T> mIterableObj;
  // Current index of iteration.
  uint32_t mIndex;
  // Function pointer to binding-type-specific Wrap() call for this iterator.
  WrapFunc mWrapFunc;

  // Since we're templated on a binding, we need to possibly CC it, but can't do
  // that through macros. So it happens here.
  virtual void UnlinkHelper() final
  {
    mIterableObj = nullptr;
  }

  virtual void TraverseHelper(nsCycleCollectionTraversalCallback& cb) override
  {
    IterableIterator<T>* tmp = this;
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mIterableObj);
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_IterableIterator_h
