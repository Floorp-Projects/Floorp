/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PromiseCallback.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseNativeHandler.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "jswrapper.h"

namespace mozilla {
namespace dom {

#ifndef SPIDERMONKEY_PROMISE

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

  JS::ExposeObjectToActiveJS(mGlobal);
  JS::ExposeValueToActiveJS(aValue);

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

  JS::ExposeObjectToActiveJS(mGlobal);
  JS::ExposeValueToActiveJS(aValue);

  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return NS_ERROR_FAILURE;
  }


  mPromise->RejectInternal(aCx, value);
  return NS_OK;
}

// InvokePromiseFuncCallback

NS_IMPL_CYCLE_COLLECTION_CLASS(InvokePromiseFuncCallback)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(InvokePromiseFuncCallback,
                                                PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mPromiseFunc)
  tmp->mGlobal = nullptr;
  tmp->mNextPromiseObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(InvokePromiseFuncCallback,
                                                  PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mPromiseFunc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(InvokePromiseFuncCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mNextPromiseObj)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(InvokePromiseFuncCallback)
NS_INTERFACE_MAP_END_INHERITING(PromiseCallback)

NS_IMPL_ADDREF_INHERITED(InvokePromiseFuncCallback, PromiseCallback)
NS_IMPL_RELEASE_INHERITED(InvokePromiseFuncCallback, PromiseCallback)

InvokePromiseFuncCallback::InvokePromiseFuncCallback(JS::Handle<JSObject*> aGlobal,
                                                     JS::Handle<JSObject*> aNextPromiseObj,
                                                     AnyCallback* aPromiseFunc)
  : mGlobal(aGlobal)
  , mNextPromiseObj(aNextPromiseObj)
  , mPromiseFunc(aPromiseFunc)
{
  MOZ_ASSERT(aGlobal);
  MOZ_ASSERT(aNextPromiseObj);
  MOZ_ASSERT(aPromiseFunc);
  HoldJSObjects(this);
}

InvokePromiseFuncCallback::~InvokePromiseFuncCallback()
{
  DropJSObjects(this);
}

nsresult
InvokePromiseFuncCallback::Call(JSContext* aCx,
                                JS::Handle<JS::Value> aValue)
{
  // Run resolver's algorithm with value and the synchronous flag set.

  JS::ExposeObjectToActiveJS(mGlobal);
  JS::ExposeValueToActiveJS(aValue);

  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;
  JS::Rooted<JS::Value> ignored(aCx);
  mPromiseFunc->Call(value, &ignored, rv);
  // Useful exceptions already got reported.
  rv.SuppressException();
  return NS_OK;
}

Promise*
InvokePromiseFuncCallback::GetDependentPromise()
{
  Promise* promise;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(Promise, mNextPromiseObj, promise))) {
    return promise;
  }

  // Oh, well.
  return nullptr;
}

// WrapperPromiseCallback
NS_IMPL_CYCLE_COLLECTION_CLASS(WrapperPromiseCallback)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(WrapperPromiseCallback,
                                                PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mNextPromise)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mResolveFunc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mRejectFunc)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mCallback)
  tmp->mGlobal = nullptr;
  tmp->mNextPromiseObj = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(WrapperPromiseCallback,
                                                  PromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mNextPromise)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mResolveFunc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRejectFunc)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mCallback)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(WrapperPromiseCallback)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mNextPromiseObj)
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

