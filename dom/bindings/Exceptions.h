/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_Exceptions_h__
#define mozilla_dom_Exceptions_h__

// DOM exception throwing machinery (for both main thread and workers).

#include <stdint.h>
#include "jspubtd.h"
#include "nsIException.h"

class nsIStackFrame;
template <class T>
class already_AddRefed;

namespace mozilla {
namespace dom {

class Exception;

bool
Throw(JSContext* cx, nsresult rv, const char* sz = nullptr);

bool
ThrowExceptionObject(JSContext* aCx, Exception* aException);

bool
ThrowExceptionObject(JSContext* aCx, nsIException* aException);

already_AddRefed<nsIStackFrame>
GetCurrentJSStack();

// Internal stuff not intended to be widely used.
namespace exceptions {

// aMaxDepth can be used to define a maximal depth for the stack trace. If the
// value is -1, a default maximal depth will be selected.
already_AddRefed<nsIStackFrame>
CreateStack(JSContext* aCx, int32_t aMaxDepth = -1);

already_AddRefed<nsIStackFrame>
CreateStackFrameLocation(uint32_t aLanguage,
                         const char* aFilename,
                         const char* aFunctionName,
                         int32_t aLineNumber,
                         nsIStackFrame* aCaller);

} // namespace exceptions
} // namespace dom
} // namespace mozilla

#endif
