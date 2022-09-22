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

  using Iterator = AsyncIterableIterator<TestInterfaceAsyncIterableDoubleUnion>;
  void InitAsyncIterator(Iterator* aIterator, ErrorResult& aError);
  void DestroyAsyncIterator(Iterator* aIterator);
  already_AddRefed<Promise> GetNextPromise(JSContext* aCx, Iterator* aIterator,
                                           ErrorResult& aRv);

 private:
  struct IteratorData {
    explicit IteratorData(int32_t aIndex) : mIndex(aIndex) {}
    ~IteratorData() {
      if (mPromise) {
        mPromise->MaybeReject(NS_ERROR_DOM_ABORT_ERR);
        mPromise = nullptr;
      }
    }
    RefPtr<Promise> mPromise;
    uint32_t mIndex;
  };

  virtual ~TestInterfaceAsyncIterableDoubleUnion() = default;
  void ResolvePromise(IteratorData* aData,
                      IterableIteratorBase::IteratorType aType);

  nsCOMPtr<nsPIDOMWindowInner> mParent;
  nsTArray<std::pair<nsString, OwningStringOrLong>> mValues;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceAsyncIterableDoubleUnion_h
