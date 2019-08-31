/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "builtin/Stream.h"

#include "js/Stream.h"

#include <stdint.h>  // int32_t

#include "builtin/streams/ClassSpecMacro.h"           // JS_STREAMS_CLASS_SPEC
#include "builtin/streams/MiscellaneousOperations.h"  // js::CreateAlgorithmFromUnderlyingMethod, js::InvokeOrNoop, js::IsMaybeWrapped, js::PromiseCall, js::PromiseRejectedWithPendingError
#include "builtin/streams/PullIntoDescriptor.h"       // js::PullIntoDescriptor
#include "builtin/streams/QueueWithSizes.h"  // js::{DequeueValue,EnqueueValueWithSize,ResetQueue}
#include "builtin/streams/ReadableStream.h"  // js::ReadableStream, js::SetUpExternalReadableByteStreamController, js::SetUpReadableStreamDefaultControllerFromUnderlyingSource
#include "builtin/streams/ReadableStreamInternals.h"  // js::ReadableStream{AddReadOrReadIntoRequest,CloseInternal,ControllerCancelSteps,CreateReadResult,ErrorInternal,FulfillReadOrReadIntoRequest,GetNumReadRequests,HasDefaultReader}
#include "builtin/streams/ReadableStreamOperations.h"  // js::ReadableStreamTee, js::ReadableStreamTee_Cancel, js::SetUpReadableStreamDefaultController
#include "builtin/streams/ReadableStreamReader.h"  // js::ReadableStream{,Default}Reader, js::CreateReadableStreamDefaultReader, js::ReadableStreamReaderGeneric{Cancel,Initialize,Release}, js::ReadableStreamDefaultReaderRead
#include "builtin/streams/TeeState.h"              // js::TeeState
#include "gc/Heap.h"
#include "js/ArrayBuffer.h"  // JS::NewArrayBuffer
#include "js/PropertySpec.h"
#include "vm/Interpreter.h"
#include "vm/JSContext.h"
#include "vm/SelfHosting.h"

#include "builtin/streams/HandlerFunction-inl.h"  // js::NewHandler, js::TargetFromHandler
#include "builtin/streams/ReadableStreamReader-inl.h"  // js::Unwrap{ReaderFromStream{,NoThrow},StreamFromReader}
#include "vm/Compartment-inl.h"
#include "vm/List-inl.h"  // js::ListObject, js::StoreNewListInFixedSlot
#include "vm/NativeObject-inl.h"

using namespace js;

#if 0  // disable user-defined byte streams

class ByteStreamChunk : public NativeObject
{
  private:
    enum Slots {
        Slot_Buffer = 0,
        Slot_ByteOffset,
        Slot_ByteLength,
        SlotCount
    };

  public:
    static const JSClass class_;

    ArrayBufferObject* buffer() {
        return &getFixedSlot(Slot_Buffer).toObject().as<ArrayBufferObject>();
    }
    uint32_t byteOffset() { return getFixedSlot(Slot_ByteOffset).toInt32(); }
    void SetByteOffset(uint32_t offset) {
        setFixedSlot(Slot_ByteOffset, Int32Value(offset));
    }
    uint32_t byteLength() { return getFixedSlot(Slot_ByteLength).toInt32(); }
    void SetByteLength(uint32_t length) {
        setFixedSlot(Slot_ByteLength, Int32Value(length));
    }

    static ByteStreamChunk* create(JSContext* cx, HandleObject buffer, uint32_t byteOffset,
                                   uint32_t byteLength)
    {
        Rooted<ByteStreamChunk*> chunk(cx, NewBuiltinClassInstance<ByteStreamChunk>(cx));
        if (!chunk) {
            return nullptr;
        }

        chunk->setFixedSlot(Slot_Buffer, ObjectValue(*buffer));
        chunk->setFixedSlot(Slot_ByteOffset, Int32Value(byteOffset));
        chunk->setFixedSlot(Slot_ByteLength, Int32Value(byteLength));
        return chunk;
    }
};

const JSClass ByteStreamChunk::class_ = {
    "ByteStreamChunk",
    JSCLASS_HAS_RESERVED_SLOTS(SlotCount)
};

#endif  // user-defined byte streams

/*** 3.3. ReadableStreamAsyncIteratorPrototype ******************************/

// Not implemented.

/*** 3.7. Class ReadableStreamBYOBReader ************************************/

// Not implemented.

/*** 3.9. Class ReadableStreamDefaultController *****************************/

inline static MOZ_MUST_USE bool ReadableStreamControllerCallPullIfNeeded(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedController);

/**
 * Streams spec, 3.10.11. SetUpReadableStreamDefaultController, step 11
 * and
 * Streams spec, 3.13.26. SetUpReadableByteStreamController, step 16:
 *      Upon fulfillment of startPromise, [...]
 */
static bool ControllerStartHandler(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<ReadableStreamController*> controller(
      cx, TargetFromHandler<ReadableStreamController>(args));

  // Step a: Set controller.[[started]] to true.
  controller->setStarted();

  // Step b: Assert: controller.[[pulling]] is false.
  MOZ_ASSERT(!controller->pulling());

  // Step c: Assert: controller.[[pullAgain]] is false.
  MOZ_ASSERT(!controller->pullAgain());

  // Step d: Perform
  //      ! ReadableStreamDefaultControllerCallPullIfNeeded(controller)
  //      (or ReadableByteStreamControllerCallPullIfNeeded(controller)).
  if (!ReadableStreamControllerCallPullIfNeeded(cx, controller)) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

/**
 * Streams spec, 3.10.11. SetUpReadableStreamDefaultController, step 12
 * and
 * Streams spec, 3.13.26. SetUpReadableByteStreamController, step 17:
 *      Upon rejection of startPromise with reason r, [...]
 */
static bool ControllerStartFailedHandler(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<ReadableStreamController*> controller(
      cx, TargetFromHandler<ReadableStreamController>(args));

  // Step a: Perform
  //      ! ReadableStreamDefaultControllerError(controller, r)
  //      (or ReadableByteStreamControllerError(controller, r)).
  if (!ReadableStreamControllerError(cx, controller, args.get(0))) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/**
 * Streams spec, 3.9.3.
 * new ReadableStreamDefaultController( stream, underlyingSource, size,
 *                                      highWaterMark )
 */
bool ReadableStreamDefaultController::constructor(JSContext* cx, unsigned argc,
                                                  Value* vp) {
  // Step 1: Throw a TypeError.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_BOGUS_CONSTRUCTOR,
                            "ReadableStreamDefaultController");
  return false;
}

/**
 * Streams spec, 3.9.4.1. get desiredSize
 */
static bool ReadableStreamDefaultController_desiredSize(JSContext* cx,
                                                        unsigned argc,
                                                        Value* vp) {
  // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
  //         TypeError exception.
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<ReadableStreamController*> unwrappedController(
      cx, UnwrapAndTypeCheckThis<ReadableStreamDefaultController>(
              cx, args, "get desiredSize"));
  if (!unwrappedController) {
    return false;
  }

  // 3.10.8. ReadableStreamDefaultControllerGetDesiredSize, steps 1-4.
  // 3.10.8. Step 1: Let stream be controller.[[controlledReadableStream]].
  ReadableStream* unwrappedStream = unwrappedController->stream();

  // 3.10.8. Step 2: Let state be stream.[[state]].
  // 3.10.8. Step 3: If state is "errored", return null.
  if (unwrappedStream->errored()) {
    args.rval().setNull();
    return true;
  }

  // 3.10.8. Step 4: If state is "closed", return 0.
  if (unwrappedStream->closed()) {
    args.rval().setInt32(0);
    return true;
  }

  // Step 2: Return ! ReadableStreamDefaultControllerGetDesiredSize(this).
  args.rval().setNumber(
      ReadableStreamControllerGetDesiredSizeUnchecked(unwrappedController));
  return true;
}

/**
 * Unified implementation of step 2 of 3.9.4.2 and 3.9.4.3,
 * and steps 2-3 of 3.11.4.3.
 */
MOZ_MUST_USE bool js::CheckReadableStreamControllerCanCloseOrEnqueue(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedController,
    const char* action) {
  // 3.9.4.2. close(), step 2, and
  // 3.9.4.3. enqueue(chunk), step 2:
  //      If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(this) is false,
  //      throw a TypeError exception.
  // RSDCCanCloseOrEnqueue returns false in two cases: (1)
  // controller.[[closeRequested]] is true; (2) the stream is not readable,
  // i.e. already closed or errored. This amounts to exactly the same thing as
  // 3.11.4.3 steps 2-3 below, and we want different error messages for the two
  // cases anyway.

  // 3.11.4.3. Step 2: If this.[[closeRequested]] is true, throw a TypeError
  //                   exception.
  if (unwrappedController->closeRequested()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_READABLESTREAMCONTROLLER_CLOSED, action);
    return false;
  }

  // 3.11.4.3. Step 3: If this.[[controlledReadableByteStream]].[[state]] is
  //                   not "readable", throw a TypeError exception.
  ReadableStream* unwrappedStream = unwrappedController->stream();
  if (!unwrappedStream->readable()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_READABLESTREAMCONTROLLER_NOT_READABLE,
                              action);
    return false;
  }

  return true;
}

