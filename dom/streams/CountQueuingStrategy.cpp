/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CountQueuingStrategy.h"
#include "mozilla/dom/FunctionBinding.h"
#include "mozilla/dom/QueuingStrategyBinding.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION(BaseQueuingStrategy, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BaseQueuingStrategy)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BaseQueuingStrategy)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BaseQueuingStrategy)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_INHERITED(CountQueuingStrategy,
                                                BaseQueuingStrategy)
NS_IMPL_ADDREF_INHERITED(CountQueuingStrategy, BaseQueuingStrategy)
NS_IMPL_RELEASE_INHERITED(CountQueuingStrategy, BaseQueuingStrategy)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(CountQueuingStrategy)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(BaseQueuingStrategy)

/* static */
already_AddRefed<CountQueuingStrategy> CountQueuingStrategy::Constructor(
    const GlobalObject& aGlobal, const QueuingStrategyInit& aInit) {
  RefPtr<CountQueuingStrategy> strategy =
      new CountQueuingStrategy(aGlobal.GetAsSupports(), aInit.mHighWaterMark);
  return strategy.forget();
}

nsIGlobalObject* BaseQueuingStrategy::GetParentObject() const {
  return mGlobal;
}

JSObject* CountQueuingStrategy::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return CountQueuingStrategy_Binding::Wrap(aCx, this, aGivenProto);
}

// https://streams.spec.whatwg.org/#count-queuing-strategy-size-function
static bool CountQueuingStrategySize(JSContext* aCx, unsigned aArgc,
                                     JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);

  // Step 1.1. Return 1.
  args.rval().setInt32(1);
  return true;
}

// https://streams.spec.whatwg.org/#cqs-size
already_AddRefed<Function> CountQueuingStrategy::GetSize(ErrorResult& aRv) {
  // Step 1. Return this's relevant global object's count queuing strategy
  // size function.
  if (RefPtr<Function> fun = mGlobal->GetCountQueuingStrategySizeFunction()) {
    return fun.forget();
  }

  // Note: Instead of eagerly allocating a size function for every global object
  // we do it lazily once in this getter.
  // After this point the steps refer to:
  // https://streams.spec.whatwg.org/#count-queuing-strategy-size-function.

  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    aRv.ThrowUnknownError("Internal error");
    return nullptr;
  }
  JSContext* cx = jsapi.cx();

  // Step 1. Let steps be the following steps:
  // Note: See CountQueuingStrategySize instead.

  // Step 2. Let F be
  // ! CreateBuiltinFunction(steps, 0, "size", « »,
  //                         globalObject’s relevant Realm).
  JS::Rooted<JSFunction*> sizeFunction(
      cx, JS_NewFunction(cx, CountQueuingStrategySize, 0, 0, "size"));
  if (!sizeFunction) {
    aRv.StealExceptionFromJSContext(cx);
    return nullptr;
  }

  // Step 3. Set globalObject’s count queuing strategy size function to
  // a Function that represents a reference to F,
  // with callback context equal to globalObject’s relevant settings object.
  JS::Rooted<JSObject*> funObj(cx, JS_GetFunctionObject(sizeFunction));
  JS::Rooted<JSObject*> global(cx, mGlobal->GetGlobalJSObject());
  RefPtr<Function> function = new Function(cx, funObj, global, mGlobal);
  mGlobal->SetCountQueuingStrategySizeFunction(function);

  return function.forget();
}

}  // namespace mozilla::dom
