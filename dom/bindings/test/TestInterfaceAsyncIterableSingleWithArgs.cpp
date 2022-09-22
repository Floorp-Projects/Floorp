/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceAsyncIterableSingleWithArgs.h"
#include "js/Value.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IterableIterator.h"
#include "mozilla/dom/Promise-inl.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(TestInterfaceAsyncIterableSingleWithArgs)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    TestInterfaceAsyncIterableSingleWithArgs, TestInterfaceAsyncIterableSingle)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    TestInterfaceAsyncIterableSingleWithArgs, TestInterfaceAsyncIterableSingle)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(
    TestInterfaceAsyncIterableSingleWithArgs, TestInterfaceAsyncIterableSingle)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mReturnLastCalledWith)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(TestInterfaceAsyncIterableSingleWithArgs,
                         TestInterfaceAsyncIterableSingle)
NS_IMPL_RELEASE_INHERITED(TestInterfaceAsyncIterableSingleWithArgs,
                          TestInterfaceAsyncIterableSingle)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    TestInterfaceAsyncIterableSingleWithArgs)
NS_INTERFACE_MAP_END_INHERITING(TestInterfaceAsyncIterableSingle)

// static
already_AddRefed<TestInterfaceAsyncIterableSingleWithArgs>
TestInterfaceAsyncIterableSingleWithArgs::Constructor(
    const GlobalObject& aGlobal, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceAsyncIterableSingleWithArgs> r =
      new TestInterfaceAsyncIterableSingleWithArgs(window);
  return r.forget();
}

JSObject* TestInterfaceAsyncIterableSingleWithArgs::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestInterfaceAsyncIterableSingleWithArgs_Binding::Wrap(aCx, this,
                                                                aGivenProto);
}

void TestInterfaceAsyncIterableSingleWithArgs::InitAsyncIteratorData(
    IteratorData& aData, Iterator::IteratorType aType,
    const TestInterfaceAsyncIteratorOptions& aOptions, ErrorResult& aError) {
  aData.mMultiplier = aOptions.mMultiplier;
  aData.mBlockingPromises = aOptions.mBlockingPromises;
}

already_AddRefed<Promise>
TestInterfaceAsyncIterableSingleWithArgs::GetNextIterationResult(
    Iterator* aIterator, ErrorResult& aRv) {
  return TestInterfaceAsyncIterableSingle::GetNextIterationResult(
      aIterator, aIterator->Data(), aRv);
}

already_AddRefed<Promise>
TestInterfaceAsyncIterableSingleWithArgs::IteratorReturn(
    JSContext* aCx, Iterator* aIterator, JS::Handle<JS::Value> aValue,
    ErrorResult& aRv) {
  ++mReturnCallCount;

  RefPtr<Promise> promise = Promise::Create(GetParentObject()->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  mReturnLastCalledWith = aValue;
  promise->MaybeResolve(JS::UndefinedHandleValue);
  return promise.forget();
}

}  // namespace mozilla::dom