WrapperPromiseCallback::WrapperPromiseCallback(JS::Handle<JSObject*> aGlobal,
                                               AnyCallback* aCallback,
                                               JS::Handle<JSObject*> aNextPromiseObj,
                                               AnyCallback* aResolveFunc,
                                               AnyCallback* aRejectFunc)
  : mNextPromiseObj(aNextPromiseObj)
  , mResolveFunc(aResolveFunc)
  , mRejectFunc(aRejectFunc)
  , mGlobal(aGlobal)
  , mCallback(aCallback)
{
  MOZ_ASSERT(mNextPromiseObj);
  MOZ_ASSERT(aResolveFunc);
  MOZ_ASSERT(aRejectFunc);
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
  JS::ExposeObjectToActiveJS(mGlobal);
  JS::ExposeValueToActiveJS(aValue);

  JSAutoCompartment ac(aCx, mGlobal);
  JS::Rooted<JS::Value> value(aCx, aValue);
  if (!JS_WrapValue(aCx, &value)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return NS_ERROR_FAILURE;
  }

  ErrorResult rv;

  // PromiseReactionTask step 6
  JS::Rooted<JS::Value> retValue(aCx);
  JSCompartment* compartment;
  if (mNextPromise) {
    compartment = mNextPromise->Compartment();
  } else {
    MOZ_ASSERT(mNextPromiseObj);
    compartment = js::GetObjectCompartment(mNextPromiseObj);
  }
  mCallback->Call(value, &retValue, rv, "promise callback",
                  CallbackObject::eRethrowExceptions,
                  compartment);

  rv.WouldReportJSException();

  // PromiseReactionTask step 7
  if (rv.Failed()) {
    if (rv.IsUncatchableException()) {
      // We have nothing to resolve/reject the promise with.
      return rv.StealNSResult();
    }

    JS::Rooted<JS::Value> value(aCx);
    { // Scope for JSAutoCompartment
      // Convert the ErrorResult to a JS exception object that we can reject
      // ourselves with.  This will be exactly the exception that would get
      // thrown from a binding method whose ErrorResult ended up with whatever
      // is on "rv" right now.  Do this in the promise reflector compartment.
      Maybe<JSAutoCompartment> ac;
      if (mNextPromise) {
        ac.emplace(aCx, mNextPromise->GlobalJSObject());
      } else {
        ac.emplace(aCx, mNextPromiseObj);
      }
      DebugOnly<bool> conversionResult = ToJSValue(aCx, rv, &value);
      MOZ_ASSERT(conversionResult);
    }

    if (mNextPromise) {
      mNextPromise->RejectInternal(aCx, value);
    } else {
      JS::Rooted<JS::Value> ignored(aCx);
      ErrorResult rejectRv;
      mRejectFunc->Call(value, &ignored, rejectRv);
      // This reported any JS exceptions; we just have a pointless exception on
      // there now.
      rejectRv.SuppressException();
    }
    return NS_OK;
  }

  // If the return value is the same as the promise itself, throw TypeError.
  if (retValue.isObject()) {
    JS::Rooted<JSObject*> valueObj(aCx, &retValue.toObject());
    valueObj = js::CheckedUnwrap(valueObj);
    JS::Rooted<JSObject*> nextPromiseObj(aCx);
    if (mNextPromise) {
      nextPromiseObj = mNextPromise->GetWrapper();
    } else {
      MOZ_ASSERT(mNextPromiseObj);
      nextPromiseObj = mNextPromiseObj;
    }
    // XXXbz shouldn't this check be over in ResolveInternal anyway?
    if (valueObj == nextPromiseObj) {
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
      if (!JS::CreateError(aCx, JSEXN_TYPEERR, nullptr, fn, lineNumber, 0,
                           nullptr, message, &typeError)) {
        // Out of memory. Promise will stay unresolved.
        JS_ClearPendingException(aCx);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      if (mNextPromise) {
        mNextPromise->RejectInternal(aCx, typeError);
      } else {
        JS::Rooted<JS::Value> ignored(aCx);
        ErrorResult rejectRv;
        mRejectFunc->Call(typeError, &ignored, rejectRv);
        // This reported any JS exceptions; we just have a pointless exception
        // on there now.
        rejectRv.SuppressException();
      }
      return NS_OK;
    }
  }

  // Otherwise, run resolver's resolve with value.
  if (!JS_WrapValue(aCx, &retValue)) {
    NS_WARNING("Failed to wrap value into the right compartment.");
    return NS_ERROR_FAILURE;
  }

  if (mNextPromise) {
    mNextPromise->ResolveInternal(aCx, retValue);
  } else {
    JS::Rooted<JS::Value> ignored(aCx);
    ErrorResult resolveRv;
    mResolveFunc->Call(retValue, &ignored, resolveRv);
    // This reported any JS exceptions; we just have a pointless exception
    // on there now.
    resolveRv.SuppressException();
  }
    
  return NS_OK;
}

Promise*
WrapperPromiseCallback::GetDependentPromise()
{
  // Per spec, various algorithms like all() and race() are actually implemented
  // in terms of calling then() but passing it the resolve/reject functions that
  // are passed as arguments to function passed to the Promise constructor.
  // That will cause the promise in question to hold on to a
  // WrapperPromiseCallback, but the dependent promise should really be the one
  // whose constructor those functions came from, not the about-to-be-ignored
  // return value of "then".  So try to determine whether we're in that case and
  // if so go ahead and dig the dependent promise out of the function we have.
  JSObject* callable = mCallback->Callable();
  // Unwrap it, in case it's a cross-compartment wrapper.  Our caller here is
  // system, so it's really ok to just go and unwrap.
  callable = js::UncheckedUnwrap(callable);
  if (JS_IsNativeFunction(callable, Promise::JSCallback)) {
    JS::Value promiseVal =
      js::GetFunctionNativeReserved(callable, Promise::SLOT_PROMISE);
    Promise* promise;
    UNWRAP_OBJECT(Promise, &promiseVal.toObject(), promise);
    return promise;
  }

  if (mNextPromise) {
    return mNextPromise;
  }

  Promise* promise;
  if (NS_SUCCEEDED(UNWRAP_OBJECT(Promise, mNextPromiseObj, promise))) {
    return promise;
  }

  // Oh, well.
  return nullptr;
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
  JS::ExposeValueToActiveJS(aValue);

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

#endif // SPIDERMONKEY_PROMISE

} // namespace dom
} // namespace mozilla