/**
 * Streams spec, 3.9.4.2 close()
 */
static bool ReadableStreamDefaultController_close(JSContext* cx, unsigned argc,
                                                  Value* vp) {
  // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
  //         TypeError exception.
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<ReadableStreamDefaultController*> unwrappedController(
      cx, UnwrapAndTypeCheckThis<ReadableStreamDefaultController>(cx, args,
                                                                  "close"));
  if (!unwrappedController) {
    return false;
  }

  // Step 2: If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(this) is
  //         false, throw a TypeError exception.
  if (!CheckReadableStreamControllerCanCloseOrEnqueue(cx, unwrappedController,
                                                      "close")) {
    return false;
  }

  // Step 3: Perform ! ReadableStreamDefaultControllerClose(this).
  if (!ReadableStreamDefaultControllerClose(cx, unwrappedController)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/**
 * Streams spec, 3.9.4.3. enqueue ( chunk )
 */
static bool ReadableStreamDefaultController_enqueue(JSContext* cx,
                                                    unsigned argc, Value* vp) {
  // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
  //         TypeError exception.
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<ReadableStreamDefaultController*> unwrappedController(
      cx, UnwrapAndTypeCheckThis<ReadableStreamDefaultController>(cx, args,
                                                                  "enqueue"));
  if (!unwrappedController) {
    return false;
  }

  // Step 2: If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(this) is
  //         false, throw a TypeError exception.
  if (!CheckReadableStreamControllerCanCloseOrEnqueue(cx, unwrappedController,
                                                      "enqueue")) {
    return false;
  }

  // Step 3: Return ! ReadableStreamDefaultControllerEnqueue(this, chunk).
  if (!ReadableStreamDefaultControllerEnqueue(cx, unwrappedController,
                                              args.get(0))) {
    return false;
  }
  args.rval().setUndefined();
  return true;
}

/**
 * Streams spec, 3.9.4.4. error ( e )
 */
static bool ReadableStreamDefaultController_error(JSContext* cx, unsigned argc,
                                                  Value* vp) {
  // Step 1: If ! IsReadableStreamDefaultController(this) is false, throw a
  //         TypeError exception.
  CallArgs args = CallArgsFromVp(argc, vp);
  Rooted<ReadableStreamDefaultController*> unwrappedController(
      cx, UnwrapAndTypeCheckThis<ReadableStreamDefaultController>(cx, args,
                                                                  "enqueue"));
  if (!unwrappedController) {
    return false;
  }

  // Step 2: Perform ! ReadableStreamDefaultControllerError(this, e).
  if (!ReadableStreamControllerError(cx, unwrappedController, args.get(0))) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static const JSPropertySpec ReadableStreamDefaultController_properties[] = {
    JS_PSG("desiredSize", ReadableStreamDefaultController_desiredSize, 0),
    JS_PS_END};

static const JSFunctionSpec ReadableStreamDefaultController_methods[] = {
    JS_FN("close", ReadableStreamDefaultController_close, 0, 0),
    JS_FN("enqueue", ReadableStreamDefaultController_enqueue, 1, 0),
    JS_FN("error", ReadableStreamDefaultController_error, 1, 0), JS_FS_END};

JS_STREAMS_CLASS_SPEC(ReadableStreamDefaultController, 0, SlotCount,
                      ClassSpec::DontDefineConstructor, 0, JS_NULL_CLASS_OPS);

static void ReadableStreamControllerClearAlgorithms(
    Handle<ReadableStreamController*> controller);

/**
 * Unified implementation of ReadableStream controllers' [[CancelSteps]]
 * internal methods.
 * Streams spec, 3.9.5.1. [[CancelSteps]] ( reason )
 * and
 * Streams spec, 3.11.5.1. [[CancelSteps]] ( reason )
 */
MOZ_MUST_USE JSObject* js::ReadableStreamControllerCancelSteps(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedController,
    HandleValue reason) {
  AssertSameCompartment(cx, reason);

  // Step 1 of 3.11.5.1: If this.[[pendingPullIntos]] is not empty,
  if (!unwrappedController->is<ReadableStreamDefaultController>()) {
    Rooted<ListObject*> unwrappedPendingPullIntos(
        cx, unwrappedController->as<ReadableByteStreamController>()
                .pendingPullIntos());

    if (unwrappedPendingPullIntos->length() != 0) {
      // Step a: Let firstDescriptor be the first element of
      //         this.[[pendingPullIntos]].
      PullIntoDescriptor* unwrappedDescriptor =
          UnwrapAndDowncastObject<PullIntoDescriptor>(
              cx, &unwrappedPendingPullIntos->get(0).toObject());
      if (!unwrappedDescriptor) {
        return nullptr;
      }

      // Step b: Set firstDescriptor.[[bytesFilled]] to 0.
      unwrappedDescriptor->setBytesFilled(0);
    }
  }

  RootedValue unwrappedUnderlyingSource(cx);
  unwrappedUnderlyingSource = unwrappedController->underlyingSource();

  // Step 1 of 3.9.5.1, step 2 of 3.11.5.1: Perform ! ResetQueue(this).
  if (!ResetQueue(cx, unwrappedController)) {
    return nullptr;
  }

  // Step 2 of 3.9.5.1, step 3 of 3.11.5.1: Let result be the result of
  //     performing this.[[cancelAlgorithm]], passing reason.
  //
  // Our representation of cancel algorithms is a bit awkward, for
  // performance, so we must figure out which algorithm is being invoked.
  RootedObject result(cx);
  if (IsMaybeWrapped<TeeState>(unwrappedUnderlyingSource)) {
    // The cancel algorithm given in ReadableStreamTee step 13 or 14.
    MOZ_ASSERT(unwrappedUnderlyingSource.toObject().is<TeeState>(),
               "tee streams and controllers are always same-compartment with "
               "the TeeState object");
    Rooted<TeeState*> unwrappedTeeState(
        cx, &unwrappedUnderlyingSource.toObject().as<TeeState>());
    Rooted<ReadableStreamDefaultController*> unwrappedDefaultController(
        cx, &unwrappedController->as<ReadableStreamDefaultController>());
    result = ReadableStreamTee_Cancel(cx, unwrappedTeeState,
                                      unwrappedDefaultController, reason);
  } else if (unwrappedController->hasExternalSource()) {
    // An embedding-provided cancel algorithm.
    RootedValue rval(cx);
    {
      AutoRealm ar(cx, unwrappedController);
      JS::ReadableStreamUnderlyingSource* source =
          unwrappedController->externalSource();
      Rooted<ReadableStream*> stream(cx, unwrappedController->stream());
      RootedValue wrappedReason(cx, reason);
      if (!cx->compartment()->wrap(cx, &wrappedReason)) {
        return nullptr;
      }

      cx->check(stream, wrappedReason);
      rval = source->cancel(cx, stream, wrappedReason);
    }

    // Make sure the ReadableStreamControllerClearAlgorithms call below is
    // reached, even on error.
    if (!cx->compartment()->wrap(cx, &rval)) {
      result = nullptr;
    } else {
      result = PromiseObject::unforgeableResolve(cx, rval);
    }
  } else {
    // The algorithm created in
    // SetUpReadableByteStreamControllerFromUnderlyingSource step 5.
    RootedValue unwrappedCancelMethod(cx, unwrappedController->cancelMethod());
    if (unwrappedCancelMethod.isUndefined()) {
      // CreateAlgorithmFromUnderlyingMethod step 7.
      result = PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
    } else {
      // CreateAlgorithmFromUnderlyingMethod steps 6.c.i-ii.
      {
        AutoRealm ar(cx, &unwrappedCancelMethod.toObject());
        RootedValue underlyingSource(cx, unwrappedUnderlyingSource);
        if (!cx->compartment()->wrap(cx, &underlyingSource)) {
          return nullptr;
        }
        RootedValue wrappedReason(cx, reason);
        if (!cx->compartment()->wrap(cx, &wrappedReason)) {
          return nullptr;
        }

        // If PromiseCall fails, don't bail out until after the
        // ReadableStreamControllerClearAlgorithms call below.
        result = PromiseCall(cx, unwrappedCancelMethod, underlyingSource,
                             wrappedReason);
      }
      if (!cx->compartment()->wrap(cx, &result)) {
        result = nullptr;
      }
    }
  }

  // Step 3 (or 4): Perform
  //      ! ReadableStreamDefaultControllerClearAlgorithms(this)
  //      (or ReadableByteStreamControllerClearAlgorithms(this)).
  ReadableStreamControllerClearAlgorithms(unwrappedController);

  // Step 4 (or 5): Return result.
  return result;
}

/**
 * Streams spec, 3.9.5.2.
 *     ReadableStreamDefaultController [[PullSteps]]( forAuthorCode )
 */
static JSObject* ReadableStreamDefaultControllerPullSteps(
    JSContext* cx,
    Handle<ReadableStreamDefaultController*> unwrappedController) {
  // Step 1: Let stream be this.[[controlledReadableStream]].
  Rooted<ReadableStream*> unwrappedStream(cx, unwrappedController->stream());

  // Step 2: If this.[[queue]] is not empty,
  Rooted<ListObject*> unwrappedQueue(cx);
  RootedValue val(
      cx, unwrappedController->getFixedSlot(StreamController::Slot_Queue));
  if (val.isObject()) {
    unwrappedQueue = &val.toObject().as<ListObject>();
  }

  if (unwrappedQueue && unwrappedQueue->length() != 0) {
    // Step a: Let chunk be ! DequeueValue(this).
    RootedValue chunk(cx);
    if (!DequeueValue(cx, unwrappedController, &chunk)) {
      return nullptr;
    }

    // Step b: If this.[[closeRequested]] is true and this.[[queue]] is empty,
    if (unwrappedController->closeRequested() &&
        unwrappedQueue->length() == 0) {
      // Step i: Perform ! ReadableStreamDefaultControllerClearAlgorithms(this).
      ReadableStreamControllerClearAlgorithms(unwrappedController);

      // Step ii: Perform ! ReadableStreamClose(stream).
      if (!ReadableStreamCloseInternal(cx, unwrappedStream)) {
        return nullptr;
      }
    }

    // Step c: Otherwise, perform
    //         ! ReadableStreamDefaultControllerCallPullIfNeeded(this).
    else {
      if (!ReadableStreamControllerCallPullIfNeeded(cx, unwrappedController)) {
        return nullptr;
      }
    }

    // Step d: Return a promise resolved with
    //         ! ReadableStreamCreateReadResult(chunk, false, forAuthorCode).
    cx->check(chunk);
    ReadableStreamReader* unwrappedReader =
        UnwrapReaderFromStream(cx, unwrappedStream);
    if (!unwrappedReader) {
      return nullptr;
    }
    RootedObject readResultObj(
        cx, ReadableStreamCreateReadResult(cx, chunk, false,
                                           unwrappedReader->forAuthorCode()));
    if (!readResultObj) {
      return nullptr;
    }
    RootedValue readResult(cx, ObjectValue(*readResultObj));
    return PromiseObject::unforgeableResolve(cx, readResult);
  }

  // Step 3: Let pendingPromise be
  //         ! ReadableStreamAddReadRequest(stream, forAuthorCode).
  RootedObject pendingPromise(
      cx, ReadableStreamAddReadOrReadIntoRequest(cx, unwrappedStream));
  if (!pendingPromise) {
    return nullptr;
  }

  // Step 4: Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(this).
  if (!ReadableStreamControllerCallPullIfNeeded(cx, unwrappedController)) {
    return nullptr;
  }

  // Step 5: Return pendingPromise.
  return pendingPromise;
}

/*** 3.10. Readable stream default controller abstract operations ***********/

// Streams spec, 3.10.1. IsReadableStreamDefaultController ( x )
// Implemented via is<ReadableStreamDefaultController>()

/**
 * Streams spec, 3.10.2 and 3.13.3. step 7:
 *      Upon fulfillment of pullPromise, [...]
 */
static bool ControllerPullHandler(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<ReadableStreamController*> unwrappedController(
      cx, UnwrapCalleeSlot<ReadableStreamController>(cx, args, 0));
  if (!unwrappedController) {
    return false;
  }

  bool pullAgain = unwrappedController->pullAgain();

  // Step a: Set controller.[[pulling]] to false.
  // Step b.i: Set controller.[[pullAgain]] to false.
  unwrappedController->clearPullFlags();

  // Step b: If controller.[[pullAgain]] is true,
  if (pullAgain) {
    // Step ii: Perform
    //          ! ReadableStreamDefaultControllerCallPullIfNeeded(controller)
    //          (or ReadableByteStreamControllerCallPullIfNeeded(controller)).
    if (!ReadableStreamControllerCallPullIfNeeded(cx, unwrappedController)) {
      return false;
    }
  }

  args.rval().setUndefined();
  return true;
}

/**
 * Streams spec, 3.10.2 and 3.13.3. step 8:
 * Upon rejection of pullPromise with reason e,
 */
static bool ControllerPullFailedHandler(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  HandleValue e = args.get(0);

  Rooted<ReadableStreamController*> controller(
      cx, UnwrapCalleeSlot<ReadableStreamController>(cx, args, 0));
  if (!controller) {
    return false;
  }

  // Step a: Perform ! ReadableStreamDefaultControllerError(controller, e).
  //         (ReadableByteStreamControllerError in 3.12.3.)
  if (!ReadableStreamControllerError(cx, controller, e)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool ReadableStreamControllerShouldCallPull(
    ReadableStreamController* unwrappedController);

/**
 * Streams spec, 3.10.2
 *      ReadableStreamDefaultControllerCallPullIfNeeded ( controller )
 * Streams spec, 3.13.3.
 *      ReadableByteStreamControllerCallPullIfNeeded ( controller )
 */
inline static MOZ_MUST_USE bool ReadableStreamControllerCallPullIfNeeded(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedController) {
  // Step 1: Let shouldPull be
  //         ! ReadableStreamDefaultControllerShouldCallPull(controller).
  // (ReadableByteStreamDefaultControllerShouldCallPull in 3.13.3.)
  bool shouldPull = ReadableStreamControllerShouldCallPull(unwrappedController);

  // Step 2: If shouldPull is false, return.
  if (!shouldPull) {
    return true;
  }

  // Step 3: If controller.[[pulling]] is true,
  if (unwrappedController->pulling()) {
    // Step a: Set controller.[[pullAgain]] to true.
    unwrappedController->setPullAgain();

    // Step b: Return.
    return true;
  }

  // Step 4: Assert: controller.[[pullAgain]] is false.
  MOZ_ASSERT(!unwrappedController->pullAgain());

  // Step 5: Set controller.[[pulling]] to true.
  unwrappedController->setPulling();

  // We use this variable in step 7. For ease of error-handling, we wrap it
  // early.
  RootedObject wrappedController(cx, unwrappedController);
  if (!cx->compartment()->wrap(cx, &wrappedController)) {
    return false;
  }

  // Step 6: Let pullPromise be the result of performing
  //         controller.[[pullAlgorithm]].
  // Our representation of pull algorithms is a bit awkward, for performance,
  // so we must figure out which algorithm is being invoked.
  RootedObject pullPromise(cx);
  RootedValue unwrappedUnderlyingSource(
      cx, unwrappedController->underlyingSource());

  if (IsMaybeWrapped<TeeState>(unwrappedUnderlyingSource)) {
    // The pull algorithm given in ReadableStreamTee step 12.
    MOZ_ASSERT(unwrappedUnderlyingSource.toObject().is<TeeState>(),
               "tee streams and controllers are always same-compartment with "
               "the TeeState object");
    Rooted<TeeState*> unwrappedTeeState(
        cx, &unwrappedUnderlyingSource.toObject().as<TeeState>());
    pullPromise = ReadableStreamTee_Pull(cx, unwrappedTeeState);
  } else if (unwrappedController->hasExternalSource()) {
    // An embedding-provided pull algorithm.
    {
      AutoRealm ar(cx, unwrappedController);
      JS::ReadableStreamUnderlyingSource* source =
          unwrappedController->externalSource();
      Rooted<ReadableStream*> stream(cx, unwrappedController->stream());
      double desiredSize =
          ReadableStreamControllerGetDesiredSizeUnchecked(unwrappedController);
      source->requestData(cx, stream, desiredSize);
    }
    pullPromise = PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
  } else {
    // The pull algorithm created in
    // SetUpReadableStreamDefaultControllerFromUnderlyingSource step 4.
    RootedValue unwrappedPullMethod(cx, unwrappedController->pullMethod());
    if (unwrappedPullMethod.isUndefined()) {
      // CreateAlgorithmFromUnderlyingMethod step 7.
      pullPromise = PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
    } else {
      // CreateAlgorithmFromUnderlyingMethod step 6.b.i.
      {
        AutoRealm ar(cx, &unwrappedPullMethod.toObject());
        RootedValue underlyingSource(cx, unwrappedUnderlyingSource);
        if (!cx->compartment()->wrap(cx, &underlyingSource)) {
          return false;
        }
        RootedValue controller(cx, ObjectValue(*unwrappedController));
        if (!cx->compartment()->wrap(cx, &controller)) {
          return false;
        }
        pullPromise =
            PromiseCall(cx, unwrappedPullMethod, underlyingSource, controller);
        if (!pullPromise) {
          return false;
        }
      }
      if (!cx->compartment()->wrap(cx, &pullPromise)) {
        return false;
      }
    }
  }
  if (!pullPromise) {
    return false;
  }

  // Step 7: Upon fulfillment of pullPromise, [...]
  // Step 8. Upon rejection of pullPromise with reason e, [...]
  RootedObject onPullFulfilled(
      cx, NewHandler(cx, ControllerPullHandler, wrappedController));
  if (!onPullFulfilled) {
    return false;
  }
  RootedObject onPullRejected(
      cx, NewHandler(cx, ControllerPullFailedHandler, wrappedController));
  if (!onPullRejected) {
    return false;
  }
  return JS::AddPromiseReactions(cx, pullPromise, onPullFulfilled,
                                 onPullRejected);
}

/**
 * Streams spec, 3.10.3.
 *      ReadableStreamDefaultControllerShouldCallPull ( controller )
 * Streams spec, 3.13.25.
 *      ReadableByteStreamControllerShouldCallPull ( controller )
 */
static bool ReadableStreamControllerShouldCallPull(
    ReadableStreamController* unwrappedController) {
  // Step 1: Let stream be controller.[[controlledReadableStream]]
  //         (or [[controlledReadableByteStream]]).
  ReadableStream* unwrappedStream = unwrappedController->stream();

  // 3.10.3. Step 2:
  //      If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(controller)
  //      is false, return false.
  // This turns out to be the same as 3.13.25 steps 2-3.

  // 3.13.25 Step 2: If stream.[[state]] is not "readable", return false.
  if (!unwrappedStream->readable()) {
    return false;
  }

  // 3.13.25 Step 3: If controller.[[closeRequested]] is true, return false.
  if (unwrappedController->closeRequested()) {
    return false;
  }

  // Step 3 (or 4):
  //      If controller.[[started]] is false, return false.
  if (!unwrappedController->started()) {
    return false;
  }

  // 3.10.3.
  // Step 4: If ! IsReadableStreamLocked(stream) is true and
  //      ! ReadableStreamGetNumReadRequests(stream) > 0, return true.
  //
  // 3.13.25.
  // Step 5: If ! ReadableStreamHasDefaultReader(stream) is true and
  //         ! ReadableStreamGetNumReadRequests(stream) > 0, return true.
  // Step 6: If ! ReadableStreamHasBYOBReader(stream) is true and
  //         ! ReadableStreamGetNumReadIntoRequests(stream) > 0, return true.
  //
  // All of these amount to the same thing in this implementation:
  if (unwrappedStream->locked() &&
      ReadableStreamGetNumReadRequests(unwrappedStream) > 0) {
    return true;
  }

  // Step 5 (or 7):
  //      Let desiredSize be
  //      ! ReadableStreamDefaultControllerGetDesiredSize(controller).
  //      (ReadableByteStreamControllerGetDesiredSize in 3.13.25.)
  double desiredSize =
      ReadableStreamControllerGetDesiredSizeUnchecked(unwrappedController);

  // Step 6 (or 8): Assert: desiredSize is not null (implicit).
  // Step 7 (or 9): If desiredSize > 0, return true.
  // Step 8 (or 10): Return false.
  return desiredSize > 0;
}

/**
 * Streams spec, 3.10.4.
 *      ReadableStreamDefaultControllerClearAlgorithms ( controller )
 * and 3.13.4.
 *      ReadableByteStreamControllerClearAlgorithms ( controller )
 */
static void ReadableStreamControllerClearAlgorithms(
    Handle<ReadableStreamController*> controller) {
  // Step 1: Set controller.[[pullAlgorithm]] to undefined.
  // Step 2: Set controller.[[cancelAlgorithm]] to undefined.
  // (In this implementation, the UnderlyingSource slot is part of the
  // representation of these algorithms.)
  controller->setPullMethod(UndefinedHandleValue);
  controller->setCancelMethod(UndefinedHandleValue);
  ReadableStreamController::clearUnderlyingSource(controller);

  // Step 3 (of 3.10.4 only) : Set controller.[[strategySizeAlgorithm]] to
  // undefined.
  if (controller->is<ReadableStreamDefaultController>()) {
    controller->as<ReadableStreamDefaultController>().setStrategySize(
        UndefinedHandleValue);
  }
}

/**
 * Streams spec, 3.10.5. ReadableStreamDefaultControllerClose ( controller )
 */
MOZ_MUST_USE bool js::ReadableStreamDefaultControllerClose(
    JSContext* cx,
    Handle<ReadableStreamDefaultController*> unwrappedController) {
  // Step 1: Let stream be controller.[[controlledReadableStream]].
  Rooted<ReadableStream*> unwrappedStream(cx, unwrappedController->stream());

  // Step 2: Assert:
  //         ! ReadableStreamDefaultControllerCanCloseOrEnqueue(controller)
  //         is true.
  MOZ_ASSERT(!unwrappedController->closeRequested());
  MOZ_ASSERT(unwrappedStream->readable());

  // Step 3: Set controller.[[closeRequested]] to true.
  unwrappedController->setCloseRequested();

  // Step 4: If controller.[[queue]] is empty,
  Rooted<ListObject*> unwrappedQueue(cx, unwrappedController->queue());
  if (unwrappedQueue->length() == 0) {
    // Step a: Perform
    //         ! ReadableStreamDefaultControllerClearAlgorithms(controller).
    ReadableStreamControllerClearAlgorithms(unwrappedController);

    // Step b: Perform ! ReadableStreamClose(stream).
    return ReadableStreamCloseInternal(cx, unwrappedStream);
  }

  return true;
}

/**
 * Streams spec, 3.10.6.
 *      ReadableStreamDefaultControllerEnqueue ( controller, chunk )
 */
MOZ_MUST_USE bool js::ReadableStreamDefaultControllerEnqueue(
    JSContext* cx, Handle<ReadableStreamDefaultController*> unwrappedController,
    HandleValue chunk) {
  AssertSameCompartment(cx, chunk);

  // Step 1: Let stream be controller.[[controlledReadableStream]].
  Rooted<ReadableStream*> unwrappedStream(cx, unwrappedController->stream());

  // Step 2: Assert:
  //      ! ReadableStreamDefaultControllerCanCloseOrEnqueue(controller) is
  //      true.
  MOZ_ASSERT(!unwrappedController->closeRequested());
  MOZ_ASSERT(unwrappedStream->readable());

  // Step 3: If ! IsReadableStreamLocked(stream) is true and
  //         ! ReadableStreamGetNumReadRequests(stream) > 0, perform
  //         ! ReadableStreamFulfillReadRequest(stream, chunk, false).
  if (unwrappedStream->locked() &&
      ReadableStreamGetNumReadRequests(unwrappedStream) > 0) {
    if (!ReadableStreamFulfillReadOrReadIntoRequest(cx, unwrappedStream, chunk,
                                                    false)) {
      return false;
    }
  } else {
    // Step 4: Otherwise,
    // Step a: Let result be the result of performing
    //         controller.[[strategySizeAlgorithm]], passing in chunk, and
    //         interpreting the result as an ECMAScript completion value.
    // Step c: (on success) Let chunkSize be result.[[Value]].
    RootedValue chunkSize(cx, NumberValue(1));
    bool success = true;
    RootedValue strategySize(cx, unwrappedController->strategySize());
    if (!strategySize.isUndefined()) {
      if (!cx->compartment()->wrap(cx, &strategySize)) {
        return false;
      }
      success = Call(cx, strategySize, UndefinedHandleValue, chunk, &chunkSize);
    }

    // Step d: Let enqueueResult be
    //         EnqueueValueWithSize(controller, chunk, chunkSize).
    if (success) {
      success = EnqueueValueWithSize(cx, unwrappedController, chunk, chunkSize);
    }

    // Step b: If result is an abrupt completion,
    // and
    // Step e: If enqueueResult is an abrupt completion,
    if (!success) {
      RootedValue exn(cx);
      RootedSavedFrame stack(cx);
      if (!cx->isExceptionPending() ||
          !GetAndClearExceptionAndStack(cx, &exn, &stack)) {
        // Uncatchable error. Die immediately without erroring the
        // stream.
        return false;
      }

      // Step b.i: Perform ! ReadableStreamDefaultControllerError(
      //           controller, result.[[Value]]).
      // Step e.i: Perform ! ReadableStreamDefaultControllerError(
      //           controller, enqueueResult.[[Value]]).
      if (!ReadableStreamControllerError(cx, unwrappedController, exn)) {
        return false;
      }

      // Step b.ii: Return result.
      // Step e.ii: Return enqueueResult.
      // (I.e., propagate the exception.)
      cx->setPendingException(exn, stack);
      return false;
    }
  }

  // Step 5: Perform
  //         ! ReadableStreamDefaultControllerCallPullIfNeeded(controller).
  return ReadableStreamControllerCallPullIfNeeded(cx, unwrappedController);
}

static MOZ_MUST_USE bool ReadableByteStreamControllerClearPendingPullIntos(
    JSContext* cx, Handle<ReadableByteStreamController*> unwrappedController);

/**
 * Streams spec, 3.10.7. ReadableStreamDefaultControllerError ( controller, e )
 * Streams spec, 3.13.11. ReadableByteStreamControllerError ( controller, e )
 */
MOZ_MUST_USE bool js::ReadableStreamControllerError(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedController,
    HandleValue e) {
  MOZ_ASSERT(!cx->isExceptionPending());
  AssertSameCompartment(cx, e);

  // Step 1: Let stream be controller.[[controlledReadableStream]]
  //         (or controller.[[controlledReadableByteStream]]).
  Rooted<ReadableStream*> unwrappedStream(cx, unwrappedController->stream());

  // Step 2: If stream.[[state]] is not "readable", return.
  if (!unwrappedStream->readable()) {
    return true;
  }

  // Step 3 of 3.13.10:
  // Perform ! ReadableByteStreamControllerClearPendingPullIntos(controller).
  if (unwrappedController->is<ReadableByteStreamController>()) {
    Rooted<ReadableByteStreamController*> unwrappedByteStreamController(
        cx, &unwrappedController->as<ReadableByteStreamController>());
    if (!ReadableByteStreamControllerClearPendingPullIntos(
            cx, unwrappedByteStreamController)) {
      return false;
    }
  }

  // Step 3 (or 4): Perform ! ResetQueue(controller).
  if (!ResetQueue(cx, unwrappedController)) {
    return false;
  }

  // Step 4 (or 5):
  //      Perform ! ReadableStreamDefaultControllerClearAlgorithms(controller)
  //      (or ReadableByteStreamControllerClearAlgorithms(controller)).
  ReadableStreamControllerClearAlgorithms(unwrappedController);

  // Step 5 (or 6): Perform ! ReadableStreamError(stream, e).
  return ReadableStreamErrorInternal(cx, unwrappedStream, e);
}

/**
 * Streams spec, 3.10.8.
 *      ReadableStreamDefaultControllerGetDesiredSize ( controller )
 * Streams spec 3.13.14.
 *      ReadableByteStreamControllerGetDesiredSize ( controller )
 */
MOZ_MUST_USE double js::ReadableStreamControllerGetDesiredSizeUnchecked(
    ReadableStreamController* controller) {
  // Steps 1-4 done at callsites, so only assert that they have been done.
#if DEBUG
  ReadableStream* stream = controller->stream();
  MOZ_ASSERT(!(stream->errored() || stream->closed()));
#endif  // DEBUG

  // Step 5: Return controller.[[strategyHWM]] − controller.[[queueTotalSize]].
  return controller->strategyHWM() - controller->queueTotalSize();
}

/**
 * Streams spec, 3.10.11.
 *      SetUpReadableStreamDefaultController(stream, controller,
 *          startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
 *          sizeAlgorithm )
 *
 * The standard algorithm takes a `controller` argument which must be a new,
 * blank object. This implementation creates a new controller instead.
 *
 * In the spec, three algorithms (startAlgorithm, pullAlgorithm,
 * cancelAlgorithm) are passed as arguments to this routine. This
 * implementation passes these "algorithms" as data, using four arguments:
 * sourceAlgorithms, underlyingSource, pullMethod, and cancelMethod. The
 * sourceAlgorithms argument tells how to interpret the other three:
 *
 * -   SourceAlgorithms::Script - We're creating a stream from a JS source.
 *     The caller is `new ReadableStream(underlyingSource)` or
 *     `JS::NewReadableDefaultStreamObject`. `underlyingSource` is the
 *     source; `pullMethod` and `cancelMethod` are its .pull and
 *     .cancel methods, which the caller has already extracted and
 *     type-checked: each one must be either a callable JS object or undefined.
 *
 *     Script streams use the start/pull/cancel algorithms defined in
 *     3.10.12. SetUpReadableStreamDefaultControllerFromUnderlyingSource, which
 *     call JS methods of the underlyingSource.
 *
 * -   SourceAlgorithms::Tee - We're creating a tee stream. `underlyingSource`
 *     is a TeeState object. `pullMethod` and `cancelMethod` are undefined.
 *
 *     Tee streams use the start/pull/cancel algorithms given in
 *     3.4.10. ReadableStreamTee.
 *
 * Note: All arguments must be same-compartment with cx. ReadableStream
 * controllers are always created in the same compartment as the stream.
 */
MOZ_MUST_USE bool js::SetUpReadableStreamDefaultController(
    JSContext* cx, Handle<ReadableStream*> stream,
    SourceAlgorithms sourceAlgorithms, HandleValue underlyingSource,
    HandleValue pullMethod, HandleValue cancelMethod, double highWaterMark,
    HandleValue size) {
  cx->check(stream, underlyingSource, size);
  MOZ_ASSERT(pullMethod.isUndefined() || IsCallable(pullMethod));
  MOZ_ASSERT(cancelMethod.isUndefined() || IsCallable(cancelMethod));
  MOZ_ASSERT_IF(sourceAlgorithms != SourceAlgorithms::Script,
                pullMethod.isUndefined());
  MOZ_ASSERT_IF(sourceAlgorithms != SourceAlgorithms::Script,
                cancelMethod.isUndefined());
  MOZ_ASSERT(highWaterMark >= 0);
  MOZ_ASSERT(size.isUndefined() || IsCallable(size));

  // Done elsewhere in the standard: Create the new controller.
  Rooted<ReadableStreamDefaultController*> controller(
      cx, NewBuiltinClassInstance<ReadableStreamDefaultController>(cx));
  if (!controller) {
    return false;
  }

  // Step 1: Assert: stream.[[readableStreamController]] is undefined.
  MOZ_ASSERT(!stream->hasController());

  // Step 2: Set controller.[[controlledReadableStream]] to stream.
  controller->setStream(stream);

  // Step 3: Set controller.[[queue]] and controller.[[queueTotalSize]] to
  //         undefined (implicit), then perform ! ResetQueue(controller).
  if (!ResetQueue(cx, controller)) {
    return false;
  }

  // Step 4: Set controller.[[started]], controller.[[closeRequested]],
  //         controller.[[pullAgain]], and controller.[[pulling]] to false.
  controller->setFlags(0);

  // Step 5: Set controller.[[strategySizeAlgorithm]] to sizeAlgorithm
  //         and controller.[[strategyHWM]] to highWaterMark.
  controller->setStrategySize(size);
  controller->setStrategyHWM(highWaterMark);

  // Step 6: Set controller.[[pullAlgorithm]] to pullAlgorithm.
  // (In this implementation, the pullAlgorithm is determined by the
  // underlyingSource in combination with the pullMethod field.)
  controller->setUnderlyingSource(underlyingSource);
  controller->setPullMethod(pullMethod);

  // Step 7: Set controller.[[cancelAlgorithm]] to cancelAlgorithm.
  controller->setCancelMethod(cancelMethod);

  // Step 8: Set stream.[[readableStreamController]] to controller.
  stream->setController(controller);

  // Step 9: Let startResult be the result of performing startAlgorithm.
  RootedValue startResult(cx);
  if (sourceAlgorithms == SourceAlgorithms::Script) {
    RootedValue controllerVal(cx, ObjectValue(*controller));
    if (!InvokeOrNoop(cx, underlyingSource, cx->names().start, controllerVal,
                      &startResult)) {
      return false;
    }
  }

  // Step 10: Let startPromise be a promise resolved with startResult.
  RootedObject startPromise(cx,
                            PromiseObject::unforgeableResolve(cx, startResult));
  if (!startPromise) {
    return false;
  }

  // Step 11: Upon fulfillment of startPromise, [...]
  // Step 12: Upon rejection of startPromise with reason r, [...]
  RootedObject onStartFulfilled(
      cx, NewHandler(cx, ControllerStartHandler, controller));
  if (!onStartFulfilled) {
    return false;
  }
  RootedObject onStartRejected(
      cx, NewHandler(cx, ControllerStartFailedHandler, controller));
  if (!onStartRejected) {
    return false;
  }
  if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled,
                               onStartRejected)) {
    return false;
  }

  return true;
}

/**
 * Streams spec, 3.10.12.
 *      SetUpReadableStreamDefaultControllerFromUnderlyingSource( stream,
 *          underlyingSource, highWaterMark, sizeAlgorithm )
 */
MOZ_MUST_USE bool js::SetUpReadableStreamDefaultControllerFromUnderlyingSource(
    JSContext* cx, Handle<ReadableStream*> stream, HandleValue underlyingSource,
    double highWaterMark, HandleValue sizeAlgorithm) {
  // Step 1: Assert: underlyingSource is not undefined.
  MOZ_ASSERT(!underlyingSource.isUndefined());

  // Step 2: Let controller be ObjectCreate(the original value of
  //         ReadableStreamDefaultController's prototype property).
  // (Deferred to SetUpReadableStreamDefaultController.)

  // Step 3: Let startAlgorithm be the following steps:
  //         a. Return ? InvokeOrNoop(underlyingSource, "start",
  //                                  « controller »).
  SourceAlgorithms sourceAlgorithms = SourceAlgorithms::Script;

  // Step 4: Let pullAlgorithm be
  //         ? CreateAlgorithmFromUnderlyingMethod(underlyingSource, "pull",
  //                                               0, « controller »).
  RootedValue pullMethod(cx);
  if (!CreateAlgorithmFromUnderlyingMethod(cx, underlyingSource,
                                           "ReadableStream source.pull method",
                                           cx->names().pull, &pullMethod)) {
    return false;
  }

  // Step 5. Let cancelAlgorithm be
  //         ? CreateAlgorithmFromUnderlyingMethod(underlyingSource,
  //                                               "cancel", 1, « »).
  RootedValue cancelMethod(cx);
  if (!CreateAlgorithmFromUnderlyingMethod(
          cx, underlyingSource, "ReadableStream source.cancel method",
          cx->names().cancel, &cancelMethod)) {
    return false;
  }

  // Step 6. Perform ? SetUpReadableStreamDefaultController(stream,
  //             controller, startAlgorithm, pullAlgorithm, cancelAlgorithm,
  //             highWaterMark, sizeAlgorithm).
  return SetUpReadableStreamDefaultController(
      cx, stream, sourceAlgorithms, underlyingSource, pullMethod, cancelMethod,
      highWaterMark, sizeAlgorithm);
}

/*** 3.11. Class ReadableByteStreamController *******************************/

#if 0  // disable user-defined byte streams

/**
 * Streams spec, 3.10.3
 *      new ReadableByteStreamController ( stream, underlyingSource,
 *                                         highWaterMark )
 * Steps 3 - 16.
 *
 * Note: All arguments must be same-compartment with cx. ReadableStream
 * controllers are always created in the same compartment as the stream.
 */
static MOZ_MUST_USE ReadableByteStreamController*
CreateReadableByteStreamController(JSContext* cx,
                                   Handle<ReadableStream*> stream,
                                   HandleValue underlyingByteSource,
                                   HandleValue highWaterMarkVal)
{
    cx->check(stream, underlyingByteSource, highWaterMarkVal);

    Rooted<ReadableByteStreamController*> controller(cx,
        NewBuiltinClassInstance<ReadableByteStreamController>(cx));
    if (!controller) {
        return nullptr;
    }

    // Step 3: Set this.[[controlledReadableStream]] to stream.
    controller->setStream(stream);

    // Step 4: Set this.[[underlyingByteSource]] to underlyingByteSource.
    controller->setUnderlyingSource(underlyingByteSource);

    // Step 5: Set this.[[pullAgain]], and this.[[pulling]] to false.
    controller->setFlags(0);

    // Step 6: Perform ! ReadableByteStreamControllerClearPendingPullIntos(this).
    if (!ReadableByteStreamControllerClearPendingPullIntos(cx, controller)) {
        return nullptr;
    }

    // Step 7: Perform ! ResetQueue(this).
    if (!ResetQueue(cx, controller)) {
        return nullptr;
    }

    // Step 8: Set this.[[started]] and this.[[closeRequested]] to false.
    // These should be false by default, unchanged since step 5.
    MOZ_ASSERT(controller->flags() == 0);

    // Step 9: Set this.[[strategyHWM]] to
    //         ? ValidateAndNormalizeHighWaterMark(highWaterMark).
    double highWaterMark;
    if (!ValidateAndNormalizeHighWaterMark(cx, highWaterMarkVal, &highWaterMark)) {
        return nullptr;
    }
    controller->setStrategyHWM(highWaterMark);

    // Step 10: Let autoAllocateChunkSize be
    //          ? GetV(underlyingByteSource, "autoAllocateChunkSize").
    RootedValue autoAllocateChunkSize(cx);
    if (!GetProperty(cx, underlyingByteSource, cx->names().autoAllocateChunkSize,
                     &autoAllocateChunkSize))
    {
        return nullptr;
    }

    // Step 11: If autoAllocateChunkSize is not undefined,
    if (!autoAllocateChunkSize.isUndefined()) {
        // Step a: If ! IsInteger(autoAllocateChunkSize) is false, or if
        //         autoAllocateChunkSize ≤ 0, throw a RangeError exception.
        if (!IsInteger(autoAllocateChunkSize) || autoAllocateChunkSize.toNumber() <= 0) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_READABLEBYTESTREAMCONTROLLER_BAD_CHUNKSIZE);
            return nullptr;
        }
    }

    // Step 12: Set this.[[autoAllocateChunkSize]] to autoAllocateChunkSize.
    controller->setAutoAllocateChunkSize(autoAllocateChunkSize);

    // Step 13: Set this.[[pendingPullIntos]] to a new empty List.
    if (!StoreNewListInFixedSlot(cx, controller,
                                 ReadableByteStreamController::Slot_PendingPullIntos)) {
        return nullptr;
    }

    // Step 14: Let controller be this (implicit).

    // Step 15: Let startResult be
    //          ? InvokeOrNoop(underlyingSource, "start", « this »).
    RootedValue startResult(cx);
    RootedValue controllerVal(cx, ObjectValue(*controller));
    if (!InvokeOrNoop(cx, underlyingByteSource, cx->names().start, controllerVal, &startResult)) {
        return nullptr;
    }

    // Step 16: Let startPromise be a promise resolved with startResult:
    RootedObject startPromise(cx, PromiseObject::unforgeableResolve(cx, startResult));
    if (!startPromise) {
        return nullptr;
    }

    RootedObject onStartFulfilled(cx, NewHandler(cx, ControllerStartHandler, controller));
    if (!onStartFulfilled) {
        return nullptr;
    }

    RootedObject onStartRejected(cx, NewHandler(cx, ControllerStartFailedHandler, controller));
    if (!onStartRejected) {
        return nullptr;
    }

    if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled, onStartRejected)) {
        return nullptr;
    }

    return controller;
}

