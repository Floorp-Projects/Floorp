/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceAsyncIterableSingleWithArgs.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IterableIterator.h"

namespace mozilla::dom {

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

void TestInterfaceAsyncIterableSingleWithArgs::InitAsyncIterator(
    Iterator* aIterator, const TestInterfaceAsyncIteratorOptions& aOptions,
    ErrorResult& aError) {
  UniquePtr<IteratorData> data(new IteratorData(0, aOptions.mMultiplier));
  aIterator->SetData((void*)data.release());
}

void TestInterfaceAsyncIterableSingleWithArgs::DestroyAsyncIterator(
    Iterator* aIterator) {
  auto* data = reinterpret_cast<IteratorData*>(aIterator->GetData());
  delete data;
}

already_AddRefed<Promise>
TestInterfaceAsyncIterableSingleWithArgs::GetNextPromise(JSContext* aCx,
                                                         Iterator* aIterator,
                                                         ErrorResult& aRv) {
  return TestInterfaceAsyncIterableSingle::GetNextPromise(
      aCx, aIterator, reinterpret_cast<IteratorData*>(aIterator->GetData()),
      aRv);
}

}  // namespace mozilla::dom
