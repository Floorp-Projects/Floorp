/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ReadableStreamDefaultController.h"
#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"
#include "mozilla/dom/UnderlyingSourceBinding.h"

namespace mozilla::dom {

// UnderlyingSourceAlgorithmsBase
NS_IMPL_CYCLE_COLLECTION(UnderlyingSourceAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSourceAlgorithmsBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourceAlgorithmsBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_INHERITED_WITH_JS_MEMBERS(
    UnderlyingSourceAlgorithms, UnderlyingSourceAlgorithmsBase,
    (mGlobal, mStartCallback, mPullCallback, mCancelCallback),
    (mUnderlyingSource))
NS_IMPL_ADDREF_INHERITED(UnderlyingSourceAlgorithms,
                         UnderlyingSourceAlgorithmsBase)
NS_IMPL_RELEASE_INHERITED(UnderlyingSourceAlgorithms,
                          UnderlyingSourceAlgorithmsBase)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourceAlgorithms)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceAlgorithmsBase)

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
void UnderlyingSourceAlgorithms::StartCallback(
    JSContext* aCx, ReadableStreamController& aController,
    JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) {
  if (!mStartCallback) {
    // Step 2: Let startAlgorithm be an algorithm that returns undefined.
    aRetVal.setUndefined();
    return;
  }

  // Step 5: If underlyingSourceDict["start"] exists, then set startAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSourceDict["start"] with argument list « controller » and
  // callback this value underlyingSource.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  ReadableStreamDefaultControllerOrReadableByteStreamController controller;
  if (aController.IsDefault()) {
    controller.SetAsReadableStreamDefaultController() = aController.AsDefault();
  } else {
    controller.SetAsReadableByteStreamController() = aController.AsByte();
  }

  return mStartCallback->Call(thisObj, controller, aRetVal, aRv,
                              "UnderlyingSource.start",
                              CallbackFunction::eRethrowExceptions);
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
already_AddRefed<Promise> UnderlyingSourceAlgorithms::PullCallback(
    JSContext* aCx, ReadableStreamController& aController, ErrorResult& aRv) {
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  if (!mPullCallback) {
    // Step 3: Let pullAlgorithm be an algorithm that returns a promise resolved
    // with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 6: If underlyingSourceDict["pull"] exists, then set pullAlgorithm to
  // an algorithm which returns the result of invoking
  // underlyingSourceDict["pull"] with argument list « controller » and callback
  // this value underlyingSource.
  ReadableStreamDefaultControllerOrReadableByteStreamController controller;
  if (aController.IsDefault()) {
    controller.SetAsReadableStreamDefaultController() = aController.AsDefault();
  } else {
    controller.SetAsReadableByteStreamController() = aController.AsByte();
  }

  RefPtr<Promise> promise =
      mPullCallback->Call(thisObj, controller, aRv, "UnderlyingSource.pull",
                          CallbackFunction::eRethrowExceptions);

  return promise.forget();
}

// https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
already_AddRefed<Promise> UnderlyingSourceAlgorithms::CancelCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  if (!mCancelCallback) {
    // Step 4: Let cancelAlgorithm be an algorithm that returns a promise
    // resolved with undefined.
    return Promise::CreateResolvedWithUndefined(mGlobal, aRv);
  }

  // Step 7: If underlyingSourceDict["cancel"] exists, then set cancelAlgorithm
  // to an algorithm which takes an argument reason and returns the result of
  // invoking underlyingSourceDict["cancel"] with argument list « reason » and
  // callback this value underlyingSource.
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSource);
  RefPtr<Promise> promise =
      mCancelCallback->Call(thisObj, aReason, aRv, "UnderlyingSource.cancel",
                            CallbackFunction::eRethrowExceptions);

  return promise.forget();
}

}  // namespace mozilla::dom
