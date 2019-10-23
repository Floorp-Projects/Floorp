/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Miscellaneous operations. */

#ifndef builtin_streams_MiscellaneousOperations_inl_h
#define builtin_streams_MiscellaneousOperations_inl_h

#include "builtin/streams/MiscellaneousOperations.h"

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/Promise.h"      // JS::{Resolve,Reject}Promise
#include "js/RootingAPI.h"   // JS::Rooted, JS::{,Mutable}Handle
#include "js/Value.h"        // JS::UndefinedHandleValue, JS::Value
#include "vm/Compartment.h"  // JS::Compartment
#include "vm/JSContext.h"    // JSContext
#include "vm/JSObject.h"     // JSObject

#include "vm/Compartment-inl.h"  // JS::Compartment::wrap
#include "vm/JSContext-inl.h"    // JSContext::check

namespace js {

/**
 * Resolve the unwrapped promise |unwrappedPromise| with |value|.
 */
inline MOZ_MUST_USE bool ResolveUnwrappedPromiseWithValue(
    JSContext* cx, JSObject* unwrappedPromise, JS::Handle<JS::Value> value) {
  cx->check(value);

  JS::Rooted<JSObject*> promise(cx, unwrappedPromise);
  if (!cx->compartment()->wrap(cx, &promise)) {
    return false;
  }

  return JS::ResolvePromise(cx, promise, value);
}

/**
 * Resolve the unwrapped promise |unwrappedPromise| with |undefined|.
 */
inline MOZ_MUST_USE bool ResolveUnwrappedPromiseWithUndefined(
    JSContext* cx, JSObject* unwrappedPromise) {
  return ResolveUnwrappedPromiseWithValue(cx, unwrappedPromise,
                                          JS::UndefinedHandleValue);
}

/**
 * Reject the unwrapped promise |unwrappedPromise| with |error|, overwriting
 * |*unwrappedPromise| with its wrapped form.
 */
inline MOZ_MUST_USE bool RejectUnwrappedPromiseWithError(
    JSContext* cx, JS::MutableHandle<JSObject*> unwrappedPromise,
    JS::Handle<JS::Value> error) {
  cx->check(error);

  if (!cx->compartment()->wrap(cx, unwrappedPromise)) {
    return false;
  }

  return JS::RejectPromise(cx, unwrappedPromise, error);
}

/**
 * Reject the unwrapped promise |unwrappedPromise| with |error|.
 */
inline MOZ_MUST_USE bool RejectUnwrappedPromiseWithError(
    JSContext* cx, JSObject* unwrappedPromise, JS::Handle<JS::Value> error) {
  JS::Rooted<JSObject*> promise(cx, unwrappedPromise);
  return RejectUnwrappedPromiseWithError(cx, &promise, error);
}

}  // namespace js

#endif  // builtin_streams_MiscellaneousOperations_inl_h
