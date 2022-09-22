/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceAsyncIterableSingle.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/IterableIterator.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceAsyncIterableSingle, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceAsyncIterableSingle)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceAsyncIterableSingle)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceAsyncIterableSingle)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceAsyncIterableSingle::TestInterfaceAsyncIterableSingle(
    nsPIDOMWindowInner* aParent, bool aFailToInit)
    : mParent(aParent), mFailToInit(aFailToInit) {}

// static
already_AddRefed<TestInterfaceAsyncIterableSingle>
TestInterfaceAsyncIterableSingle::Constructor(
    const GlobalObject& aGlobal,
    const TestInterfaceAsyncIterableSingleOptions& aOptions, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceAsyncIterableSingle> r =
      new TestInterfaceAsyncIterableSingle(window, aOptions.mFailToInit);
  return r.forget();
}

JSObject* TestInterfaceAsyncIterableSingle::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestInterfaceAsyncIterableSingle_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* TestInterfaceAsyncIterableSingle::GetParentObject() const {
  return mParent;
}

void TestInterfaceAsyncIterableSingle::InitAsyncIterator(Iterator* aIterator,
                                                         ErrorResult& aError) {
  if (mFailToInit) {
    aError.ThrowTypeError("Caller asked us to fail");
    return;
  }

  UniquePtr<IteratorData> data(new IteratorData(0, 1));
  aIterator->SetData((void*)data.release());
}

void TestInterfaceAsyncIterableSingle::DestroyAsyncIterator(
    Iterator* aIterator) {
  auto* data = reinterpret_cast<IteratorData*>(aIterator->GetData());
  delete data;
}

already_AddRefed<Promise> TestInterfaceAsyncIterableSingle::GetNextPromise(
    JSContext* aCx, Iterator* aIterator, ErrorResult& aRv) {
  return GetNextPromise(aCx, aIterator,
                        reinterpret_cast<IteratorData*>(aIterator->GetData()),
                        aRv);
}

already_AddRefed<Promise> TestInterfaceAsyncIterableSingle::GetNextPromise(
    JSContext* aCx, IterableIteratorBase* aIterator, IteratorData* aData,
    ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mParent->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }
  aData->mPromise = promise;

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "TestInterfaceAsyncIterableSingle::GetNextPromise",
      [iterator = RefPtr{aIterator}, aData, self = RefPtr{this}] {
        self->ResolvePromise(iterator, aData);
      }));

  return promise.forget();
}

void TestInterfaceAsyncIterableSingle::ResolvePromise(
    IterableIteratorBase* aIterator, IteratorData* aData) {
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mParent))) {
    return;
  }
  JSContext* cx = jsapi.cx();
  ErrorResult rv;
  if (aData->mIndex >= 10) {
    iterator_utils::ResolvePromiseForFinished(cx, aData->mPromise, rv);
  } else {
    JS::Rooted<JS::Value> value(cx);
    Unused << ToJSValue(
        cx, (int32_t)(aData->mIndex * 9 % 7 * aData->mMultiplier), &value);
    iterator_utils::ResolvePromiseWithKeyOrValue(cx, aData->mPromise, value,
                                                 rv);

    aData->mIndex++;
  }
  aData->mPromise = nullptr;
}

}  // namespace mozilla::dom
