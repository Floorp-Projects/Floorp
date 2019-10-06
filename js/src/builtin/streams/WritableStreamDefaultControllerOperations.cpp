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

#include "builtin/Promise.h"  // js::PromiseObject
#include "builtin/streams/MiscellaneousOperations.h"  // js::CreateAlgorithmFromUnderlyingMethod, js::InvokeOrNoop
#include "builtin/streams/QueueWithSizes.h"  // js::ResetQueue
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "builtin/streams/WritableStreamDefaultController.h"  // js::WritableStreamDefaultController
#include "builtin/streams/WritableStreamOperations.h"  // js::WritableStream{DealWithRejection,FinishErroring,UpdateBackpressure}
#include "js/CallArgs.h"    // JS::CallArgs{,FromVp}
#include "js/RootingAPI.h"  // JS::Handle, JS::Rooted
#include "js/Value.h"       // JS::{,Object}Value
#include "vm/JSContext.h"   // JSContext
#include "vm/JSObject.h"    // JSObject
#include "vm/List.h"        // js::ListObject
#include "vm/Runtime.h"     // JSAtomState

#include "builtin/streams/HandlerFunction-inl.h"  // js::TargetFromHandler
#include "vm/JSObject-inl.h"  // js::NewBuiltinClassInstance, js::NewObjectWithClassProto

using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::Handle;
using JS::ObjectValue;
using JS::Rooted;
using JS::Value;

using js::ListObject;
using js::WritableStream;
using js::WritableStreamDefaultController;
using js::WritableStreamFinishErroring;

/*** 4.8. Writable stream default controller abstract operations ************/

static MOZ_MUST_USE bool WritableStreamDefaultControllerAdvanceQueueIfNeeded(
    JSContext* cx,
    Handle<WritableStreamDefaultController*> unwrappedController);

/**
 * Streams spec, 4.8.2. SetUpWritableStreamDefaultController, step 16:
 *      Upon fulfillment of startPromise, [...]
 */
