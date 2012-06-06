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
 *   JSObject *thisObj;
 *   if (!NonGenericMethodGuard(cx, args, clasp, &thisObj))
 *     return false;
 *   if (!thisObj)
 *     return true;
 *
 * Specifically: if args.thisv is a proxy, NonGenericMethodGuard will call its
 * ProxyHandler's nativeCall hook (which may recursively call args.callee in
 * args.thisv's compartment). These are the possible post-conditions:
 *
 *   1. NonGenericMethodGuard returned false because it encountered an error:
 *      args.thisv wasn't an object, was an object of the wrong class, was a
 *      proxy to an object of the wrong class, or was a proxy to an object of
 *      the right class but the recursive call to args.callee failed. This case
 *      should be handled like any other failure: propagate it, or catch it and
 *      continue.
 *   2. NonGenericMethodGuard returned true, and thisObj was nulled out.  In
 *      this case args.thisv was a proxy to an object with the desired class,
 *      and recursive invocation of args.callee succeeded. This completes the
 *      invocation of args.callee, so return true.
 *   3. NonGenericMethodGuard returned true, and thisObj was set to a non-null
 *      pointer. In this case args.thisv was exactly an object of the desired
 *      class, and not a proxy to one. Finish up the call using thisObj as the
 *      this object provided to the call, which will have clasp as its class.
 *
 * Be careful! This guard may reenter the native, so the guard must be placed
 * before any effectful operations are performed.
 */
inline bool
NonGenericMethodGuard(JSContext *cx, CallArgs args, Native native, Class *clasp, JSObject **thisObj);

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
