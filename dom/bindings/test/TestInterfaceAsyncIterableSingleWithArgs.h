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
  using TestInterfaceAsyncIterableSingle::TestInterfaceAsyncIterableSingle;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  static already_AddRefed<TestInterfaceAsyncIterableSingleWithArgs> Constructor(
      const GlobalObject& aGlobal, ErrorResult& rv);

  using Iterator =
      AsyncIterableIterator<TestInterfaceAsyncIterableSingleWithArgs>;
  void InitAsyncIterator(Iterator* aIterator,
                         const TestInterfaceAsyncIteratorOptions& aOptions,
                         ErrorResult& aError);
  void DestroyAsyncIterator(Iterator* aIterator);

  already_AddRefed<Promise> GetNextPromise(JSContext* aCx, Iterator* aIterator,
                                           ErrorResult& aRv);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TestInterfaceAsyncIterableSingleWithArgs_h
