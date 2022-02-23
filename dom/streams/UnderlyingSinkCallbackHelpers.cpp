/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/UnderlyingSinkCallbackHelpers.h"

using namespace mozilla::dom;

// UnderlyingSinkStartCallbackHelper
NS_IMPL_CYCLE_COLLECTION_CLASS(UnderlyingSinkStartCallbackHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(UnderlyingSinkStartCallbackHelper)
  tmp->mUnderlyingSink = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(UnderlyingSinkStartCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(UnderlyingSinkStartCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mUnderlyingSink)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSinkStartCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSinkStartCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSinkStartCallbackHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

void UnderlyingSinkStartCallbackHelper::StartCallback(
    JSContext* aCx, WritableStreamDefaultController& aController,
    JS::MutableHandle<JS::Value> aRetVal, ErrorResult& aRv) {
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSink);
  RefPtr<UnderlyingSinkStartCallback> callback(mCallback);
  return callback->Call(thisObj, aController, aRetVal, aRv,
                        "UnderlyingSink.start",
                        CallbackFunction::eRethrowExceptions);
}

// UnderlyingSinkWriteCallbackHelper
NS_IMPL_CYCLE_COLLECTION_CLASS(UnderlyingSinkWriteCallbackHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(UnderlyingSinkWriteCallbackHelper)
  tmp->mUnderlyingSink = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(UnderlyingSinkWriteCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(UnderlyingSinkWriteCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mUnderlyingSink)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSinkWriteCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSinkWriteCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSinkWriteCallbackHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<Promise> UnderlyingSinkWriteCallbackHelper::WriteCallback(
    JSContext* aCx, JS::Handle<JS::Value> aChunk,
    WritableStreamDefaultController& aController, ErrorResult& aRv) {
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSink);
  RefPtr<UnderlyingSinkWriteCallback> callback(mCallback);
  RefPtr<Promise> promise =
      callback->Call(thisObj, aChunk, aController, aRv, "UnderlyingSink.write",
                     CallbackFunction::eRethrowExceptions);
  return promise.forget();
}

// UnderlyingSinkCloseCallbackHelper
NS_IMPL_CYCLE_COLLECTION_CLASS(UnderlyingSinkCloseCallbackHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(UnderlyingSinkCloseCallbackHelper)
  tmp->mUnderlyingSink = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(UnderlyingSinkCloseCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(UnderlyingSinkCloseCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mUnderlyingSink)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSinkCloseCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSinkCloseCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSinkCloseCallbackHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<Promise> UnderlyingSinkCloseCallbackHelper::CloseCallback(
    JSContext* aCx, ErrorResult& aRv) {
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSink);
  RefPtr<UnderlyingSinkCloseCallback> callback(mCallback);
  RefPtr<Promise> promise =
      callback->Call(thisObj, aRv, "UnderlyingSink.close",
                     CallbackFunction::eRethrowExceptions);
  return promise.forget();
}

// UnderlyingSinkAbortCallbackHelper
NS_IMPL_CYCLE_COLLECTION_CLASS(UnderlyingSinkAbortCallbackHelper)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(UnderlyingSinkAbortCallbackHelper)
  tmp->mUnderlyingSink = nullptr;
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(UnderlyingSinkAbortCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(UnderlyingSinkAbortCallbackHelper)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mUnderlyingSink)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(UnderlyingSinkAbortCallbackHelper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(UnderlyingSinkAbortCallbackHelper)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(UnderlyingSinkAbortCallbackHelper)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<Promise> UnderlyingSinkAbortCallbackHelper::AbortCallback(
    JSContext* aCx, const Optional<JS::Handle<JS::Value>>& aReason,
    ErrorResult& aRv) {
  JS::Rooted<JSObject*> thisObj(aCx, mUnderlyingSink);

  // Strong Ref
  RefPtr<UnderlyingSinkAbortCallback> callback(mCallback);
  RefPtr<Promise> promise =
      callback->Call(thisObj, aReason, aRv, "UnderlyingSink.abort",
                     CallbackFunction::eRethrowExceptions);

  return promise.forget();
}
