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

class nsPIDOMWindowInner;

namespace mozilla {

class ErrorResult;

namespace dom {

class GlobalObject;
struct TestInterfaceAsyncIterableSingleOptions;

// Implementation of test binding for webidl iterable interfaces, using
// primitives for value type
class TestInterfaceAsyncIterableSingle final : public nsISupports,
                                               public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(TestInterfaceAsyncIterableSingle)

  TestInterfaceAsyncIterableSingle(nsPIDOMWindowInner* aParent,
                                   bool aFailToInit);
  nsPIDOMWindowInner* GetParentObject() const;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceAsyncIterableSingle> Constructor(
      const GlobalObject& aGlobal,
      const TestInterfaceAsyncIterableSingleOptions& aOptions, ErrorResult& rv);

  using Iterator = AsyncIterableIterator<TestInterfaceAsyncIterableSingle>;
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
  virtual ~TestInterfaceAsyncIterableSingle() = default;
  void ResolvePromise(IteratorData* aData);

  nsCOMPtr<nsPIDOMWindowInner> mParent;
  bool mFailToInit;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TestInterfaceAsyncIterableSingle_h