#endif  // user-defined byte streams

/**
 * Streams spec, 3.11.3.
 * new ReadableByteStreamController ( stream, underlyingByteSource,
 *                                    highWaterMark )
 */
bool ReadableByteStreamController::constructor(JSContext* cx, unsigned argc,
                                               Value* vp) {
  // Step 1: Throw a TypeError exception.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_BOGUS_CONSTRUCTOR,
                            "ReadableByteStreamController");
  return false;
}

// Disconnect the source from a controller without calling finalize() on it,
// unless this class is reset(). This ensures that finalize() will not be called
// on the source if setting up the controller fails.
class MOZ_RAII AutoClearUnderlyingSource {
  Rooted<ReadableStreamController*> controller_;

 public:
  AutoClearUnderlyingSource(JSContext* cx, ReadableStreamController* controller)
      : controller_(cx, controller) {}

  ~AutoClearUnderlyingSource() {
    if (controller_) {
      ReadableStreamController::clearUnderlyingSource(
          controller_, /* finalizeSource */ false);
    }
  }

  void reset() { controller_ = nullptr; }
};

/**
 * Version of SetUpReadableByteStreamController that's specialized for handling
 * external, embedding-provided, underlying sources.
 */
MOZ_MUST_USE bool js::SetUpExternalReadableByteStreamController(
    JSContext* cx, Handle<ReadableStream*> stream,
    JS::ReadableStreamUnderlyingSource* source) {
  // Done elsewhere in the standard: Create the controller object.
  Rooted<ReadableByteStreamController*> controller(
      cx, NewBuiltinClassInstance<ReadableByteStreamController>(cx));
  if (!controller) {
    return false;
  }

  AutoClearUnderlyingSource autoClear(cx, controller);

  // Step 1: Assert: stream.[[readableStreamController]] is undefined.
  MOZ_ASSERT(!stream->hasController());

  // Step 2: If autoAllocateChunkSize is not undefined, [...]
  // (It's treated as undefined.)

  // Step 3: Set controller.[[controlledReadableByteStream]] to stream.
  controller->setStream(stream);

  // Step 4: Set controller.[[pullAgain]] and controller.[[pulling]] to false.
  controller->setFlags(0);
  MOZ_ASSERT(!controller->pullAgain());
  MOZ_ASSERT(!controller->pulling());

  // Step 5: Perform
  //         ! ReadableByteStreamControllerClearPendingPullIntos(controller).
  // Omitted. This step is apparently redundant; see
  // <https://github.com/whatwg/streams/issues/975>.

  // Step 6: Perform ! ResetQueue(this).
  controller->setQueueTotalSize(0);

  // Step 7: Set controller.[[closeRequested]] and controller.[[started]] to
  //         false (implicit).
  MOZ_ASSERT(!controller->closeRequested());
  MOZ_ASSERT(!controller->started());

  // Step 8: Set controller.[[strategyHWM]] to
  //         ? ValidateAndNormalizeHighWaterMark(highWaterMark).
  controller->setStrategyHWM(0);

  // Step 9: Set controller.[[pullAlgorithm]] to pullAlgorithm.
  // Step 10: Set controller.[[cancelAlgorithm]] to cancelAlgorithm.
  // (These algorithms are given by source's virtual methods.)
  controller->setExternalSource(source);

  // Step 11: Set controller.[[autoAllocateChunkSize]] to
  //          autoAllocateChunkSize (implicit).
  MOZ_ASSERT(controller->autoAllocateChunkSize().isUndefined());

  // Step 12: Set this.[[pendingPullIntos]] to a new empty List.
  if (!StoreNewListInFixedSlot(
          cx, controller,
          ReadableByteStreamController::Slot_PendingPullIntos)) {
    return false;
  }

  // Step 13: Set stream.[[readableStreamController]] to controller.
  stream->setController(controller);

  // Step 14: Let startResult be the result of performing startAlgorithm.
  // (For external sources, this algorithm does nothing and returns undefined.)
  // Step 15: Let startPromise be a promise resolved with startResult.
  RootedObject startPromise(
      cx, PromiseObject::unforgeableResolve(cx, UndefinedHandleValue));
  if (!startPromise) {
    return false;
  }

  // Step 16: Upon fulfillment of startPromise, [...]
  // Step 17: Upon rejection of startPromise with reason r, [...]
  RootedObject onStartFulfilled(
      cx, NewHandler(cx, ControllerStartHandler, controller));
  if (!onStartFulfilled) {
    return false;
  }
  RootedObject onStartRejected(
      cx, NewHandler(cx, ControllerStartFailedHandler, controller));
  if (!onStartRejected) {
    return false;
  }
  if (!JS::AddPromiseReactions(cx, startPromise, onStartFulfilled,
                               onStartRejected)) {
    return false;
  }

  autoClear.reset();
  return true;
}

