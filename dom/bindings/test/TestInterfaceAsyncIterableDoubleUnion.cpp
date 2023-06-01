/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceAsyncIterableDoubleUnion.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceAsyncIterableDoubleUnion,
                                      mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceAsyncIterableDoubleUnion)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceAsyncIterableDoubleUnion)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceAsyncIterableDoubleUnion)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceAsyncIterableDoubleUnion::TestInterfaceAsyncIterableDoubleUnion(
    nsPIDOMWindowInner* aParent)
    : mParent(aParent) {
  OwningStringOrLong a;
  a.SetAsLong() = 1;
  mValues.AppendElement(std::pair<nsString, OwningStringOrLong>(u"long"_ns, a));
  a.SetAsString() = u"a"_ns;
  mValues.AppendElement(
      std::pair<nsString, OwningStringOrLong>(u"string"_ns, a));
}

// static
already_AddRefed<TestInterfaceAsyncIterableDoubleUnion>
TestInterfaceAsyncIterableDoubleUnion::Constructor(const GlobalObject& aGlobal,
                                                   ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceAsyncIterableDoubleUnion> r =
      new TestInterfaceAsyncIterableDoubleUnion(window);
  return r.forget();
}

JSObject* TestInterfaceAsyncIterableDoubleUnion::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestInterfaceAsyncIterableDoubleUnion_Binding::Wrap(aCx, this,
                                                             aGivenProto);
}

nsPIDOMWindowInner* TestInterfaceAsyncIterableDoubleUnion::GetParentObject()
    const {
  return mParent;
}

already_AddRefed<Promise>
TestInterfaceAsyncIterableDoubleUnion::GetNextIterationResult(
    Iterator* aIterator, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mParent->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  NS_DispatchToMainThread(NewRunnableMethod<RefPtr<Iterator>, RefPtr<Promise>>(
      "TestInterfaceAsyncIterableDoubleUnion::GetNextIterationResult", this,
      &TestInterfaceAsyncIterableDoubleUnion::ResolvePromise, aIterator,
      promise));

  return promise.forget();
}

void TestInterfaceAsyncIterableDoubleUnion::ResolvePromise(Iterator* aIterator,
                                                           Promise* aPromise) {
  IteratorData& data = aIterator->Data();

  // Test data:
  // [long, 1], [string, "a"]
  uint32_t idx = data.mIndex;
  if (idx >= mValues.Length()) {
    iterator_utils::ResolvePromiseForFinished(aPromise);
  } else {
    switch (aIterator->GetIteratorType()) {
      case IterableIteratorBase::IteratorType::Keys:
        aPromise->MaybeResolve(mValues[idx].first);
        break;
      case IterableIteratorBase::IteratorType::Values:
        aPromise->MaybeResolve(mValues[idx].second);
        break;
      case IterableIteratorBase::IteratorType::Entries:
        iterator_utils::ResolvePromiseWithKeyAndValue(
            aPromise, mValues[idx].first, mValues[idx].second);
        break;
    }

    data.mIndex++;
  }
}

}  // namespace mozilla::dom