bool js::WritableStreamControllerStartHandler(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<WritableStreamDefaultController*> unwrappedController(
      cx, TargetFromHandler<WritableStreamDefaultController>(args));

  // Step a: Assert: stream.[[state]] is "writable" or "erroring".
#ifdef DEBUG
  const auto* unwrappedStream = unwrappedController->stream();
  MOZ_ASSERT(unwrappedStream->writable() ^ unwrappedStream->erroring());
#endif

  // Step b: Set controller.[[started]] to true.
  unwrappedController->setStarted();

  // Step c: Perform
  //      ! WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller).
  if (!WritableStreamDefaultControllerAdvanceQueueIfNeeded(
          cx, unwrappedController)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/**
 * Streams spec, 4.8.2. SetUpWritableStreamDefaultController, step 17:
 *      Upon rejection of startPromise with reason r, [...]
 */
bool js::WritableStreamControllerStartFailedHandler(JSContext* cx,
                                                    unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<WritableStreamDefaultController*> unwrappedController(
      cx, TargetFromHandler<WritableStreamDefaultController>(args));

  Rooted<WritableStream*> unwrappedStream(cx, unwrappedController->stream());

  // Step a: Assert: stream.[[state]] is "writable" or "erroring".
  MOZ_ASSERT(unwrappedStream->writable() ^ unwrappedStream->erroring());

  // Step b: Set controller.[[started]] to true.
  unwrappedController->setStarted();

  // Step c: Perform ! WritableStreamDealWithRejection(stream, r).
  if (!WritableStreamDealWithRejection(cx, unwrappedStream, args.get(0))) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

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
 * An additional sizeAlgorithm in the spec is an algorithm used to compute the
 * size of a chunk.  Per MakeSizeAlgorithmFromSizeFunction, we just save the
 * |size| value used to create that algorithm, then -- inline -- perform the
 * requisite algorithm steps.  (Hence the unadorned name |size|.)
 *
 * Note: All arguments must be same-compartment with cx.  WritableStream
 * controllers are always created in the same compartment as the stream.
 */
MOZ_MUST_USE bool js::SetUpWritableStreamDefaultController(
    JSContext* cx, Handle<WritableStream*> stream,
    SinkAlgorithms sinkAlgorithms, Handle<Value> underlyingSink,
    Handle<Value> writeMethod, Handle<Value> closeMethod,
    Handle<Value> abortMethod, double highWaterMark, Handle<Value> size) {
  cx->check(stream, underlyingSink, size);
  MOZ_ASSERT(writeMethod.isUndefined() || IsCallable(writeMethod));
  MOZ_ASSERT(closeMethod.isUndefined() || IsCallable(closeMethod));
  MOZ_ASSERT(abortMethod.isUndefined() || IsCallable(abortMethod));
  MOZ_ASSERT(highWaterMark >= 0);
  MOZ_ASSERT(size.isUndefined() || IsCallable(size));

  // Done elsewhere in the standard: Create the new controller.
  Rooted<WritableStreamDefaultController*> controller(
      cx, NewBuiltinClassInstance<WritableStreamDefaultController>(cx));
  if (!controller) {
    return false;
  }

  // Step 1: Assert: ! IsWritableStream(stream) is true.
  // (guaranteed by |stream|'s type)

  // Step 2: Assert: stream.[[writableStreamController]] is undefined.
  MOZ_ASSERT(!stream->hasController());

  // Step 3: Set controller.[[controlledWritableStream]] to stream.
  controller->setStream(stream);

  // Step 4: Set stream.[[writableStreamController]] to controller.
  stream->setController(controller);

  // Step 5: Perform ! ResetQueue(controller).
  if (!ResetQueue(cx, controller)) {
    return false;
  }

  // Step 6: Set controller.[[started]] to false.
  controller->setFlags(0);
  MOZ_ASSERT(!controller->started());

  // Step 7: Set controller.[[strategySizeAlgorithm]] to sizeAlgorithm.
  controller->setStrategySize(size);

  // Step 8: Set controller.[[strategyHWM]] to highWaterMark.
  controller->setStrategyHWM(highWaterMark);

  // Step 9: Set controller.[[writeAlgorithm]] to writeAlgorithm.
  // Step 10: Set controller.[[closeAlgorithm]] to closeAlgorithm.
  // Step 11: Set controller.[[abortAlgorithm]] to abortAlgorithm.
  controller->setWriteMethod(writeMethod);
  controller->setCloseMethod(closeMethod);
  controller->setAbortMethod(abortMethod);

  // Step 12: Let backpressure be
  //          ! WritableStreamDefaultControllerGetBackpressure(controller).
  bool backpressure =
      WritableStreamDefaultControllerGetBackpressure(controller);

  // Step 13: Perform ! WritableStreamUpdateBackpressure(stream, backpressure).
  if (!WritableStreamUpdateBackpressure(cx, stream, backpressure)) {
    return false;
  }

  // Step 14: Let startResult be the result of performing startAlgorithm. (This
  //          may throw an exception.)
  Rooted<Value> startResult(cx);
  if (sinkAlgorithms == SinkAlgorithms::Script) {
    Rooted<Value> controllerVal(cx, ObjectValue(*controller));
    if (!InvokeOrNoop(cx, underlyingSink, cx->names().start, controllerVal,
                      &startResult)) {
      return false;
    }
  }

  // Step 15: Let startPromise be a promise resolved with startResult.
  Rooted<JSObject*> startPromise(
      cx, PromiseObject::unforgeableResolve(cx, startResult));
  if (!startPromise) {
    return false;
  }

  // Step 16: Upon fulfillment of startPromise,
  //    Assert: stream.[[state]] is "writable" or "erroring".
  //    Set controller.[[started]] to true.
  //  Perform ! WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller).
  // Step 17: Upon rejection of startPromise with reason r,
  //    Assert: stream.[[state]] is "writable" or "erroring".
  //    Set controller.[[started]] to true.
  //    Perform ! WritableStreamDealWithRejection(stream, r).
  Rooted<JSObject*> onStartFulfilled(
      cx, NewHandler(cx, WritableStreamControllerStartHandler, controller));
  if (!onStartFulfilled) {
    return false;
  }
  Rooted<JSObject*> onStartRejected(
      cx,
      NewHandler(cx, WritableStreamControllerStartFailedHandler, controller));
  if (!onStartRejected) {
    return false;
  }

  return JS::AddPromiseReactions(cx, startPromise, onStartFulfilled,
                                 onStartRejected);
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

/**
 * Streams spec, 4.8.7.
 *      WritableStreamDefaultControllerGetDesiredSize ( controller )
 */
double js::WritableStreamDefaultControllerGetDesiredSize(
    const WritableStreamDefaultController* controller) {
  return controller->strategyHWM() - controller->queueTotalSize();
}

/**
 * Streams spec, 4.8.9.
 *      WritableStreamDefaultControllerAdvanceQueueIfNeeded ( controller )
 */
MOZ_MUST_USE bool WritableStreamDefaultControllerAdvanceQueueIfNeeded(
    JSContext* cx,
    Handle<WritableStreamDefaultController*> unwrappedController) {
  // Step 2: If controller.[[started]] is false, return.
  if (!unwrappedController->started()) {
    return true;
  }

  // Step 1: Let stream be controller.[[controlledWritableStream]].
  Rooted<WritableStream*> unwrappedStream(cx, unwrappedController->stream());

  // Step 3: If stream.[[inFlightWriteRequest]] is not undefined, return.
  if (!unwrappedStream->inFlightWriteRequest().isUndefined()) {
    return true;
  }

  // Step 4: Let state be stream.[[state]].
  // Step 5: Assert: state is not "closed" or "errored".
  // Step 6: If state is "erroring",
  MOZ_ASSERT(!unwrappedStream->closed());
  MOZ_ASSERT(!unwrappedStream->errored());
  if (unwrappedStream->erroring()) {
    // Step 6a: Perform ! WritableStreamFinishErroring(stream).
    // Step 6b: Return.
    return WritableStreamFinishErroring(cx, unwrappedStream);
  }

  // Step 7: If controller.[[queue]] is empty, return.
  Rooted<ListObject*> unwrappedQueue(cx, unwrappedController->queue());
  if (unwrappedQueue->length() == 0) {
    return true;
  }

  // Step 8: Let writeRecord be ! PeekQueueValue(controller).
  // Step 9: If writeRecord is "close", perform
  //         ! WritableStreamDefaultControllerProcessClose(controller).
  // Step 10: Otherwise, perform
  //          ! WritableStreamDefaultControllerProcessWrite(
  //              controller, writeRecord.[[chunk]]).
  // XXX jwalden fill me in!
  JS_ReportErrorASCII(cx, "nope");
  return false;
}

/**
 * Streams spec, 4.8.13.
 *      WritableStreamDefaultControllerGetBackpressure ( controller )
 */
bool js::WritableStreamDefaultControllerGetBackpressure(
    const WritableStreamDefaultController* controller) {
  return WritableStreamDefaultControllerGetDesiredSize(controller) <= 0.0;
}
