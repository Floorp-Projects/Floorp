/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Exceptions_h__
#define mozilla_dom_Exceptions_h__

// DOM exception throwing machinery (for both main thread and workers).

#include <stdint.h>
#include "jspubtd.h"
#include "nsIException.h"
#include "nsString.h"
#include "jsapi.h"

class nsIStackFrame;
class nsPIDOMWindowInner;
template <class T>
struct already_AddRefed;

namespace mozilla {
namespace dom {

class Exception;

// If we're throwing a DOMException and message is empty, the default
// message for the nsresult in question will be used.
bool
Throw(JSContext* cx, nsresult rv, const nsACString& message = EmptyCString());

// Create, throw and report an exception to a given window.
void
ThrowAndReport(nsPIDOMWindowInner* aWindow, nsresult aRv);

// Both signatures of ThrowExceptionObject guarantee that an exception is set on
// aCx before they return.
void
ThrowExceptionObject(JSContext* aCx, Exception* aException);
void
ThrowExceptionObject(JSContext* aCx, nsIException* aException);

// Create an exception object for the given nsresult and message. If we're
// throwing a DOMException and aMessage is empty, the default message for the
// nsresult in question will be used.
//
// This never returns null.
already_AddRefed<Exception>
CreateException(nsresult aRv, const nsACString& aMessage = EmptyCString());

// aMaxDepth can be used to define a maximal depth for the stack trace. If the
// value is -1, a default maximal depth will be selected.  Will return null if
// there is no JS stack right now.
already_AddRefed<nsIStackFrame>
GetCurrentJSStack(int32_t aMaxDepth = -1);

// Internal stuff not intended to be widely used.
namespace exceptions {

already_AddRefed<nsIStackFrame>
CreateStack(JSContext* aCx, JS::StackCapture&& aCaptureMode);

} // namespace exceptions
} // namespace dom
} // namespace mozilla

#endif
