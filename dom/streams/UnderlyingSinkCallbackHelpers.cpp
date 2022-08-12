/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UnderlyingSinkCallbackHelpers.h"

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION(UnderlyingSinkAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSinkAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSinkAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSinkAlgorithmsBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_INHERITED_WITH_JS_MEMBERS(
    UnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase,
    (mGlobal, mStartCallback, mWriteCallback, mCloseCallback, mAbortCallback),
    (mUnderlyingSink))
NS_IMPL_ADDREF_INHERITED(UnderlyingSinkAlgorithms, UnderlyingSinkAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(UnderlyingSinkAlgorithms,
                          UnderlyingSinkAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSinkAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSinkAlgorithmsBase)

// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
void UnderlyingSinkAlgorithms::StartCallback(
    JSContext* aCx, WritableStreamDefaultController& aController,
    JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) {
  if (!mStartCallback) {
    // Step 2: Let startAlgorithm be an algorithm that returns undefined.
    aRetVal.setUndefined();
    return;
  }

  // Step 6: If underlyingSinkDict["start"] exists, then set startAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSinkDict["start"] with argument list « controller » and callback
  // this value underlyingSink.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSink);
  return mStartCallback->Call(thisObj, aController, aRetVal, aRv,
                              "UnderlyingSink.start",
                              CallbackFunction::eRethrowExceptions);
}

// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
already_AddRefed<Promise> UnderlyingSinkAlgorithms::WriteCallback(
    JSContext* aCx, JS::Handle<JS::Value> aChunk,
    WritableStreamDefaultController& aController, ErrorResult& aRv) {
  if (!mWriteCallback) {
    // Step 3: Let writeAlgorithm be an algorithm that returns a promise
    // resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 7: If underlyingSinkDict["write"] exists, then set writeAlgorithm to
  // an algorithm which takes an argument chunk and returns the result of
  // invoking underlyingSinkDict["write"] with argument list « chunk, controller
  // » and callback this value underlyingSink.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSink);
  RefPtr<Promise> promise = mWriteCallback->Call(
      thisObj, aChunk, aController, aRv, "UnderlyingSink.write",
      CallbackFunction::eRethrowExceptions);
  return promise.forget();
}

// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
already_AddRefed<Promise> UnderlyingSinkAlgorithms::CloseCallback(
    JSContext* aCx, ErrorResult& aRv) {
  if (!mCloseCallback) {
    // Step 4: Let closeAlgorithm be an algorithm that returns a promise
    // resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 8: If underlyingSinkDict["close"] exists, then set closeAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSinkDict["close"] with argument list «» and callback this value
  // underlyingSink.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSink);
  RefPtr<Promise> promise =
      mCloseCallback->Call(thisObj, aRv, "UnderlyingSink.close",
                           CallbackFunction::eRethrowExceptions);
  return promise.forget();
}

// https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
already_AddRefed<Promise> UnderlyingSinkAlgorithms::AbortCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  if (!mAbortCallback) {
    // Step 5: Let abortAlgorithm be an algorithm that returns a promise
    // resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 9: Let abortAlgorithm be an algorithm that returns a promise resolved
  // with undefined.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSink);
  RefPtr<Promise> promise =
      mAbortCallback->Call(thisObj, aReason, aRv, "UnderlyingSink.abort",
                           CallbackFunction::eRethrowExceptions);

  return promise.forget();
}
