/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PromiseCallback.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"

#include "js/OldDebugAPI.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(PromiseCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PromiseCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PromiseCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_CLASS(PromiseCallback)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PromiseCallback)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PromiseCallback)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

PromiseCallback::PromiseCallback()
{
  MOZ_COUNT_CTOR(PromiseCallback);
}

PromiseCallback::~PromiseCallback()
{
  MOZ_COUNT_DTOR(PromiseCallback);
}

// ResolvePromiseCallback

NS_IMPL_CYCLE_COLLECTION_CLASS(ResolvePromiseCallback)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ResolvePromiseCallback,
                                                PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  tmp->mGlobal = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ResolvePromiseCallback,
                                                  PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ResolvePromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ResolvePromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(ResolvePromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(ResolvePromiseCallback, PromiseCallback)

ResolvePromiseCallback::ResolvePromiseCallback(Promise* aPromise,
                                               JS::Handle<JSObject*> aGlobal)
  : mPromise(aPromise)
  , mGlobal(aGlobal)
{
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(aGlobal);
  MOZ_COUNT_CTOR(ResolvePromiseCallback);
  HoldJSObjects(this);
}

ResolvePromiseCallback::~ResolvePromiseCallback()
{
  MOZ_COUNT_DTOR(ResolvePromiseCallback);
  DropJSObjects(this);
}

void
ResolvePromiseCallback::Call(JSContext* aCx,
                             JS::Handle<JS::Value> aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.

  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return;
  }

  mPromise->ResolveInternal(aCx, value, Promise::SyncTask);
}

// RejectPromiseCallback

