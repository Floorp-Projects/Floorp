/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PromiseCallback.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"

#include "jsapi.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTING_ADDREF(PromiseCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PromiseCallback)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PromiseCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_0(PromiseCallback)

PromiseCallback::PromiseCallback()
{
}

PromiseCallback::~PromiseCallback()
{
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
  HoldJSObjects(this);
}

ResolvePromiseCallback::~ResolvePromiseCallback()
{
  DropJSObjects(this);
}

nsresult
ResolvePromiseCallback::Call(JSContext* aCx,
                             JS::Handle<JS::Value> aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.

  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return NS_ERROR_FAILURE;
  }

  mPromise->ResolveInternal(aCx, value);
  return NS_OK;
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
  HoldJSObjects(this);
}

RejectPromiseCallback::~RejectPromiseCallback()
{
  DropJSObjects(this);
}

nsresult
RejectPromiseCallback::Call(JSContext* aCx,
                            JS::Handle<JS::Value> aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.

  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return NS_ERROR_FAILURE;
  }


  mPromise->RejectInternal(aCx, value);
  return NS_OK;
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
  HoldJSObjects(this);
}

WrapperPromiseCallback::~WrapperPromiseCallback()
{
  DropJSObjects(this);
}

nsresult
WrapperPromiseCallback::Call(JSContext* aCx,
                             JS::Handle<JS::Value> aValue)
{
  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;

  // PromiseReactionTask step 6
  JS::Rooted<JS::Value> retValue(aCx);
  mCallback->Call(value, &retValue, rv, "promise callback",
                  CallbackObject::eRethrowExceptions,
                  mNextPromise->Compartment());

  rv.WouldReportJSException();

  // PromiseReactionTask step 7
  if (rv.Failed()) {
    JS::Rooted<JS::Value> value(aCx);
    if (rv.IsJSException()) {
      rv.StealJSException(aCx, &value);

      if (!JS_WrapValue(aCx, &value)) {
        NS_WARNING("Failed to wrap value into the right compartment.");
        return NS_ERROR_FAILURE;
      }
    } else {
      // Convert the ErrorResult to a JS exception object that we can reject
      // ourselves with.  This will be exactly the exception that would get
      // thrown from a binding method whose ErrorResult ended up with whatever
      // is on "rv" right now.
      JSAutoCompartment ac(aCx, mNextPromise->GlobalJSObject());
      DebugOnly<bool> conversionResult = ToJSValue(aCx, rv, &value);
      MOZ_ASSERT(conversionResult);
    }

    mNextPromise->RejectInternal(aCx, value);
    return NS_OK;
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
      JS::Rooted<JSString*> fn(aCx, JS_NewStringCopyZ(aCx, fileName));
      if (!fn) {
        // Out of memory. Promise will stay unresolved.
        JS_ClearPendingException(aCx);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      JS::Rooted<JSString*> message(aCx,
        JS_NewStringCopyZ(aCx,
          "then() cannot return same Promise that it resolves."));
      if (!message) {
        // Out of memory. Promise will stay unresolved.
        JS_ClearPendingException(aCx);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      JS::Rooted<JS::Value> typeError(aCx);
      if (!JS::CreateError(aCx, JSEXN_TYPEERR, JS::NullPtr(), fn, lineNumber, 0,
                           nullptr, message, &typeError)) {
        // Out of memory. Promise will stay unresolved.
        JS_ClearPendingException(aCx);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      mNextPromise->RejectInternal(aCx, typeError);
      return NS_OK;
    }
  }

  // Otherwise, run resolver's resolve with value.
  if (!JS_WrapValue(aCx, &retValue)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return NS_ERROR_FAILURE;
  }

  mNextPromise->ResolveInternal(aCx, retValue);
  return NS_OK;
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
}

NativePromiseCallback::~NativePromiseCallback()
{
}

nsresult
NativePromiseCallback::Call(JSContext* aCx,
                            JS::Handle<JS::Value> aValue)
{
  if (mState == Promise::Resolved) {
    mHandler->ResolvedCallback(aCx, aValue);
    return NS_OK;
  }

  if (mState == Promise::Rejected) {
    mHandler->RejectedCallback(aCx, aValue);
    return NS_OK;
  }

  NS_NOTREACHED("huh?");
  return NS_ERROR_FAILURE;
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
