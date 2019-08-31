/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Miscellaneous operations. */

#ifndef builtin_streams_MiscellaneousOperations_h
#define builtin_streams_MiscellaneousOperations_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/CallArgs.h"    // JS::CallArgs
#include "js/RootingAPI.h"  // JS::{,Mutable}Handle
#include "js/Value.h"       // JS::Value
#include "vm/JSObject.h"    // JSObject

struct JSContext;

namespace js {

class PropertyName;

extern MOZ_MUST_USE JSObject* PromiseRejectedWithPendingError(JSContext* cx);

inline MOZ_MUST_USE bool ReturnPromiseRejectedWithPendingError(
    JSContext* cx, const JS::CallArgs& args) {
  JSObject* promise = PromiseRejectedWithPendingError(cx);
  if (!promise) {
    return false;
  }

  args.rval().setObject(*promise);
  return true;
}

/**
 * Streams spec, 6.3.1.
 *      CreateAlgorithmFromUnderlyingMethod ( underlyingObject, methodName,
 *                                            algoArgCount, extraArgs )
 *
 * This function only partly implements the standard algorithm. We do not
 * actually create a new JSFunction completely encapsulating the new algorithm.
 * Instead, this just gets the specified method and checks for errors. It's the
 * caller's responsibility to make sure that later, when the algorithm is
 * "performed", the appropriate steps are carried out.
 */
extern MOZ_MUST_USE bool CreateAlgorithmFromUnderlyingMethod(
    JSContext* cx, JS::Handle<JS::Value> underlyingObject,
    const char* methodNameForErrorMessage, JS::Handle<PropertyName*> methodName,
    JS::MutableHandle<JS::Value> method);

/**
 * Streams spec, 6.3.2. InvokeOrNoop ( O, P, args )
 * As it happens, all callers pass exactly one argument.
 */
extern MOZ_MUST_USE bool InvokeOrNoop(JSContext* cx, JS::Handle<JS::Value> O,
                                      JS::Handle<PropertyName*> P,
                                      JS::Handle<JS::Value> arg,
                                      JS::MutableHandle<JS::Value> rval);

/**
 * Streams spec, 6.3.5. PromiseCall ( F, V, args )
 * As it happens, all callers pass exactly one argument.
 */
extern MOZ_MUST_USE JSObject* PromiseCall(JSContext* cx,
                                          JS::Handle<JS::Value> F,
                                          JS::Handle<JS::Value> V,
                                          JS::Handle<JS::Value> arg);

/**
 * Streams spec, 6.3.7. ValidateAndNormalizeHighWaterMark ( highWaterMark )
 */
extern MOZ_MUST_USE bool ValidateAndNormalizeHighWaterMark(
    JSContext* cx, JS::Handle<JS::Value> highWaterMarkVal,
    double* highWaterMark);

/**
 * Streams spec, 6.3.8. MakeSizeAlgorithmFromSizeFunction ( size )
 *
 * The standard makes a big deal of turning JavaScript functions (grubby,
 * touched by users, covered with germs) into algorithms (pristine,
 * respectable, purposeful). We don't bother. Here we only check for errors and
 * leave `size` unchanged. Then, in ReadableStreamDefaultControllerEnqueue,
 * where this value is used, we have to check for undefined and behave as if we
 * had "made" an "algorithm" as described below.
 */
extern MOZ_MUST_USE bool MakeSizeAlgorithmFromSizeFunction(
    JSContext* cx, JS::Handle<JS::Value> size);

template <class T>
inline bool IsMaybeWrapped(const JS::Handle<JS::Value> v) {
  return v.isObject() && v.toObject().canUnwrapAs<T>();
}

}  // namespace js

#endif  // builtin_streams_MiscellaneousOperations_h