NS_IMPL_CYCLE_COLLECTION_CLASS(RejectPromiseCallback)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(RejectPromiseCallback,
                                                PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromise)
  tmp->mGlobal = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(RejectPromiseCallback,
                                                  PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(RejectPromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(RejectPromiseCallback,
                                               PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_ADDREF_INHERITED(RejectPromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(RejectPromiseCallback, PromiseCallback)

RejectPromiseCallback::RejectPromiseCallback(Promise* aPromise,
                                             JS::Handle<JSObject*> aGlobal)
  : mPromise(aPromise)
  , mGlobal(aGlobal)
{
  MOZ_ASSERT(aPromise);
  MOZ_ASSERT(mGlobal);
  MOZ_COUNT_CTOR(RejectPromiseCallback);
  HoldJSObjects(this);
}

RejectPromiseCallback::~RejectPromiseCallback()
{
  MOZ_COUNT_DTOR(RejectPromiseCallback);
  DropJSObjects(this);
}

void
RejectPromiseCallback::Call(JSContext* aCx,
                            JS::Handle<JS::Value> aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.

  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return;
  }


  mPromise->RejectInternal(aCx, value, Promise::SyncTask);
}

// WrapperPromiseCallback
NS_IMPL_CYCLE_COLLECTION_CLASS(WrapperPromiseCallback)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WrapperPromiseCallback,
                                                PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNextPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  tmp->mGlobal = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WrapperPromiseCallback,
                                                  PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNextPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WrapperPromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(WrapperPromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(WrapperPromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(WrapperPromiseCallback, PromiseCallback)

WrapperPromiseCallback::WrapperPromiseCallback(Promise* aNextPromise,
                                               JS::Handle<JSObject*> aGlobal,
                                               AnyCallback* aCallback)
  : mNextPromise(aNextPromise)
  , mGlobal(aGlobal)
  , mCallback(aCallback)
{
  MOZ_ASSERT(aNextPromise);
  MOZ_ASSERT(aGlobal);
  MOZ_COUNT_CTOR(WrapperPromiseCallback);
  HoldJSObjects(this);
}

WrapperPromiseCallback::~WrapperPromiseCallback()
{
  MOZ_COUNT_DTOR(WrapperPromiseCallback);
  DropJSObjects(this);
}

void
WrapperPromiseCallback::Call(JSContext* aCx,
                             JS::Handle<JS::Value> aValue)
{
  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return;
  }

  ErrorResult rv;

  // If invoking callback threw an exception, run resolver's reject with the
  // thrown exception as argument and the synchronous flag set.
  JS::Rooted<JS::Value> retValue(aCx);
  mCallback->Call(value, &retValue, rv, CallbackObject::eRethrowExceptions);

  rv.WouldReportJSException();

  if (rv.Failed() && rv.IsJSException()) {
    JS::Rooted<JS::Value> value(aCx);
    rv.StealJSException(aCx, &value);

    if (!JS_WrapValue(aCx, &value)) {
      NS_WARNING("Failed to wrap value into the right compartment.");
      return;
    }

    mNextPromise->RejectInternal(aCx, value, Promise::SyncTask);
    return;
  }

  // If the return value is the same as the promise itself, throw TypeError.
  if (retValue.isObject()) {
    JS::Rooted<JSObject*> valueObj(aCx, &retValue.toObject());
    Promise* returnedPromise;
    nsresult r = UNWRAP_OBJECT(Promise, valueObj, returnedPromise);

    if (NS_SUCCEEDED(r) && returnedPromise == mNextPromise) {
      const char* fileName = nullptr;
      uint32_t lineNumber = 0;

      // Try to get some information about the callback to report a sane error,
      // but don't try too hard (only deals with scripted functions).
      JS::Rooted<JSObject*> unwrapped(aCx,
        js::CheckedUnwrap(mCallback->Callback()));

      if (unwrapped) {
        JSAutoCompartment ac(aCx, unwrapped);
        if (JS_ObjectIsFunction(aCx, unwrapped)) {
          JS::Rooted<JS::Value> asValue(aCx, JS::ObjectValue(*unwrapped));
          JS::Rooted<JSFunction*> func(aCx, JS_ValueToFunction(aCx, asValue));

          MOZ_ASSERT(func);
          JSScript* script = JS_GetFunctionScript(aCx, func);
          if (script) {
            fileName = JS_GetScriptFilename(script);
            lineNumber = JS_GetScriptBaseLineNumber(aCx, script);
          }
        }
      }

      // We're back in aValue's compartment here.
      JS::Rooted<JSString*> stack(aCx, JS_GetEmptyString(JS_GetRuntime(aCx)));
      JS::Rooted<JSString*> fn(aCx, JS_NewStringCopyZ(aCx, fileName));
      if (!fn) {
        // Out of memory. Promise will stay unresolved.
        JS_ClearPendingException(aCx);
        return;
      }

      JS::Rooted<JSString*> message(aCx,
        JS_NewStringCopyZ(aCx,
          "then() cannot return same Promise that it resolves."));
      if (!message) {
        // Out of memory. Promise will stay unresolved.
        JS_ClearPendingException(aCx);
        return;
      }

      JS::Rooted<JS::Value> typeError(aCx);
      if (!JS::CreateTypeError(aCx, stack, fn, lineNumber, 0,
                               nullptr, message, &typeError)) {
        // Out of memory. Promise will stay unresolved.
        JS_ClearPendingException(aCx);
        return;
      }

      mNextPromise->RejectInternal(aCx, typeError, Promise::SyncTask);
      return;
    }
  }

  // Otherwise, run resolver's resolve with value and the synchronous flag
  // set.
  if (!JS_WrapValue(aCx, &retValue)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return;
  }

  mNextPromise->ResolveInternal(aCx, retValue, Promise::SyncTask);
}

// NativePromiseCallback

NS_IMPL_CYCLE_COLLECTION_INHERITED(NativePromiseCallback,
                                   PromiseCallback, mHandler)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(NativePromiseCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(NativePromiseCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(NativePromiseCallback, PromiseCallback)

NativePromiseCallback::NativePromiseCallback(PromiseNativeHandler* aHandler,
                                             Promise::PromiseState aState)
  : mHandler(aHandler)
  , mState(aState)
{
  MOZ_ASSERT(aHandler);
  MOZ_COUNT_CTOR(NativePromiseCallback);
}

NativePromiseCallback::~NativePromiseCallback()
{
  MOZ_COUNT_DTOR(NativePromiseCallback);
}

void
NativePromiseCallback::Call(JSContext* aCx,
                            JS::Handle<JS::Value> aValue)
{
  if (mState == Promise::Resolved) {
    mHandler->ResolvedCallback(aCx, aValue);
    return;
  }

  if (mState == Promise::Rejected) {
    mHandler->RejectedCallback(aCx, aValue);
    return;
  }

  NS_NOTREACHED("huh?");
}

/* static */ PromiseCallback*
PromiseCallback::Factory(Promise* aNextPromise, JS::Handle<JSObject*> aGlobal,
                         AnyCallback* aCallback, Task aTask)
{
  MOZ_ASSERT(aNextPromise);

  // If we have a callback and a next resolver, we have to exec the callback and
  // then propagate the return value to the next resolver->resolve().
  if (aCallback) {
    return new WrapperPromiseCallback(aNextPromise, aGlobal, aCallback);
  }

  if (aTask == Resolve) {
    return new ResolvePromiseCallback(aNextPromise, aGlobal);
  }

  if (aTask == Reject) {
    return new RejectPromiseCallback(aNextPromise, aGlobal);
  }

  MOZ_ASSERT(false, "This should not happen");
  return nullptr;
}

} // namespace dom
} // namespace mozilla
