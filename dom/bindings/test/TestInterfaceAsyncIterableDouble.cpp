/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestInterfaceAsyncIterableDouble.h"
#include "mozilla/dom/TestInterfaceJSMaplikeSetlikeIterableBinding.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/BindingUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(TestInterfaceAsyncIterableDouble, mParent)

NS_IMPL_CYCLE_COLLECTING_ADDREF(TestInterfaceAsyncIterableDouble)
NS_IMPL_CYCLE_COLLECTING_RELEASE(TestInterfaceAsyncIterableDouble)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(TestInterfaceAsyncIterableDouble)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

TestInterfaceAsyncIterableDouble::TestInterfaceAsyncIterableDouble(
    nsPIDOMWindowInner* aParent)
    : mParent(aParent) {
  mValues.AppendElement(std::pair<nsString, nsString>(u"a"_ns, u"b"_ns));
  mValues.AppendElement(std::pair<nsString, nsString>(u"c"_ns, u"d"_ns));
  mValues.AppendElement(std::pair<nsString, nsString>(u"e"_ns, u"f"_ns));
}

// static
already_AddRefed<TestInterfaceAsyncIterableDouble>
TestInterfaceAsyncIterableDouble::Constructor(const GlobalObject& aGlobal,
                                              ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<TestInterfaceAsyncIterableDouble> r =
      new TestInterfaceAsyncIterableDouble(window);
  return r.forget();
}

JSObject* TestInterfaceAsyncIterableDouble::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  return TestInterfaceAsyncIterableDouble_Binding::Wrap(aCx, this, aGivenProto);
}

nsPIDOMWindowInner* TestInterfaceAsyncIterableDouble::GetParentObject() const {
  return mParent;
}

void TestInterfaceAsyncIterableDouble::InitAsyncIterator(Iterator* aIterator,
                                                         ErrorResult& aError) {
  UniquePtr<IteratorData> data(new IteratorData(0));
  aIterator->SetData((void*)data.release());
}

void TestInterfaceAsyncIterableDouble::DestroyAsyncIterator(
    Iterator* aIterator) {
  auto* data = reinterpret_cast<IteratorData*>(aIterator->GetData());
  delete data;
}

already_AddRefed<Promise> TestInterfaceAsyncIterableDouble::GetNextPromise(
    JSContext* aCx, Iterator* aIterator, ErrorResult& aRv) {
  RefPtr<Promise> promise = Promise::Create(mParent->AsGlobal(), aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  auto* data = reinterpret_cast<IteratorData*>(aIterator->GetData());
  data->mPromise = promise;

  IterableIteratorBase::IteratorType type = aIterator->GetIteratorType();
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "TestInterfaceAsyncIterableDouble::GetNextPromise",
      [data, type, self = RefPtr{this}] { self->ResolvePromise(data, type); }));

  return promise.forget();
}

void TestInterfaceAsyncIterableDouble::ResolvePromise(
    IteratorData* aData, IterableIteratorBase::IteratorType aType) {
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mParent))) {
    return;
  }
  JSContext* cx = jsapi.cx();
  ErrorResult rv;

  // Test data: ['a', 'b'], ['c', 'd'], ['e', 'f']
  uint32_t idx = aData->mIndex;
  if (idx >= mValues.Length()) {
    iterator_utils::ResolvePromiseForFinished(cx, aData->mPromise, rv);
  } else {
    JS::Rooted<JS::Value> key(cx);
    JS::Rooted<JS::Value> value(cx);
    switch (aType) {
      case IterableIteratorBase::IteratorType::Keys:
        Unused << ToJSValue(cx, mValues[idx].first, &key);
        iterator_utils::ResolvePromiseWithKeyOrValue(cx, aData->mPromise, key,
                                                     rv);
        break;
      case IterableIteratorBase::IteratorType::Values:
        Unused << ToJSValue(cx, mValues[idx].second, &value);
        iterator_utils::ResolvePromiseWithKeyOrValue(cx, aData->mPromise, value,
                                                     rv);
        break;
      case IterableIteratorBase::IteratorType::Entries:
        Unused << ToJSValue(cx, mValues[idx].first, &key);
        Unused << ToJSValue(cx, mValues[idx].second, &value);
        iterator_utils::ResolvePromiseWithKeyAndValue(cx, aData->mPromise, key,
                                                      value, rv);
        break;
    }

    aData->mIndex++;
    aData->mPromise = nullptr;
  }
}

}  // namespace mozilla::dom
