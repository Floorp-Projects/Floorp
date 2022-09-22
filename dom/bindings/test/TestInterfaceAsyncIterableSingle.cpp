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
#include "mozilla/dom/Promise-inl.h"
#include "nsThreadUtils.h"

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

  UniquePtr<IteratorData> data(new IteratorData());
  aIterator->SetData((void*)data.release());
}

void TestInterfaceAsyncIterableSingle::DestroyAsyncIterator(
    Iterator* aIterator) {
  auto* data = reinterpret_cast<IteratorData*>(aIterator->GetData());
  delete data;
}

already_AddRefed<Promise> TestInterfaceAsyncIterableSingle::GetNextPromise(
    Iterator* aIterator, ErrorResult& aRv) {
  return GetNextPromise(
      aIterator, reinterpret_cast<IteratorData*>(aIterator->GetData()), aRv);
}

already_AddRefed<Promise> TestInterfaceAsyncIterableSingle::GetNextPromise(
    IterableIteratorBase* aIterator, IteratorData* aData, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mParent->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIRunnable> callResolvePromise =
      NewRunnableMethod<RefPtr<IterableIteratorBase>, IteratorData*,
                        RefPtr<Promise>>(
          "TestInterfaceAsyncIterableSingle::GetNextPromise", this,
          &TestInterfaceAsyncIterableSingle::ResolvePromise, aIterator, aData,
          promise);
  if (aData->mBlockingPromisesIndex < aData->mBlockingPromises.Length()) {
    aData->mBlockingPromises[aData->mBlockingPromisesIndex]
        ->AddCallbacksWithCycleCollectedArgs(
            [](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
               nsIRunnable* aCallResolvePromise) {
              NS_DispatchToMainThread(aCallResolvePromise);
            },
            [](JSContext* aCx, JS::Handle<JS::Value> aValue, ErrorResult& aRv,
               nsIRunnable* aCallResolvePromise) {},
            std::move(callResolvePromise));
    ++aData->mBlockingPromisesIndex;
  } else {
    NS_DispatchToMainThread(callResolvePromise);
  }

  return promise.forget();
}

void TestInterfaceAsyncIterableSingle::ResolvePromise(
    IterableIteratorBase* aIterator, IteratorData* aData, Promise* aPromise) {
  if (aData->mIndex >= 10) {
    iterator_utils::ResolvePromiseForFinished(aPromise);
  } else {
    aPromise->MaybeResolve(int32_t(aData->mIndex * 9 % 7 * aData->mMultiplier));

    aData->mIndex++;
  }
}

}  // namespace mozilla::dom