static const JSPropertySpec ReadableByteStreamController_properties[] = {
    JS_PS_END};

static const JSFunctionSpec ReadableByteStreamController_methods[] = {
    JS_FS_END};

static void ReadableByteStreamControllerFinalize(JSFreeOp* fop, JSObject* obj) {
  ReadableByteStreamController& controller =
      obj->as<ReadableByteStreamController>();

  if (controller.getFixedSlot(ReadableStreamController::Slot_Flags)
          .isUndefined()) {
    return;
  }

  if (!controller.hasExternalSource()) {
    return;
  }

  controller.externalSource()->finalize();
}

static const JSClassOps ReadableByteStreamControllerClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    ReadableByteStreamControllerFinalize,
    nullptr, /* call        */
    nullptr, /* hasInstance */
    nullptr, /* construct   */
    nullptr, /* trace   */
};

JS_STREAMS_CLASS_SPEC(ReadableByteStreamController, 0, SlotCount,
                      ClassSpec::DontDefineConstructor,
                      JSCLASS_BACKGROUND_FINALIZE,
                      &ReadableByteStreamControllerClassOps);

// Streams spec, 3.11.5.1. [[CancelSteps]] ()
// Unified with 3.9.5.1 above.

static MOZ_MUST_USE bool ReadableByteStreamControllerHandleQueueDrain(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedController);

