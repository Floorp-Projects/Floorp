/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UnderlyingSourceCallbackHelpers.h"

namespace mozilla::dom {

// UnderlyingSourceStartCallbackHelper
NS_IMPL_CYCLE_COLLECTION_CLASS(UnderlyingSourceStartCallbackHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(UnderlyingSourceStartCallbackHelper)
  tmp->mThisObj.set(nullptr);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(UnderlyingSourceStartCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(UnderlyingSourceStartCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mThisObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSourceStartCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSourceStartCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourceStartCallbackHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

// UnderlyingSourcePullCallbackHelper
NS_IMPL_CYCLE_COLLECTION(UnderlyingSourcePullCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSourcePullCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSourcePullCallbackHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourcePullCallbackHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(UnderlyingSourcePullCallbackHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// IDLUnderlyingSourcePullCallbackHelper
NS_IMPL_CYCLE_COLLECTION_CLASS(IDLUnderlyingSourcePullCallbackHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    IDLUnderlyingSourcePullCallbackHelper, UnderlyingSourcePullCallbackHelper)
  tmp->mThisObj.set(nullptr);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    IDLUnderlyingSourcePullCallbackHelper, UnderlyingSourcePullCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(
    IDLUnderlyingSourcePullCallbackHelper, UnderlyingSourcePullCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mThisObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(IDLUnderlyingSourcePullCallbackHelper,
                         UnderlyingSourcePullCallbackHelper)
NS_IMPL_RELEASE_INHERITED(IDLUnderlyingSourcePullCallbackHelper,
                          UnderlyingSourcePullCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDLUnderlyingSourcePullCallbackHelper)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourcePullCallbackHelper)

// UnderlyingSourceCancelCallbackHelper
NS_IMPL_CYCLE_COLLECTION(UnderlyingSourceCancelCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSourceCancelCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSourceCancelCallbackHelper)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSourceCancelCallbackHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(UnderlyingSourceCancelCallbackHelper)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

// IDLUnderlyingSourceCancelCallbackHelper
NS_IMPL_CYCLE_COLLECTION_CLASS(IDLUnderlyingSourceCancelCallbackHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(
    IDLUnderlyingSourceCancelCallbackHelper,
    UnderlyingSourceCancelCallbackHelper)
  tmp->mThisObj.set(nullptr);
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    IDLUnderlyingSourceCancelCallbackHelper,
    UnderlyingSourceCancelCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(
    IDLUnderlyingSourceCancelCallbackHelper,
    UnderlyingSourceCancelCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mThisObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(IDLUnderlyingSourceCancelCallbackHelper,
                         UnderlyingSourceCancelCallbackHelper)
NS_IMPL_RELEASE_INHERITED(IDLUnderlyingSourceCancelCallbackHelper,
                          UnderlyingSourceCancelCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IDLUnderlyingSourceCancelCallbackHelper)
NS_INTERFACE_MAP_END_INHERITING(UnderlyingSourceCancelCallbackHelper)

void UnderlyingSourceStartCallbackHelper::StartCallback(
    JSContext* aCx, ReadableStreamDefaultController& aController,
    JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) {
  JS::RootedObject thisObj(aCx, mThisObj);
  RefPtr<UnderlyingSourceStartCallback> callback(mCallback);
  return callback->Call(thisObj, aController, aRetVal, aRv,
                        "UnderlyingSource.start",
                        CallbackFunction::eRethrowExceptions);
}

already_AddRefed<Promise> IDLUnderlyingSourcePullCallbackHelper::PullCallback(
    JSContext* aCx, ReadableStreamDefaultController& aController,
    ErrorResult& aRv) {
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1726595 for why all this
  // gorp exists.

  // Pre-allocate a promise which we may end up discarding or rejecting.
  // We do this here in order to avoid having to try allocating on a
  // failure path after the callback is called.
  nsIGlobalObject* global = GetIncumbentGlobal();
  RefPtr<Promise> maybeRejectPromise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  JS::RootedObject thisObj(aCx, mThisObj);

  // Strong Ref
  RefPtr<UnderlyingSourcePullCallback> callback(mCallback);
  RefPtr<Promise> promise =
      callback->Call(thisObj, aController, aRv, "UnderlyingSource.pull",
                     CallbackFunction::eRethrowExceptions);

  // Inform the ErrorResult that we're handling a JS exception if it happened.
  aRv.WouldReportJSException();
  if (aRv.Failed()) {
    MOZ_ASSERT(!promise);

    // Use the previously allocated promise now.
    maybeRejectPromise->MaybeReject(std::move(aRv));
    return maybeRejectPromise.forget();
  }

  return promise.forget();
}

already_AddRefed<Promise>
IDLUnderlyingSourceCancelCallbackHelper::CancelCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=1726595 for why all this
  // gorp exists.

  // Pre-allocate a promise which we may end up discarding or rejecting.
  // We do this here in order to avoid having to try allocating on a
  // failure path after the callback is called.
  nsIGlobalObject* global = GetIncumbentGlobal();
  RefPtr<Promise> maybeRejectPromise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  JS::RootedObject thisObj(aCx, mThisObj);

  // Strong Ref
  RefPtr<UnderlyingSourceCancelCallback> callback(mCallback);
  RefPtr<Promise> promise =
      callback->Call(thisObj, aReason, aRv, "UnderlyingSource.cancel",
                     CallbackFunction::eRethrowExceptions);

  // Inform the ErrorResult that we're handling a JS exception if it happened.
  aRv.WouldReportJSException();
  if (aRv.Failed()) {
    MOZ_ASSERT(!promise);

    // Use the previously allocated promise now.
    maybeRejectPromise->MaybeReject(std::move(aRv));
    return maybeRejectPromise.forget();
  }

  return promise.forget();
}

}  // namespace mozilla::dom
