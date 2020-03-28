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
#include "builtin/streams/ReadableStreamReader.h"  // js::CreateReadableStreamDefaultReader, js::ForAuthorCodeBool
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::CreateWritableStreamDefaultWriter
#include "builtin/streams/WritableStreamOperations.h"  // js::WritableStreamCloseQueuedOrInFlight
#include "js/Class.h"          // JSClass, JSCLASS_HAS_RESERVED_SLOTS
#include "js/RootingAPI.h"     // JS::Handle, JS::Rooted
#include "vm/PromiseObject.h"  // js::PromiseObject

#include "vm/JSContext-inl.h"  // JSContext::check
#include "vm/JSObject-inl.h"   // js::NewBuiltinClassInstance

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

using JS::Handle;
using JS::Int32Value;
using JS::ObjectValue;
using JS::Rooted;
using JS::Value;

using js::GetErrorMessage;
using js::PipeToState;
using js::ReadableStream;
using js::WritableStream;

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

static MOZ_MUST_USE bool ShutdownWithAction(
    JSContext* cx, Handle<PipeToState*> state, Action action,
    Handle<Maybe<Value>> originalError) {
  cx->check(state);
  cx->check(originalError);

  // XXX fill me in!
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_READABLESTREAM_METHOD_NOT_IMPLEMENTED,
                            "pipeTo shutdown with action");
  return false;
}

static MOZ_MUST_USE bool Shutdown(JSContext* cx, Handle<PipeToState*> state,
                                  Handle<Maybe<Value>> error) {
  cx->check(state);
  cx->check(error);

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

static MOZ_MUST_USE bool StartPiping(JSContext* cx, Handle<PipeToState*> state,
                                     Handle<ReadableStream*> unwrappedSource,
                                     Handle<WritableStream*> unwrappedDest) {
  cx->check(state);

  bool shouldStart;
  if (!CanPipeStreams(cx, state, unwrappedSource, unwrappedDest,
                      &shouldStart)) {
    return false;
  }
  if (!shouldStart) {
    return true;
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