/**
 * Streams spec, 3.11.5.2. [[PullSteps]] ( forAuthorCode )
 */
static MOZ_MUST_USE JSObject* ReadableByteStreamControllerPullSteps(
    JSContext* cx, Handle<ReadableByteStreamController*> unwrappedController) {
  // Step 1: Let stream be this.[[controlledReadableByteStream]].
  Rooted<ReadableStream*> unwrappedStream(cx, unwrappedController->stream());

  // Step 2: Assert: ! ReadableStreamHasDefaultReader(stream) is true.
#ifdef DEBUG
  bool result;
  if (!ReadableStreamHasDefaultReader(cx, unwrappedStream, &result)) {
    return nullptr;
  }
  MOZ_ASSERT(result);
#endif

  RootedValue val(cx);
  // Step 3: If this.[[queueTotalSize]] > 0,
  double queueTotalSize = unwrappedController->queueTotalSize();
  if (queueTotalSize > 0) {
    // Step 3.a: Assert: ! ReadableStreamGetNumReadRequests(_stream_) is 0.
    MOZ_ASSERT(ReadableStreamGetNumReadRequests(unwrappedStream) == 0);

    RootedObject view(cx);

    MOZ_RELEASE_ASSERT(unwrappedStream->mode() ==
                       JS::ReadableStreamMode::ExternalSource);
#if 0   // disable user-defined byte streams
        if (unwrappedStream->mode() == JS::ReadableStreamMode::ExternalSource)
#endif  // user-defined byte streams
    {
      JS::ReadableStreamUnderlyingSource* source =
          unwrappedController->externalSource();

      view = JS_NewUint8Array(cx, queueTotalSize);
      if (!view) {
        return nullptr;
      }

      size_t bytesWritten;
      {
        AutoRealm ar(cx, unwrappedStream);
        JS::AutoSuppressGCAnalysis suppressGC(cx);
        JS::AutoCheckCannotGC noGC;
        bool dummy;
        void* buffer = JS_GetArrayBufferViewData(view, &dummy, noGC);

        source->writeIntoReadRequestBuffer(cx, unwrappedStream, buffer,
                                           queueTotalSize, &bytesWritten);
      }

      queueTotalSize = queueTotalSize - bytesWritten;
    }

#if 0   // disable user-defined byte streams
        else {
            // Step 3.b: Let entry be the first element of this.[[queue]].
            // Step 3.c: Remove entry from this.[[queue]], shifting all other
            //           elements downward (so that the second becomes the
            //           first, and so on).
            Rooted<ListObject*> unwrappedQueue(cx, unwrappedController->queue());
            Rooted<ByteStreamChunk*> unwrappedEntry(cx,
                UnwrapAndDowncastObject<ByteStreamChunk>(
                    cx, &unwrappedQueue->popFirstAs<JSObject>(cx)));
            if (!unwrappedEntry) {
                return nullptr;
            }

            queueTotalSize = queueTotalSize - unwrappedEntry->byteLength();

            // Step 3.f: Let view be ! Construct(%Uint8Array%,
            //                                   « entry.[[buffer]],
            //                                     entry.[[byteOffset]],
            //                                     entry.[[byteLength]] »).
            // (reordered)
            RootedObject buffer(cx, unwrappedEntry->buffer());
            if (!cx->compartment()->wrap(cx, &buffer)) {
                return nullptr;
            }

            uint32_t byteOffset = unwrappedEntry->byteOffset();
            view = JS_NewUint8ArrayWithBuffer(cx, buffer, byteOffset, unwrappedEntry->byteLength());
            if (!view) {
                return nullptr;
            }
        }
#endif  // user-defined byte streams

    // Step 3.d: Set this.[[queueTotalSize]] to
    //           this.[[queueTotalSize]] − entry.[[byteLength]].
    // (reordered)
    unwrappedController->setQueueTotalSize(queueTotalSize);

    // Step 3.e: Perform ! ReadableByteStreamControllerHandleQueueDrain(this).
    // (reordered)
    if (!ReadableByteStreamControllerHandleQueueDrain(cx,
                                                      unwrappedController)) {
      return nullptr;
    }

    // Step 3.g: Return a promise resolved with
    //           ! ReadableStreamCreateReadResult(view, false, forAuthorCode).
    val.setObject(*view);
    ReadableStreamReader* unwrappedReader =
        UnwrapReaderFromStream(cx, unwrappedStream);
    if (!unwrappedReader) {
      return nullptr;
    }
    RootedObject readResult(
        cx, ReadableStreamCreateReadResult(cx, val, false,
                                           unwrappedReader->forAuthorCode()));
    if (!readResult) {
      return nullptr;
    }
    val.setObject(*readResult);

    return PromiseObject::unforgeableResolve(cx, val);
  }

  // Step 4: Let autoAllocateChunkSize be this.[[autoAllocateChunkSize]].
  val = unwrappedController->autoAllocateChunkSize();

  // Step 5: If autoAllocateChunkSize is not undefined,
  if (!val.isUndefined()) {
    double autoAllocateChunkSize = val.toNumber();

    // Step 5.a: Let buffer be
    //           Construct(%ArrayBuffer%, « autoAllocateChunkSize »).
    JSObject* bufferObj = JS::NewArrayBuffer(cx, autoAllocateChunkSize);

    // Step 5.b: If buffer is an abrupt completion,
    //           return a promise rejected with buffer.[[Value]].
    if (!bufferObj) {
      return PromiseRejectedWithPendingError(cx);
    }

    RootedArrayBufferObject buffer(cx, &bufferObj->as<ArrayBufferObject>());

    // Step 5.c: Let pullIntoDescriptor be
    //           Record {[[buffer]]: buffer.[[Value]],
    //                   [[byteOffset]]: 0,
    //                   [[byteLength]]: autoAllocateChunkSize,
    //                   [[bytesFilled]]: 0,
    //                   [[elementSize]]: 1,
    //                   [[ctor]]: %Uint8Array%,
    //                   [[readerType]]: `"default"`}.
    RootedObject pullIntoDescriptor(
        cx, PullIntoDescriptor::create(cx, buffer, 0, autoAllocateChunkSize, 0,
                                       1, nullptr, ReaderType::Default));
    if (!pullIntoDescriptor) {
      return PromiseRejectedWithPendingError(cx);
    }

    // Step 5.d: Append pullIntoDescriptor as the last element of
    //           this.[[pendingPullIntos]].
    if (!AppendToListInFixedSlot(
            cx, unwrappedController,
            ReadableByteStreamController::Slot_PendingPullIntos,
            pullIntoDescriptor)) {
      return nullptr;
    }
  }

  // Step 6: Let promise be ! ReadableStreamAddReadRequest(stream,
  //                                                       forAuthorCode).
  RootedObject promise(
      cx, ReadableStreamAddReadOrReadIntoRequest(cx, unwrappedStream));
  if (!promise) {
    return nullptr;
  }

  // Step 7: Perform ! ReadableByteStreamControllerCallPullIfNeeded(this).
  if (!ReadableStreamControllerCallPullIfNeeded(cx, unwrappedController)) {
    return nullptr;
  }

  // Step 8: Return promise.
  return promise;
}

