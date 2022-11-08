/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceAsyncIterableSingle_h
#define mozilla_dom_TestInterfaceAsyncIterableSingle_h

#include "IterableIterator.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"
#include "nsTArray.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;
struct TestInterfaceAsyncIterableSingleOptions;

// Implementation of test binding for webidl iterable interfaces, using
// primitives for value type
class TestInterfaceAsyncIterableSingle : public nsISupports,
                                         public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(TestInterfaceAsyncIterableSingle)

  explicit TestInterfaceAsyncIterableSingle(nsPIDOMWindowInner* aParent,
                                            bool aFailToInit = false);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceAsyncIterableSingle> Constructor(
      const GlobalObject& aGlobal,
      const TestInterfaceAsyncIterableSingleOptions& aOptions, ErrorResult& rv);

  using Iterator = AsyncIterableIterator<TestInterfaceAsyncIterableSingle>;
  already_AddRefed<Promise> GetNextIterationResult(Iterator* aIterator,
                                                   ErrorResult& aRv);

  struct IteratorData {
    void Traverse(nsCycleCollectionTraversalCallback& cb);
    void Unlink();

    uint32_t mIndex = 0;
    uint32_t mMultiplier = 1;
    Sequence<OwningNonNull<Promise>> mBlockingPromises;
    size_t mBlockingPromisesIndex = 0;
    uint32_t mFailNextAfter = 4294967295;
    bool mThrowFromNext = false;
    RefPtr<TestThrowingCallback> mThrowFromReturn;
  };

  void InitAsyncIteratorData(IteratorData& aData, Iterator::IteratorType aType,
                             ErrorResult& aError);

 protected:
  already_AddRefed<Promise> GetNextIterationResult(
      IterableIteratorBase* aIterator, IteratorData& aData, ErrorResult& aRv);

  virtual ~TestInterfaceAsyncIterableSingle() = default;

 private:
  void ResolvePromise(IterableIteratorBase* aIterator, IteratorData& aData,
                      Promise* aPromise);

  nsCOMPtr<nsPIDOMWindowInner> mParent;
  bool mFailToInit;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceAsyncIterableSingle_h
