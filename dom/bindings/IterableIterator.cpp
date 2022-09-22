/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/IterableIterator.h"

namespace mozilla::dom {

// Due to IterableIterator being a templated class, we implement the necessary
// CC bits in a superclass that IterableIterator then inherits from. This allows
// us to put the macros outside of the header. The base class has pure virtual
// functions for Traverse/Unlink that the templated subclasses will override.

NS_IMPL_CYCLE_COLLECTION_CLASS(IterableIteratorBase)

NS_IMPL_CYCLE_COLLECTING_ADDREF(IterableIteratorBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IterableIteratorBase)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(IterableIteratorBase)
  tmp->TraverseHelper(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(IterableIteratorBase)
  tmp->UnlinkHelper();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IterableIteratorBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

namespace iterator_utils {
void DictReturn(JSContext* aCx, JS::MutableHandle<JSObject*> aResult,
                bool aDone, JS::Handle<JS::Value> aValue, ErrorResult& aRv) {
  RootedDictionary<IterableKeyOrValueResult> dict(aCx);
  dict.mDone = aDone;
  dict.mValue = aValue;
  JS::Rooted<JS::Value> dictValue(aCx);
  if (!ToJSValue(aCx, dict, &dictValue)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aResult.set(&dictValue.toObject());
}

void KeyAndValueReturn(JSContext* aCx, JS::Handle<JS::Value> aKey,
                       JS::Handle<JS::Value> aValue,
                       JS::MutableHandle<JSObject*> aResult, ErrorResult& aRv) {
  RootedDictionary<IterableKeyAndValueResult> dict(aCx);
  dict.mDone = false;
  // Dictionary values are a Sequence, which is a FallibleTArray, so we need
  // to check returns when appending.
  if (!dict.mValue.AppendElement(aKey, mozilla::fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  if (!dict.mValue.AppendElement(aValue, mozilla::fallible)) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
  JS::Rooted<JS::Value> dictValue(aCx);
  if (!ToJSValue(aCx, dict, &dictValue)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return;
  }
  aResult.set(&dictValue.toObject());
}

void ResolvePromiseForFinished(JSContext* aCx, Promise* aPromise,
                               ErrorResult& aRv) {
  MOZ_ASSERT(aPromise);

  JS::Rooted<JSObject*> dict(aCx);
  DictReturn(aCx, &dict, true, JS::UndefinedHandleValue, aRv);
  if (aRv.Failed()) {
    return;
  }
  aPromise->MaybeResolve(dict);
}

void ResolvePromiseWithKeyOrValue(JSContext* aCx, Promise* aPromise,
                                  JS::Handle<JS::Value> aKeyOrValue,
                                  ErrorResult& aRv) {
  MOZ_ASSERT(aPromise);

  JS::Rooted<JSObject*> dict(aCx);
  DictReturn(aCx, &dict, false, aKeyOrValue, aRv);
  if (aRv.Failed()) {
    return;
  }
  aPromise->MaybeResolve(dict);
}

void ResolvePromiseWithKeyAndValue(JSContext* aCx, Promise* aPromise,
                                   JS::Handle<JS::Value> aKey,
                                   JS::Handle<JS::Value> aValue,
                                   ErrorResult& aRv) {
  MOZ_ASSERT(aPromise);

  JS::Rooted<JSObject*> dict(aCx);
  KeyAndValueReturn(aCx, aKey, aValue, &dict, aRv);
  if (aRv.Failed()) {
    return;
  }
  aPromise->MaybeResolve(dict);
}
}  // namespace iterator_utils
}  // namespace mozilla::dom