/**
 * Unified implementation of ReadableStream controllers' [[PullSteps]] internal
 * methods.
 * Streams spec, 3.9.5.2. [[PullSteps]] ( forAuthorCode )
 * and
 * Streams spec, 3.11.5.2. [[PullSteps]] ( forAuthorCode )
 */
MOZ_MUST_USE JSObject* js::ReadableStreamControllerPullSteps(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedController) {
  if (unwrappedController->is<ReadableStreamDefaultController>()) {
    Rooted<ReadableStreamDefaultController*> unwrappedDefaultController(
        cx, &unwrappedController->as<ReadableStreamDefaultController>());
    return ReadableStreamDefaultControllerPullSteps(cx,
                                                    unwrappedDefaultController);
  }

  Rooted<ReadableByteStreamController*> unwrappedByteController(
      cx, &unwrappedController->as<ReadableByteStreamController>());
  return ReadableByteStreamControllerPullSteps(cx, unwrappedByteController);
}

/*** 3.13. Readable stream BYOB controller abstract operations **************/

// Streams spec, 3.13.1. IsReadableStreamBYOBRequest ( x )
// Implemented via is<ReadableStreamBYOBRequest>()

// Streams spec, 3.13.2. IsReadableByteStreamController ( x )
// Implemented via is<ReadableByteStreamController>()

