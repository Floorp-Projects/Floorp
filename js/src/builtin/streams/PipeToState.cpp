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
#include "builtin/streams/ReadableStream.h"        // js::ReadableStream
#include "builtin/streams/ReadableStreamReader.h"  // js::CreateReadableStreamDefaultReader, js::ForAuthorCodeBool, js::ReadableStreamDefaultReader
#include "builtin/streams/WritableStream.h"        // js::WritableStream
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::CreateWritableStreamDefaultWriter, js::WritableStreamDefaultWriter
#include "builtin/streams/WritableStreamOperations.h"  // js::WritableStreamCloseQueuedOrInFlight
#include "builtin/streams/WritableStreamWriterOperations.h"  // js::WritableStreamDefaultWriter{GetDesiredSize,Write}
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
using js::PromiseObject;
using js::ReadableStream;
using js::ReadableStreamDefaultReader;
using js::TargetFromHandler;
using js::UnwrapReaderFromStream;
using js::UnwrapStreamFromWriter;
using js::UnwrapWriterFromStream;
using js::WritableStream;
using js::WritableStreamDefaultWriter;
using js::WritableStreamDefaultWriterWrite;

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

static bool WritableAndNotClosing(const WritableStream* unwrappedDest) {
  return unwrappedDest->writable() &&
         WritableStreamCloseQueuedOrInFlight(unwrappedDest);
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
  WritableStream* unwrappedDest = GetUnwrappedDest(cx, state);
  if (!unwrappedDest) {
    return false;
  }
  if (WritableAndNotClosing(unwrappedDest)) {
    // Step c.i:  If any chunks have been read but not yet written, write them
    //            to dest.
    // Step c.ii: Wait until every chunk that has been read has been written
    //            (i.e. the corresponding promises have settled).
  }

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
  WritableStream* unwrappedDest = GetUnwrappedDest(cx, state);
  if (!unwrappedDest) {
    return false;
  }
  if (WritableAndNotClosing(unwrappedDest)) {
    // Step c.i:  If any chunks have been read but not yet written, write them
    //            to dest.
    // Step c.ii: Wait until every chunk that has been read has been written
    //            (i.e. the corresponding promises have settled).
  }

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
  // This assertion holds when this function is called by
  // |SourceOrDestErroredOrClosed|, before any async internal piping operations
  // happen.
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
static MOZ_MUST_USE bool SourceOrDestErroredOrClosed(
    JSContext* cx, Handle<PipeToState*> state,
    Handle<ReadableStream*> unwrappedSource,
    Handle<WritableStream*> unwrappedDest, bool* erroredOrClosed) {
  cx->check(state);

  *erroredOrClosed = true;

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

  *erroredOrClosed = false;
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

static MOZ_MUST_USE bool ReadFromSource(JSContext* cx,
                                        Handle<PipeToState*> state);

static bool ReadFulfilled(JSContext* cx, Handle<PipeToState*> state,
                          Handle<JSObject*> result) {
  cx->check(state);
  cx->check(result);

  state->clearReadPending();

  // In general, "Shutdown must stop activity: if shuttingDown becomes true, the
  // user agent must not initiate further reads from reader, and must only
  // perform writes of already-read chunks".  But we "perform writes of already-
  // read chunks" here, so we ignore |state->shuttingDown()|.

  {
    bool done;
    {
      Rooted<Value> doneVal(cx);
      if (!GetProperty(cx, result, result, cx->names().done, &doneVal)) {
        return false;
      }
      done = doneVal.toBoolean();
    }

    if (done) {
      // All chunks have been read from |reader| and written to |writer| (but
      // not necessarily fulfilled yet, in the latter case).  Proceed as if
      // |source| is now closed.  (This will asynchronously wait until any
      // pending writes have fulfilled.)
      return OnSourceClosed(cx, state);
    }
  }

  // A chunk was read, and *at the time the read was requested*, |dest| was
  // ready to accept a write.  (Only one read is processed at a time per
  // |state->isReadPending()|, so this condition remains true now.)  Write the
  // chunk to |dest|.
  {
    Rooted<Value> chunk(cx);
    if (!GetProperty(cx, result, result, cx->names().value, &chunk)) {
      return false;
    }

    Rooted<WritableStreamDefaultWriter*> writer(cx, state->writer());
    cx->check(writer);

    PromiseObject* writeRequest =
        WritableStreamDefaultWriterWrite(cx, writer, chunk);
    if (!writeRequest) {
      return false;
    }

    // Stash away this new last write request.  (The shutdown process will react
    // to this write request to finish shutdown only once all pending writes are
    // completed.)
    state->updateLastWriteRequest(writeRequest);
  }

  // Read another chunk if this write didn't fill up |dest|.
  //
  // While we (properly) ignored |state->shuttingDown()| earlier, this call will
  // *not* initiate a fresh read if |!state->shuttingDown()|.
  return ReadFromSource(cx, state);
}

static bool ReadFulfilled(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);

  Rooted<PipeToState*> state(cx, TargetFromHandler<PipeToState>(args));
  cx->check(state);

  Rooted<JSObject*> result(cx, &args[0].toObject());
  cx->check(result);

  if (!ReadFulfilled(cx, state, result)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool ReadRejected(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  MOZ_ASSERT(args.length() == 1);

  Rooted<PipeToState*> state(cx, TargetFromHandler<PipeToState>(args));
  cx->check(state);

  state->clearReadPending();

  // XXX fill me in!

  args.rval().setUndefined();
  return true;
}

static bool ReadFromSource(JSContext* cx, unsigned argc, Value* vp);

static MOZ_MUST_USE bool ReadFromSource(JSContext* cx,
                                        Handle<PipeToState*> state) {
  cx->check(state);

  MOZ_ASSERT(!state->isReadPending(),
             "should only have one read in flight at a time, because multiple "
             "reads could cause the latter read to ignore backpressure "
             "signals");

  // "Shutdown must stop activity: if shuttingDown becomes true, the user agent
  // must not initiate further reads from reader..."
  if (state->shuttingDown()) {
    return true;
  }

  Rooted<WritableStreamDefaultWriter*> writer(cx, state->writer());
  cx->check(writer);

  // "While WritableStreamDefaultWriterGetDesiredSize(writer) is ≤ 0 or is null,
  // the user agent must not read from reader."
  Rooted<Value> desiredSize(cx);
  if (!WritableStreamDefaultWriterGetDesiredSize(cx, writer, &desiredSize)) {
    return false;
  }

  // If we're in the middle of erroring or are fully errored, either way the
  // |dest|-closed reaction queued up in |StartPiping| will do the right
  // thing, so do nothing here.
  if (desiredSize.isNull()) {
#ifdef DEBUG
    {
      WritableStream* unwrappedDest = GetUnwrappedDest(cx, state);
      if (!unwrappedDest) {
        return false;
      }

      MOZ_ASSERT(unwrappedDest->erroring() || unwrappedDest->errored());
    }
#endif

    return true;
  }

  // If |dest| isn't ready to receive writes yet (i.e. backpressure applies),
  // resume when it is.
  MOZ_ASSERT(desiredSize.isNumber());
  if (desiredSize.toNumber() <= 0) {
    Rooted<JSObject*> readyPromise(cx, writer->readyPromise());
    cx->check(readyPromise);

    Rooted<JSFunction*> readFromSource(cx,
                                       NewHandler(cx, ReadFromSource, state));
    if (!readFromSource) {
      return false;
    }

    // Resume when there's writable capacity.  Don't bother handling rejection:
    // if this happens, the stream is going to be errored shortly anyway, and
    // |StartPiping| has us ready to react to that already.
    //
    // XXX Double-check the claim that we need not handle rejections and that a
    //     rejection of [[readyPromise]] *necessarily* is always followed by
    //     rejection of [[closedPromise]].
    return JS::AddPromiseReactionsIgnoringUnhandledRejection(
        cx, readyPromise, readFromSource, nullptr);
  }

  // |dest| is ready to receive at least one write.  Read one chunk from the
  // reader now that we're not subject to backpressure.
  Rooted<ReadableStreamDefaultReader*> reader(cx, state->reader());
  cx->check(reader);

  Rooted<PromiseObject*> readRequest(
      cx, js::ReadableStreamDefaultReaderRead(cx, reader));
  if (!readRequest) {
    return false;
  }

  Rooted<JSFunction*> readFulfilled(cx, NewHandler(cx, ReadFulfilled, state));
  if (!readFulfilled) {
    return false;
  }

  Rooted<JSFunction*> readRejected(cx, NewHandler(cx, ReadRejected, state));
  if (!readRejected) {
    return false;
  }

  // Once the chunk is read, immediately write it and attempt to read more.
  // Don't bother handling a rejection: |source| will be closed/errored, and
  // |StartPiping| poised us to react to that already.
  if (!JS::AddPromiseReactionsIgnoringUnhandledRejection(
          cx, readRequest, readFulfilled, readRejected)) {
    return false;
  }

  // The spec is clear that a write started before an error/stream-closure is
  // encountered must be completed before shutdown.  It is *not* clear that a
  // read that hasn't yet fulfilled should delay shutdown (or until that read's
  // successive write is completed).
  //
  // It seems easiest to explain, both from a user perspective (no read is ever
  // just dropped on the ground) and an implementer perspective (if we *don't*
  // delay, then a read could be started, a shutdown could be started, then the
  // read could finish but we can't write it which arguably conflicts with the
  // requirement that chunks that have been read must be written before shutdown
  // completes), to delay.  XXX file a spec issue to require this!
  state->setReadPending();
  return true;
}

static bool ReadFromSource(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<PipeToState*> state(cx, TargetFromHandler<PipeToState>(args));
  cx->check(state);

  if (!ReadFromSource(cx, state)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static MOZ_MUST_USE bool StartPiping(JSContext* cx, Handle<PipeToState*> state,
                                     Handle<ReadableStream*> unwrappedSource,
                                     Handle<WritableStream*> unwrappedDest) {
  cx->check(state);

  // "Shutdown must stop activity: if shuttingDown becomes true, the user agent
  // must not initiate further reads from reader..."
  MOZ_ASSERT(!state->shuttingDown(), "can't be shutting down when starting");

  // "Error and close states must be propagated: the following conditions must
  // be applied in order."
  //
  // Before piping has started, we have to check for source/dest being errored
  // or closed manually.
  bool erroredOrClosed;
  if (!SourceOrDestErroredOrClosed(cx, state, unwrappedSource, unwrappedDest,
                                   &erroredOrClosed)) {
    return false;
  }
  if (erroredOrClosed) {
    return true;
  }

  // *After* piping has started, add reactions to respond to source/dest
  // becoming errored or closed.
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

  return ReadFromSource(cx, state);
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
