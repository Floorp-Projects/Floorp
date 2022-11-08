/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestInterfaceAsyncIterableSingleWithArgs_h
#define mozilla_dom_TestInterfaceAsyncIterableSingleWithArgs_h

#include "mozilla/dom/TestInterfaceAsyncIterableSingle.h"

namespace mozilla::dom {

struct TestInterfaceAsyncIteratorOptions;

// Implementation of test binding for webidl iterable interfaces, using
// primitives for value type
class TestInterfaceAsyncIterableSingleWithArgs final
    : public TestInterfaceAsyncIterableSingle {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(
      TestInterfaceAsyncIterableSingleWithArgs,
      TestInterfaceAsyncIterableSingle)

  using TestInterfaceAsyncIterableSingle::TestInterfaceAsyncIterableSingle;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceAsyncIterableSingleWithArgs> Constructor(
      const GlobalObject& aGlobal, ErrorResult& rv);

  using Iterator =
      AsyncIterableIterator<TestInterfaceAsyncIterableSingleWithArgs>;

  void InitAsyncIteratorData(IteratorData& aData, Iterator::IteratorType aType,
                             const TestInterfaceAsyncIteratorOptions& aOptions,
                             ErrorResult& aError);

  already_AddRefed<Promise> GetNextIterationResult(
      Iterator* aIterator, ErrorResult& aRv) MOZ_CAN_RUN_SCRIPT;
  already_AddRefed<Promise> IteratorReturn(JSContext* aCx, Iterator* aIterator,
                                           JS::Handle<JS::Value> aValue,
                                           ErrorResult& aRv) MOZ_CAN_RUN_SCRIPT;

  uint32_t ReturnCallCount() { return mReturnCallCount; }
  void GetReturnLastCalledWith(JSContext* aCx,
                               JS::MutableHandle<JS::Value> aReturnCalledWith) {
    aReturnCalledWith.set(mReturnLastCalledWith);
  }

 private:
  ~TestInterfaceAsyncIterableSingleWithArgs() = default;

  JS::Heap<JS::Value> mReturnLastCalledWith;
  uint32_t mReturnCallCount = 0;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TestInterfaceAsyncIterableSingleWithArgs_h