// Streams spec, 3.13.3.
//      ReadableByteStreamControllerCallPullIfNeeded ( controller )
// Unified with 3.9.2 above.

static MOZ_MUST_USE bool ReadableByteStreamControllerInvalidateBYOBRequest(
    JSContext* cx, Handle<ReadableByteStreamController*> unwrappedController);

/**
 * Streams spec, 3.13.5.
 *      ReadableByteStreamControllerClearPendingPullIntos ( controller )
 */
static MOZ_MUST_USE bool ReadableByteStreamControllerClearPendingPullIntos(
    JSContext* cx, Handle<ReadableByteStreamController*> unwrappedController) {
  // Step 1: Perform
  //         ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
  if (!ReadableByteStreamControllerInvalidateBYOBRequest(cx,
                                                         unwrappedController)) {
    return false;
  }

  // Step 2: Set controller.[[pendingPullIntos]] to a new empty List.
  return StoreNewListInFixedSlot(
      cx, unwrappedController,
      ReadableByteStreamController::Slot_PendingPullIntos);
}

/**
 * Streams spec, 3.13.6. ReadableByteStreamControllerClose ( controller )
 */
MOZ_MUST_USE bool js::ReadableByteStreamControllerClose(
    JSContext* cx, Handle<ReadableByteStreamController*> unwrappedController) {
  // Step 1: Let stream be controller.[[controlledReadableByteStream]].
  Rooted<ReadableStream*> unwrappedStream(cx, unwrappedController->stream());

  // Step 2: Assert: controller.[[closeRequested]] is false.
  MOZ_ASSERT(!unwrappedController->closeRequested());

  // Step 3: Assert: stream.[[state]] is "readable".
  MOZ_ASSERT(unwrappedStream->readable());

  // Step 4: If controller.[[queueTotalSize]] > 0,
  if (unwrappedController->queueTotalSize() > 0) {
    // Step a: Set controller.[[closeRequested]] to true.
    unwrappedController->setCloseRequested();

    // Step b: Return.
    return true;
  }

  // Step 5: If controller.[[pendingPullIntos]] is not empty,
  Rooted<ListObject*> unwrappedPendingPullIntos(
      cx, unwrappedController->pendingPullIntos());
  if (unwrappedPendingPullIntos->length() != 0) {
    // Step a: Let firstPendingPullInto be the first element of
    //         controller.[[pendingPullIntos]].
    Rooted<PullIntoDescriptor*> unwrappedFirstPendingPullInto(
        cx, UnwrapAndDowncastObject<PullIntoDescriptor>(
                cx, &unwrappedPendingPullIntos->get(0).toObject()));
    if (!unwrappedFirstPendingPullInto) {
      return false;
    }

    // Step b: If firstPendingPullInto.[[bytesFilled]] > 0,
    if (unwrappedFirstPendingPullInto->bytesFilled() > 0) {
      // Step i: Let e be a new TypeError exception.
      JS_ReportErrorNumberASCII(
          cx, GetErrorMessage, nullptr,
          JSMSG_READABLEBYTESTREAMCONTROLLER_CLOSE_PENDING_PULL);
      RootedValue e(cx);
      RootedSavedFrame stack(cx);
      if (!cx->isExceptionPending() ||
          !GetAndClearExceptionAndStack(cx, &e, &stack)) {
        // Uncatchable error. Die immediately without erroring the
        // stream.
        return false;
      }

      // Step ii: Perform ! ReadableByteStreamControllerError(controller, e).
      if (!ReadableStreamControllerError(cx, unwrappedController, e)) {
        return false;
      }

      // Step iii: Throw e.
      cx->setPendingException(e, stack);
      return false;
    }
  }

  // Step 6: Perform ! ReadableByteStreamControllerClearAlgorithms(controller).
  ReadableStreamControllerClearAlgorithms(unwrappedController);

  // Step 7: Perform ! ReadableStreamClose(stream).
  return ReadableStreamCloseInternal(cx, unwrappedStream);
}

