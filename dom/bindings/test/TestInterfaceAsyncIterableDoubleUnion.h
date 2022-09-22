/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceAsyncIterableDoubleUnion_h
#define mozilla_dom_TestInterfaceAsyncIterableDoubleUnion_h

#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "IterableIterator.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;

// Implementation of test binding for webidl iterable interfaces, using
// primitives for value type
class TestInterfaceAsyncIterableDoubleUnion final : public nsISupports,
                                                    public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(
      TestInterfaceAsyncIterableDoubleUnion)

  explicit TestInterfaceAsyncIterableDoubleUnion(nsPIDOMWindowInner* aParent);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceAsyncIterableDoubleUnion> Constructor(
      const GlobalObject& aGlobal, ErrorResult& rv);

  struct IteratorData {
    uint32_t mIndex = 0;
  };

  using Iterator = AsyncIterableIterator<TestInterfaceAsyncIterableDoubleUnion>;

  void InitAsyncIteratorData(IteratorData& aData, Iterator::IteratorType aType,
                             ErrorResult& aError) {}

  already_AddRefed<Promise> GetNextIterationResult(Iterator* aIterator,
                                                   ErrorResult& aRv);

 private:
  virtual ~TestInterfaceAsyncIterableDoubleUnion() = default;
  void ResolvePromise(Iterator* aIterator, Promise* aPromise);

  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsTArray<std::pair<nsString, OwningStringOrLong>> mValues;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceAsyncIterableDoubleUnion_h
