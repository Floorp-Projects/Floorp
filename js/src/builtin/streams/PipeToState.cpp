/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* ReadableStream.prototype.pipeTo state. */

#include "builtin/streams/PipeToState.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE
#include "mozilla/Maybe.h"  // mozilla::Maybe, mozilla::Nothing, mozilla::Some

#include "jsapi.h"        // JS_ReportErrorNumberASCII
#include "jsfriendapi.h"  // js::GetErrorMessage, JSMSG_*

#include "builtin/Promise.h"  // js::RejectPromiseWithPendingError
#include "builtin/streams/ReadableStream.h"  // js::ReadableStream
#include "builtin/streams/ReadableStreamReader.h"  // js::CreateReadableStreamDefaultReader, js::ForAuthorCodeBool, js::ReadableStreamDefaultReader
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::CreateWritableStreamDefaultWriter, js::WritableStreamDefaultWriter
#include "builtin/streams/WritableStreamOperations.h"  // js::WritableStreamCloseQueuedOrInFlight
#include "js/CallArgs.h"       // JS::CallArgsFromVp, JS::CallArgs
#include "js/Class.h"          // JSClass, JSCLASS_HAS_RESERVED_SLOTS
#include "js/Promise.h"        // JS::AddPromiseReactions
#include "js/RootingAPI.h"     // JS::Handle, JS::Rooted
#include "vm/PromiseObject.h"  // js::PromiseObject

#include "builtin/streams/HandlerFunction-inl.h"  // js::NewHandler, js::TargetFromHandler
#include "builtin/streams/ReadableStreamReader-inl.h"  // js::UnwrapReaderFromStream, js::UnwrapStreamFromReader
#include "builtin/streams/WritableStream-inl.h"  // js::UnwrapWriterFromStream
#include "builtin/streams/WritableStreamDefaultWriter-inl.h"  // js::UnwrapStreamFromWriter
#include "vm/JSContext-inl.h"  // JSContext::check
#include "vm/JSObject-inl.h"   // js::NewBuiltinClassInstance

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::Handle;
using JS::Int32Value;
using JS::ObjectValue;
using JS::Rooted;
using JS::Value;

using js::GetErrorMessage;
using js::NewHandler;
using js::PipeToState;
using js::ReadableStream;
using js::ReadableStreamDefaultReader;
using js::TargetFromHandler;
using js::UnwrapReaderFromStream;
using js::UnwrapStreamFromWriter;
using js::UnwrapWriterFromStream;
using js::WritableStream;
using js::WritableStreamDefaultWriter;

// This typedef is undoubtedly not the right one for the long run, but it's
// enough to be placeholder for now.
using Action = bool (*)(JSContext*, Handle<PipeToState*> state);

static MOZ_MUST_USE bool DummyAction(JSContext* cx,
                                     Handle<PipeToState*> state) {
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_READABLESTREAM_METHOD_NOT_IMPLEMENTED,
                            "pipeTo dummy action");
  return false;
}

static ReadableStream* GetUnwrappedSource(JSContext* cx,
                                          Handle<PipeToState*> state) {
  cx->check(state);

  Rooted<ReadableStreamDefaultReader*> reader(cx, state->reader());
  cx->check(reader);

  return UnwrapStreamFromReader(cx, reader);
}

static WritableStream* GetUnwrappedDest(JSContext* cx,
                                        Handle<PipeToState*> state) {
  cx->check(state);

  Rooted<WritableStreamDefaultWriter*> writer(cx, state->writer());
  cx->check(writer);

  return UnwrapStreamFromWriter(cx, writer);
}

// Shutdown with an action: if any of the above requirements ask to shutdown
// with an action action, optionally with an error originalError, then:
static MOZ_MUST_USE bool ShutdownWithAction(
    JSContext* cx, Handle<PipeToState*> state, Action action,
    Handle<Maybe<Value>> originalError) {
  cx->check(state);
  cx->check(originalError);

  // Step a: If shuttingDown is true, abort these substeps.
  if (state->shuttingDown()) {
    return true;
  }

  // Step b: Set shuttingDown to true.
  state->setShuttingDown();

  // Step c: If dest.[[state]] is "writable" and
  //         ! WritableStreamCloseQueuedOrInFlight(dest) is false,
  // Step c.i:  If any chunks have been read but not yet written, write them to
  //            dest.
  // Step c.ii: Wait until every chunk that has been read has been written (i.e.
  //            the corresponding promises have settled).
  // Step d: Let p be the result of performing action.
  // Step e: Upon fulfillment of p, finalize, passing along originalError if it
  //         was given.
  // Step f: Upon rejection of p with reason newError, finalize with newError.

  // XXX fill me in!
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_READABLESTREAM_METHOD_NOT_IMPLEMENTED,
                            "pipeTo shutdown with action");
  return false;
}

// Shutdown: if any of the above requirements or steps ask to shutdown,
// optionally with an error error, then:
static MOZ_MUST_USE bool Shutdown(JSContext* cx, Handle<PipeToState*> state,
                                  Handle<Maybe<Value>> error) {
  cx->check(state);
  cx->check(error);

  // Step a: If shuttingDown is true, abort these substeps.
  if (state->shuttingDown()) {
    return true;
  }

  // Step b: Set shuttingDown to true.
  state->setShuttingDown();

  // Step c: If dest.[[state]] is "writable" and
  //         ! WritableStreamCloseQueuedOrInFlight(dest) is false,
  // Step c.i:  If any chunks have been read but not yet written, write them to
  //            dest.
  // Step c.ii: Wait until every chunk that has been read has been written (i.e.
  //            the corresponding promises have settled).
  // Step d: Finalize, passing along error if it was given.

  // XXX fill me in!
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_READABLESTREAM_METHOD_NOT_IMPLEMENTED,
                            "pipeTo shutdown");
  return false;
}

/**
 * Streams spec, 3.4.11. ReadableStreamPipeTo step 14:
 * "a. Errors must be propagated forward: if source.[[state]] is or becomes
 * 'errored', then..."
 */
static MOZ_MUST_USE bool OnSourceErrored(
    JSContext* cx, Handle<PipeToState*> state,
    Handle<ReadableStream*> unwrappedSource) {
  cx->check(state);

  Rooted<Maybe<Value>> storedError(cx, Some(unwrappedSource->storedError()));
  if (!cx->compartment()->wrap(cx, &storedError)) {
    return false;
  }

  // ii. Otherwise (if preventAbort is true), shutdown with
  //     source.[[storedError]].
  if (state->preventAbort()) {
    if (!Shutdown(cx, state, storedError)) {
      return false;
    }
  }
  // i. (If preventAbort is false,) shutdown with an action of
  //    ! WritableStreamAbort(dest, source.[[storedError]]) and with
  //    source.[[storedError]].
  else {
    if (!ShutdownWithAction(cx, state, DummyAction, storedError)) {
      return false;
    }
  }

  return true;
}

/**
 * Streams spec, 3.4.11. ReadableStreamPipeTo step 14:
 * "b. Errors must be propagated backward: if dest.[[state]] is or becomes
 * 'errored', then..."
 */
static MOZ_MUST_USE bool OnDestErrored(JSContext* cx,
                                       Handle<PipeToState*> state,
                                       Handle<WritableStream*> unwrappedDest) {
  cx->check(state);

  Rooted<Maybe<Value>> storedError(cx, Some(unwrappedDest->storedError()));
  if (!cx->compartment()->wrap(cx, &storedError)) {
    return false;
  }

  // ii. Otherwise (if preventCancel is true), shutdown with
  //     dest.[[storedError]].
  if (state->preventCancel()) {
    if (!Shutdown(cx, state, storedError)) {
      return false;
    }
  }
  // i. If preventCancel is false, shutdown with an action of
  //    ! ReadableStreamCancel(source, dest.[[storedError]]) and with
  //    dest.[[storedError]].
  else {
    if (!ShutdownWithAction(cx, state, DummyAction, storedError)) {
      return false;
    }
  }

  return true;
}

/**
 * Streams spec, 3.4.11. ReadableStreamPipeTo step 14:
 * "c. Closing must be propagated forward: if source.[[state]] is or becomes
 * 'closed', then..."
 */
static MOZ_MUST_USE bool OnSourceClosed(JSContext* cx,
                                        Handle<PipeToState*> state) {
  cx->check(state);

  Rooted<Maybe<Value>> noError(cx, Nothing());

  // ii. Otherwise (if preventClose is true), shutdown.
  if (state->preventClose()) {
    if (!Shutdown(cx, state, noError)) {
      return false;
    }
  }
  // i. If preventClose is false, shutdown with an action of
  //    ! WritableStreamDefaultWriterCloseWithErrorPropagation(writer).
  else {
    if (!ShutdownWithAction(cx, state, DummyAction, noError)) {
      return false;
    }
  }

  return true;
}

/**
 * Streams spec, 3.4.11. ReadableStreamPipeTo step 14:
 * "d. Closing must be propagated backward: if
 * ! WritableStreamCloseQueuedOrInFlight(dest) is true or dest.[[state]] is
 * 'closed', then..."
 */
static MOZ_MUST_USE bool OnDestClosed(JSContext* cx,
                                      Handle<PipeToState*> state) {
  cx->check(state);

  // i. Assert: no chunks have been read or written.
  //
  // This assertion holds when this function is called by |CanPipeStreams|,
  // prior to any asynchronous internal piping operations happening.
  //
  // But it wouldn't hold for streams that can spontaneously close of their own
  // accord, like say a hypothetical DOM TCP socket.  I think?
  //
  // XXX Add this assertion if it really does hold (and is easily performed),
  //     else report a spec bug.

  // ii. Let destClosed be a new TypeError.
  Rooted<Maybe<Value>> destClosed(cx, Nothing());
  {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAM_WRITE_CLOSING_OR_CLOSED);
    Rooted<Value> v(cx);
    if (!cx->isExceptionPending() || !GetAndClearException(cx, &v)) {
      return false;
    }

    destClosed = Some(v.get());
  }

  // iv. Otherwise (if preventCancel is true), shutdown with destClosed.
  if (state->preventCancel()) {
    if (!Shutdown(cx, state, destClosed)) {
      return false;
    }
  }
  // iii. If preventCancel is false, shutdown with an action of
  //      ! ReadableStreamCancel(source, destClosed) and with destClosed.
  else {
    if (!ShutdownWithAction(cx, state, DummyAction, destClosed)) {
      return false;
    }
  }

  return true;
}

/**
 * Streams spec, 3.4.11. ReadableStreamPipeTo step 14:
 * "Error and close states must be propagated: the following conditions must be
 * applied in order.", as applied at the very start of piping, before any reads
 * from source or writes to dest have been triggered.
 */
static MOZ_MUST_USE bool CanPipeStreams(JSContext* cx,
                                        Handle<PipeToState*> state,
                                        Handle<ReadableStream*> unwrappedSource,
                                        Handle<WritableStream*> unwrappedDest,
                                        bool* shouldStart) {
  cx->check(state);

  // At start of piping, we don't yet have reactions added for source/dest
  // closure or error, so we have to check for this manually.  Assume we
  // shouldn't start, then override that once all preconditions are checked.
  *shouldStart = false;

  // a. Errors must be propagated forward: if source.[[state]] is or becomes
  //    "errored", then
  if (unwrappedSource->errored()) {
    return OnSourceErrored(cx, state, unwrappedSource);
  }

  // b. Errors must be propagated backward: if dest.[[state]] is or becomes
  //    "errored", then
  if (unwrappedDest->errored()) {
    return OnDestErrored(cx, state, unwrappedDest);
  }

  // c. Closing must be propagated forward: if source.[[state]] is or becomes
  //    "closed", then
  if (unwrappedSource->closed()) {
    return OnSourceClosed(cx, state);
  }

  // d. Closing must be propagated backward: if
  //    ! WritableStreamCloseQueuedOrInFlight(dest) is true or dest.[[state]] is
  //    "closed", then
  if (WritableStreamCloseQueuedOrInFlight(unwrappedDest) ||
      unwrappedDest->closed()) {
    return OnDestClosed(cx, state);
  }

  *shouldStart = true;
  return true;
}

static MOZ_MUST_USE bool OnSourceClosed(JSContext* cx, unsigned argc,
                                        Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<PipeToState*> state(cx, TargetFromHandler<PipeToState>(args));
  cx->check(state);

  if (!OnSourceClosed(cx, state)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static MOZ_MUST_USE bool OnSourceErrored(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<PipeToState*> state(cx, TargetFromHandler<PipeToState>(args));
  cx->check(state);

  Rooted<ReadableStream*> unwrappedSource(cx, GetUnwrappedSource(cx, state));
  if (!unwrappedSource) {
    return false;
  }

  if (!OnSourceErrored(cx, state, unwrappedSource)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static MOZ_MUST_USE bool OnDestClosed(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<PipeToState*> state(cx, TargetFromHandler<PipeToState>(args));
  cx->check(state);

  if (!OnDestClosed(cx, state)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static MOZ_MUST_USE bool OnDestErrored(JSContext* cx, unsigned argc,
                                       Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<PipeToState*> state(cx, TargetFromHandler<PipeToState>(args));
  cx->check(state);

  Rooted<WritableStream*> unwrappedDest(cx, GetUnwrappedDest(cx, state));
  if (!unwrappedDest) {
    return false;
  }

  if (!OnDestErrored(cx, state, unwrappedDest)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

template <class StreamAccessor, class Stream>
static inline JSObject* GetClosedPromise(
    JSContext* cx, Handle<Stream*> unwrappedStream,
    StreamAccessor* (&unwrapAccessorFromStream)(JSContext*, Handle<Stream*>)) {
  StreamAccessor* unwrappedAccessor =
      unwrapAccessorFromStream(cx, unwrappedStream);
  if (!unwrappedAccessor) {
    return nullptr;
  }

  return unwrappedAccessor->closedPromise();
}

static MOZ_MUST_USE bool StartPiping(JSContext* cx, Handle<PipeToState*> state,
                                     Handle<ReadableStream*> unwrappedSource,
                                     Handle<WritableStream*> unwrappedDest) {
  cx->check(state);

  // Initially, check for source/dest closed or in error manually.
  bool shouldStart;
  if (!CanPipeStreams(cx, state, unwrappedSource, unwrappedDest,
                      &shouldStart)) {
    return false;
  }
  if (!shouldStart) {
    return true;
  }

  // After this point, react to source/dest closing or erroring as it happens.
  {
    Rooted<JSObject*> unwrappedClosedPromise(cx);
    Rooted<JSObject*> onClosed(cx);
    Rooted<JSObject*> onErrored(cx);

    auto ReactWhenClosedOrErrored =
        [&unwrappedClosedPromise, &onClosed, &onErrored, &state](
            JSContext* cx, JSNative onClosedFunc, JSNative onErroredFunc) {
          onClosed = NewHandler(cx, onClosedFunc, state);
          if (!onClosed) {
            return false;
          }

          onErrored = NewHandler(cx, onErroredFunc, state);
          if (!onErrored) {
            return false;
          }

          return JS::AddPromiseReactions(cx, unwrappedClosedPromise, onClosed,
                                         onErrored);
        };

    unwrappedClosedPromise =
        GetClosedPromise(cx, unwrappedSource, UnwrapReaderFromStream);
    if (!unwrappedClosedPromise) {
      return false;
    }

    if (!ReactWhenClosedOrErrored(cx, OnSourceClosed, OnSourceErrored)) {
      return false;
    }

    unwrappedClosedPromise =
        GetClosedPromise(cx, unwrappedDest, UnwrapWriterFromStream);
    if (!unwrappedClosedPromise) {
      return false;
    }

    if (!ReactWhenClosedOrErrored(cx, OnDestClosed, OnDestErrored)) {
      return false;
    }
  }

  // XXX Operations extending beyond preconditions are not yet implemented.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_READABLESTREAM_METHOD_NOT_IMPLEMENTED,
                            "pipeTo");
  return false;
}

/**
 * Stream spec, 3.4.11. ReadableStreamPipeTo ( source, dest,
 *                                             preventClose, preventAbort,
 *                                             preventCancel, signal )
 * Steps 4-11, 13-14.
 */
/* static */ PipeToState* PipeToState::create(
    JSContext* cx, Handle<PromiseObject*> promise,
    Handle<ReadableStream*> unwrappedSource,
    Handle<WritableStream*> unwrappedDest, bool preventClose, bool preventAbort,
    bool preventCancel, Handle<JSObject*> signal) {
  cx->check(promise);

  // Step 4. Assert: signal is undefined or signal is an instance of the
  //         AbortSignal interface.
#ifdef DEBUG
  if (signal) {
    // XXX jwalden need to add JSAPI hooks to recognize AbortSignal instances
  }
#endif

  // Step 5: Assert: ! IsReadableStreamLocked(source) is false.
  MOZ_ASSERT(!unwrappedSource->locked());

  // Step 6: Assert: ! IsWritableStreamLocked(dest) is false.
  MOZ_ASSERT(!unwrappedDest->isLocked());

  Rooted<PipeToState*> state(cx, NewBuiltinClassInstance<PipeToState>(cx));
  if (!state) {
    return nullptr;
  }

  MOZ_ASSERT(state->getFixedSlot(Slot_Promise).isUndefined());
  state->initFixedSlot(Slot_Promise, ObjectValue(*promise));

  // Step 7: If ! IsReadableByteStreamController(
  //                  source.[[readableStreamController]]) is true, let reader
  //         be either ! AcquireReadableStreamBYOBReader(source) or
  //         ! AcquireReadableStreamDefaultReader(source), at the user agent’s
  //         discretion.
  // Step 8: Otherwise, let reader be
  //         ! AcquireReadableStreamDefaultReader(source).
  // We don't implement byte streams, so we always acquire a default reader.
  {
    ReadableStreamDefaultReader* reader = CreateReadableStreamDefaultReader(
        cx, unwrappedSource, ForAuthorCodeBool::No);
    if (!reader) {
      return nullptr;
    }

    MOZ_ASSERT(state->getFixedSlot(Slot_Reader).isUndefined());
    state->initFixedSlot(Slot_Reader, ObjectValue(*reader));
  }

  // Step 9: Let writer be ! AcquireWritableStreamDefaultWriter(dest).
  {
    WritableStreamDefaultWriter* writer =
        CreateWritableStreamDefaultWriter(cx, unwrappedDest);
    if (!writer) {
      return nullptr;
    }

    MOZ_ASSERT(state->getFixedSlot(Slot_Writer).isUndefined());
    state->initFixedSlot(Slot_Writer, ObjectValue(*writer));
  }

  // Step 10: Set source.[[disturbed]] to true.
  unwrappedSource->setDisturbed();

  state->initFlags(preventClose, preventAbort, preventCancel);
  MOZ_ASSERT(state->preventClose() == preventClose);
  MOZ_ASSERT(state->preventAbort() == preventAbort);
  MOZ_ASSERT(state->preventCancel() == preventCancel);

  // Step 11: Let shuttingDown be false.
  MOZ_ASSERT(!state->shuttingDown(), "should be set to false by initFlags");

  // Step 12 ("Let promise be a new promise.") was performed by the caller and
  // |promise| was its result.

  // Step 13: If signal is not undefined,
  // XXX jwalden need JSAPI to add an algorithm/steps to an AbortSignal

  // Step 14: In parallel, using reader and writer, read all chunks from source
  //          and write them to dest.
  if (!StartPiping(cx, state, unwrappedSource, unwrappedDest)) {
    return nullptr;
  }

  return state;
}

const JSClass PipeToState::class_ = {"PipeToState",
                                     JSCLASS_HAS_RESERVED_SLOTS(SlotCount)};
