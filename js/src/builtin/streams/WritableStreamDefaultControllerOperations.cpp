/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream default controller abstract operations. */

#include "builtin/streams/WritableStreamDefaultControllerOperations.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"  // JS_ReportErrorASCII

#include "builtin/streams/MiscellaneousOperations.h"  // js::CreateAlgorithmFromUnderlyingMethod
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "js/RootingAPI.h"                   // JS::Handle, JS::Rooted
#include "js/Value.h"                        // JS::Value
#include "vm/JSContext.h"                    // JSContext
#include "vm/Runtime.h"                      // JSAtomState

#include "vm/JSObject-inl.h"                 // js::NewObjectWithClassProto

using JS::Handle;
using JS::Rooted;
using JS::Value;

/*** 4.8. Writable stream default controller abstract operations ************/

/**
 * Streams spec, 4.8.2.
 *      SetUpWritableStreamDefaultController(stream, controller,
 *          startAlgorithm, writeAlgorithm, closeAlgorithm, abortAlgorithm,
 *          highWaterMark, sizeAlgorithm )
 *
 * The standard algorithm takes a `controller` argument which must be a new,
 * blank object. This implementation creates a new controller instead.
 *
 * In the spec, four algorithms (startAlgorithm, writeAlgorithm, closeAlgorithm,
 * abortAlgorithm) are passed as arguments to this routine. This implementation
 * passes these "algorithms" as data, using five arguments: sinkAlgorithms,
 * underlyingSink, writeMethod, closeMethod, and abortMethod. The sinkAlgorithms
 * argument tells how to interpret the other three:
 *
 * -   SinkAlgorithms::Script - We're creating a stream from a JS source.  The
 *     caller is `new WritableStream(underlyingSink)` or
 *     `JS::NewWritableDefaultStreamObject`. `underlyingSink` is the sink;
 *     `writeMethod`, `closeMethod`, and `abortMethod` are its .write, .close,
 *     and .abort methods, which the caller has already extracted and
 *     type-checked: each one must be either a callable JS object or undefined.
 *
 *     Script streams use the start/write/close/abort algorithms defined in
 *     4.8.3. SetUpWritableStreamDefaultControllerFromUnderlyingSink, which
 *     call JS methods of the underlyingSink.
 *
 * -   SinkAlgorithms::Transform - We're creating a transform stream.
 *     `underlyingSink` is a Transform object. `writeMethod`, `closeMethod, and
 *     `abortMethod` are undefined.
 *
 *     Transform streams use the write/close/abort algorithms given in
 *     5.3.2 InitializeTransformStream.
 *
 * Note: All arguments must be same-compartment with cx.  WritableStream
 * controllers are always created in the same compartment as the stream.
 */
MOZ_MUST_USE bool js::SetUpWritableStreamDefaultController(
    JSContext* cx, Handle<WritableStream*> stream, SinkAlgorithms algorithms,
    Handle<Value> underlyingSink, Handle<Value> writeMethod,
    Handle<Value> closeMethod, Handle<Value> abortMethod, double highWaterMark,
    Handle<Value> size) {
  // XXX jwalden flesh me out
  JS_ReportErrorASCII(cx, "epic fail");
  return false;
}

/**
 * Streams spec, 4.8.3.
 *      SetUpWritableStreamDefaultControllerFromUnderlyingSink( stream,
 *          underlyingSink, highWaterMark, sizeAlgorithm )
 */
MOZ_MUST_USE bool js::SetUpWritableStreamDefaultControllerFromUnderlyingSink(
    JSContext* cx, Handle<WritableStream*> stream, Handle<Value> underlyingSink,
    double highWaterMark, Handle<Value> sizeAlgorithm) {
  // Step 1: Assert: underlyingSink is not undefined.
  MOZ_ASSERT(!underlyingSink.isUndefined());

  // Step 2: Let controller be ObjectCreate(the original value of
  //         WritableStreamDefaultController's prototype property).
  // (Deferred to SetUpWritableStreamDefaultController.)

  // Step 3: Let startAlgorithm be the following steps:
  //         a. Return ? InvokeOrNoop(underlyingSink, "start",
  //                                  « controller »).
  SinkAlgorithms sinkAlgorithms = SinkAlgorithms::Script;

  // Step 4: Let writeAlgorithm be
  //         ? CreateAlgorithmFromUnderlyingMethod(underlyingSink, "write", 1,
  //                                               « controller »).
  Rooted<Value> writeMethod(cx);
  if (!CreateAlgorithmFromUnderlyingMethod(cx, underlyingSink,
                                           "WritableStream sink.write method",
                                           cx->names().write, &writeMethod)) {
    return false;
  }

  // Step 5: Let closeAlgorithm be
  //         ? CreateAlgorithmFromUnderlyingMethod(underlyingSink, "close", 0,
  //                                               « »).
  Rooted<Value> closeMethod(cx);
  if (!CreateAlgorithmFromUnderlyingMethod(cx, underlyingSink,
                                           "WritableStream sink.close method",
                                           cx->names().close, &closeMethod)) {
    return false;
  }

  // Step 6: Let abortAlgorithm be
  //         ? CreateAlgorithmFromUnderlyingMethod(underlyingSink, "abort", 1,
  //                                               « »).
  Rooted<Value> abortMethod(cx);
  if (!CreateAlgorithmFromUnderlyingMethod(cx, underlyingSink,
                                           "WritableStream sink.abort method",
                                           cx->names().abort, &abortMethod)) {
    return false;
  }

  // Step 6. Perform ? SetUpWritableStreamDefaultController(stream,
  //             controller, startAlgorithm, writeAlgorithm, closeAlgorithm,
  //             abortAlgorithm, highWaterMark, sizeAlgorithm).
  return SetUpWritableStreamDefaultController(
      cx, stream, sinkAlgorithms, underlyingSink, writeMethod, closeMethod,
      abortMethod, highWaterMark, sizeAlgorithm);
}
