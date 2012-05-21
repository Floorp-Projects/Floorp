/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Method prologue type-checking and unwrapping of the this parameter. */

#ifndef MethodGuard_h___
#define MethodGuard_h___

#include "jsobj.h"

namespace js {

/*
 * Report an error if call.thisv is not compatible with the specified class.
 *
 * NB: most callers should be calling or NonGenericMethodGuard,
 * HandleNonGenericMethodClassMismatch, or BoxedPrimitiveMethodGuard (so that
 * transparent proxies are handled correctly). Thus, any caller of this
 * function better have a good explanation for why proxies are being handled
 * correctly (e.g., by IsCallable) or are not an issue (E4X).
 */
extern void
ReportIncompatibleMethod(JSContext *cx, CallReceiver call, Class *clasp);

/*
 * A non-generic method is specified to report an error if args.thisv is not an
 * object with a specific [[Class]] internal property (ES5 8.6.2).
 * NonGenericMethodGuard performs this checking. Canonical usage is:
 *
 *   CallArgs args = ...
 *   bool ok;
 *   JSObject *thisObj = NonGenericMethodGuard(cx, args, clasp, &ok);
 *   if (!thisObj)
 *     return ok;
 *
 * Specifically: if obj is a proxy, NonGenericMethodGuard will call the
 * object's ProxyHandler's nativeCall hook (which may recursively call
 * args.callee in args.thisv's compartment). Thus, there are three possible
 * post-conditions:
 *
 *   1. thisv is an object of the given clasp: the caller may proceed;
 *
 *   2. there was an error: the caller must return 'false';
 *
 *   3. thisv wrapped an object of the given clasp and the native was reentered
 *      and completed succesfully: the caller must return 'true'.
 *
 * Case 1 is indicated by a non-NULL return value; case 2 by a NULL return
 * value with *ok == false; and case 3 by a NULL return value with *ok == true.
 *
 * NB: since this guard may reenter the native, the guard must be placed before
 * any effectful operations are performed.
 */
inline JSObject *
NonGenericMethodGuard(JSContext *cx, CallArgs args, Native native, Class *clasp, bool *ok);

/*
 * NonGenericMethodGuard tests args.thisv's class using 'clasp'. If more than
 * one class is acceptable (viz., isDenseArray() || isSlowArray()), the caller
 * may test the class and delegate to HandleNonGenericMethodClassMismatch to
 * handle the proxy case and error reporting. The 'clasp' argument is only used
 * for error reporting (clasp->name).
 */
extern bool
HandleNonGenericMethodClassMismatch(JSContext *cx, CallArgs args, Native native, Class *clasp);

/*
 * Implement the extraction of a primitive from a value as needed for the
 * toString, valueOf, and a few other methods of the boxed primitives classes
 * Boolean, Number, and String (e.g., ES5 15.6.4.2). If 'true' is returned, the
 * extracted primitive is stored in |*v|. If 'false' is returned, the caller
 * must immediately 'return *ok'. For details, see NonGenericMethodGuard.
 */
template <typename T>
inline bool
BoxedPrimitiveMethodGuard(JSContext *cx, CallArgs args, Native native, T *v, bool *ok);

} /* namespace js */

#endif /* MethodGuard_h___ */
