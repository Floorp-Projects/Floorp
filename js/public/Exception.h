/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef js_Exception_h
#define js_Exception_h

#include "mozilla/Attributes.h"

#include "jspubtd.h"
#include "js/RootingAPI.h"  // JS::{Handle,Rooted}
#include "js/Value.h"       // JS::Value, JS::Handle<JS::Value>

namespace JS {

// This class encapsulates a (pending) exception and the corresponding optional
// SavedFrame stack object captured when the pending exception was set
// on the JSContext. This fuzzily correlates with a `throw` statement in JS,
// although arbitrary JSAPI consumers or VM code may also set pending exceptions
// via `JS_SetPendingException`.
//
// This is not the same stack as `e.stack` when `e` is an `Error` object.
// (That would be JS::ExceptionStackOrNull).
class MOZ_STACK_CLASS ExceptionStack {
  Rooted<Value> exception_;
  Rooted<JSObject*> stack_;

  friend JS_PUBLIC_API bool GetPendingExceptionStack(
      JSContext* cx, JS::ExceptionStack* exceptionStack);

  void init(HandleValue exception, HandleObject stack) {
    exception_ = exception;
    stack_ = stack;
  }

 public:
  explicit ExceptionStack(JSContext* cx) : exception_(cx), stack_(cx) {}

  ExceptionStack(JSContext* cx, HandleValue exception, HandleObject stack)
      : exception_(cx, exception), stack_(cx, stack) {}

  HandleValue exception() const { return exception_; }

  // |stack| can be null.
  HandleObject stack() const { return stack_; }
};

// Get the current pending exception value and stack.
// This function asserts that there is a pending exception.
// If this function returns false, then retrieving the current pending exception
// failed and might have been overwritten by a new exception.
extern JS_PUBLIC_API bool GetPendingExceptionStack(
    JSContext* cx, JS::ExceptionStack* exceptionStack);

// Similar to GetPendingExceptionStack, but also clears the current
// pending exception.
extern JS_PUBLIC_API bool StealPendingExceptionStack(
    JSContext* cx, JS::ExceptionStack* exceptionStack);

// Set both the exception value and its associated stack on the context as
// the current pending exception.
extern JS_PUBLIC_API void SetPendingExceptionStack(
    JSContext* cx, const JS::ExceptionStack& exceptionStack);

}  // namespace JS

#endif  // js_Exception_h