// Streams spec, 3.13.11. ReadableByteStreamControllerError ( controller, e )
// Unified with 3.10.7 above.

// Streams spec 3.13.14.
//      ReadableByteStreamControllerGetDesiredSize ( controller )
// Unified with 3.10.8 above.

/**
 * Streams spec, 3.13.15.
 *      ReadableByteStreamControllerHandleQueueDrain ( controller )
 */
static MOZ_MUST_USE bool ReadableByteStreamControllerHandleQueueDrain(
    JSContext* cx, Handle<ReadableStreamController*> unwrappedController) {
  MOZ_ASSERT(unwrappedController->is<ReadableByteStreamController>());

  // Step 1: Assert: controller.[[controlledReadableStream]].[[state]]
  //                 is "readable".
  Rooted<ReadableStream*> unwrappedStream(cx, unwrappedController->stream());
  MOZ_ASSERT(unwrappedStream->readable());

  // Step 2: If controller.[[queueTotalSize]] is 0 and
  //         controller.[[closeRequested]] is true,
  if (unwrappedController->queueTotalSize() == 0 &&
      unwrappedController->closeRequested()) {
    // Step a: Perform
    //         ! ReadableByteStreamControllerClearAlgorithms(controller).
    ReadableStreamControllerClearAlgorithms(unwrappedController);

    // Step b: Perform
    //         ! ReadableStreamClose(controller.[[controlledReadableStream]]).
    return ReadableStreamCloseInternal(cx, unwrappedStream);
  }

  // Step 3: Otherwise,
  // Step a: Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
  return ReadableStreamControllerCallPullIfNeeded(cx, unwrappedController);
}

enum BYOBRequestSlots {
  BYOBRequestSlot_Controller,
  BYOBRequestSlot_View,
  BYOBRequestSlotCount
};

/**
 * Streams spec 3.13.16.
 *      ReadableByteStreamControllerInvalidateBYOBRequest ( controller )
 */
static MOZ_MUST_USE bool ReadableByteStreamControllerInvalidateBYOBRequest(
    JSContext* cx, Handle<ReadableByteStreamController*> unwrappedController) {
  // Step 1: If controller.[[byobRequest]] is undefined, return.
  RootedValue unwrappedBYOBRequestVal(cx, unwrappedController->byobRequest());
  if (unwrappedBYOBRequestVal.isUndefined()) {
    return true;
  }

  RootedNativeObject unwrappedBYOBRequest(
      cx, UnwrapAndDowncastValue<NativeObject>(cx, unwrappedBYOBRequestVal));
  if (!unwrappedBYOBRequest) {
    return false;
  }

  // Step 2: Set controller.[[byobRequest]]
  //                       .[[associatedReadableByteStreamController]]
  //         to undefined.
  unwrappedBYOBRequest->setFixedSlot(BYOBRequestSlot_Controller,
                                     UndefinedValue());

  // Step 3: Set controller.[[byobRequest]].[[view]] to undefined.
  unwrappedBYOBRequest->setFixedSlot(BYOBRequestSlot_View, UndefinedValue());

  // Step 4: Set controller.[[byobRequest]] to undefined.
  unwrappedController->clearBYOBRequest();

  return true;
}

// Streams spec, 3.13.25.
//      ReadableByteStreamControllerShouldCallPull ( controller )
// Unified with 3.10.3 above.
