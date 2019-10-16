/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream abstract operations. */

#include "builtin/streams/WritableStreamOperations.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"  // JS_ReportErrorASCII, JS_SetPrivate

#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "builtin/streams/WritableStreamDefaultController.h"  // js::WritableStreamDefaultController, js::WritableStream::controller
#include "js/Promise.h"      // JS::{Reject,Resolve}Promise
#include "js/RootingAPI.h"   // JS::Handle, JS::Rooted
#include "js/Value.h"        // JS::Value, JS::ObjecValue
#include "vm/Compartment.h"  // JS::Compartment
#include "vm/JSContext.h"    // JSContext

#include "builtin/streams/WritableStream-inl.h"  // js::WritableStream::writer
#include "vm/Compartment-inl.h"  // JS::Compartment::wrap
#include "vm/JSObject-inl.h"     // js::NewObjectWithClassProto
#include "vm/List-inl.h"         // js::StoreNewListInFixedSlot

using js::WritableStream;

using JS::Handle;
using JS::ObjectValue;
using JS::RejectPromise;
using JS::ResolvePromise;
using JS::Rooted;
using JS::Value;

/*** 4.3. General writable stream abstract operations. **********************/

/**
 * Streams spec, 4.3.4. InitializeWritableStream ( stream )
 */
MOZ_MUST_USE /* static */
    WritableStream*
    WritableStream::create(
        JSContext* cx, void* nsISupportsObject_alreadyAddreffed /* = nullptr */,
        Handle<JSObject*> proto /* = nullptr */) {
  // In the spec, InitializeWritableStream is always passed a newly created
  // WritableStream object. We instead create it here and return it below.
  Rooted<WritableStream*> stream(
      cx, NewObjectWithClassProto<WritableStream>(cx, proto));
  if (!stream) {
    return nullptr;
  }

  JS_SetPrivate(stream, nsISupportsObject_alreadyAddreffed);

  stream->initWritableState();

  // Step 1: Set stream.[[state]] to "writable".
  MOZ_ASSERT(stream->writable());

  // Step 2: Set stream.[[storedError]], stream.[[writer]],
  //         stream.[[writableStreamController]],
  //         stream.[[inFlightWriteRequest]], stream.[[closeRequest]],
  //         stream.[[inFlightCloseRequest]] and stream.[[pendingAbortRequest]]
  //         to undefined.
  MOZ_ASSERT(stream->storedError().isUndefined());
  MOZ_ASSERT(!stream->hasWriter());
  MOZ_ASSERT(!stream->hasController());
  MOZ_ASSERT(!stream->haveInFlightWriteRequest());
  MOZ_ASSERT(stream->inFlightWriteRequest().isUndefined());
  MOZ_ASSERT(stream->closeRequest().isUndefined());
  MOZ_ASSERT(stream->inFlightCloseRequest().isUndefined());
  MOZ_ASSERT(!stream->hasPendingAbortRequest());

  // Step 3: Set stream.[[writeRequests]] to a new empty List.
  if (!StoreNewListInFixedSlot(cx, stream,
                               WritableStream::Slot_WriteRequests)) {
    return nullptr;
  }

  // Step 4: Set stream.[[backpressure]] to false.
  MOZ_ASSERT(!stream->backpressure());

  return stream;
}

void WritableStream::clearInFlightWriteRequest(JSContext* cx) {
  MOZ_ASSERT(stateIsInitialized());
  MOZ_ASSERT(haveInFlightWriteRequest());

  writeRequests()->popFirst(cx);
  setFlag(HaveInFlightWriteRequest, false);

  MOZ_ASSERT(!haveInFlightWriteRequest());
  MOZ_ASSERT(inFlightWriteRequest().isUndefined());
}

/*** 4.4. Writable stream abstract operations used by controllers ***********/

/**
 * Streams spec, 4.4.2.
 *      WritableStreamDealWithRejection ( stream, error )
 */
MOZ_MUST_USE bool js::WritableStreamDealWithRejection(
    JSContext* cx, Handle<WritableStream*> unwrappedStream,
    Handle<Value> error) {
  // Step 1: Let state be stream.[[state]].
  // Step 2: If state is "writable",
  if (unwrappedStream->writable()) {
    // Step 2a: Perform ! WritableStreamStartErroring(stream, error).
    // Step 2b: Return.
    return WritableStreamStartErroring(cx, unwrappedStream, error);
  }

  // Step 3: Assert: state is "erroring".
  MOZ_ASSERT(unwrappedStream->erroring());

  // Step 4: Perform ! WritableStreamFinishErroring(stream).
  return WritableStreamFinishErroring(cx, unwrappedStream);
}

/**
 * Streams spec, 4.4.3.
 *      WritableStreamStartErroring ( stream, reason )
 */
MOZ_MUST_USE bool js::WritableStreamStartErroring(
    JSContext* cx, Handle<WritableStream*> unwrappedStream,
    Handle<Value> reason) {
  // Step 1: Assert: stream.[[storedError]] is undefined.
  MOZ_ASSERT(unwrappedStream->storedError().isUndefined());

  // Step 2: Assert: stream.[[state]] is "writable".
  MOZ_ASSERT(unwrappedStream->writable());

  // Step 3: Let controller be stream.[[writableStreamController]].
  // Step 4: Assert: controller is not undefined.
  MOZ_ASSERT(unwrappedStream->hasController());
  Rooted<WritableStreamDefaultController*> unwrappedController(
      cx, unwrappedStream->controller());

  // Step 5: Set stream.[[state]] to "erroring".
  unwrappedStream->setErroring();

  // Step 6: Set stream.[[storedError]] to reason.
  unwrappedStream->setStoredError(reason);

  // Step 7: Let writer be stream.[[writer]].
  // Step 8: If writer is not undefined, perform
  //         ! WritableStreamDefaultWriterEnsureReadyPromiseRejected(
  //             writer, reason).
  // Step 9: If ! WritableStreamHasOperationMarkedInFlight(stream) is false and
  //         controller.[[started]] is true, perform
  //         ! WritableStreamFinishErroring(stream).
  // XXX jwalden flesh me out!
  JS_ReportErrorASCII(cx, "epic fail");
  return false;
}

#ifdef DEBUG
static bool WritableStreamHasOperationMarkedInFlight(
    const WritableStream* unwrappedStream);
#endif

/**
 * Streams spec, 4.4.4.
 *      WritableStreamFinishErroring ( stream )
 */
MOZ_MUST_USE bool js::WritableStreamFinishErroring(
    JSContext* cx, Handle<WritableStream*> unwrappedStream) {
  // Step 1: Assert: stream.[[state]] is "erroring".
  MOZ_ASSERT(unwrappedStream->erroring());

  // Step 2: Assert: ! WritableStreamHasOperationMarkedInFlight(stream) is
  //         false.
  MOZ_ASSERT(!WritableStreamHasOperationMarkedInFlight(unwrappedStream));

  // Step 3: Set stream.[[state]] to "errored".
  unwrappedStream->setErrored();

  // Step 4: Perform ! stream.[[writableStreamController]].[[ErrorSteps]]().
  // Step 5: Let storedError be stream.[[storedError]].
  // Step 6: Repeat for each writeRequest that is an element of
  //         stream.[[writeRequests]],
  //   a: Reject writeRequest with storedError.
  // Step 7: Set stream.[[writeRequests]] to an empty List.
  // Step 8: If stream.[[pendingAbortRequest]] is undefined,
  //   a: Perform ! WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream).
  //   b: Return.
  // Step 9: Let abortRequest be stream.[[pendingAbortRequest]].
  // Step 10: Set stream.[[pendingAbortRequest]] to undefined.
  // Step 11: If abortRequest.[[wasAlreadyErroring]] is true,
  //   a: Reject abortRequest.[[promise]] with storedError.
  //   b: Perform ! WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream).
  //   c: Return.
  // Step 12: Let promise be
  //          ! stream.[[writableStreamController]].[[AbortSteps]](
  //                abortRequest.[[reason]]).
  // Step 13: Upon fulfillment of promise,
  //   a: Resolve abortRequest.[[promise]] with undefined.
  //   b: Perform ! WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream).
  // Step 14: Upon rejection of promise with reason reason,
  //   c: Reject abortRequest.[[promise]] with reason.
  //   d: Perform ! WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream).
  // XXX jwalden flesh me out!
  JS_ReportErrorASCII(cx, "epic fail");
  return false;
}

/**
 * Streams spec, 4.4.5.
 *      WritableStreamFinishInFlightWrite ( stream )
 */
MOZ_MUST_USE bool js::WritableStreamFinishInFlightWrite(
    JSContext* cx, Handle<WritableStream*> unwrappedStream) {
  // Step 1: Assert: stream.[[inFlightWriteRequest]] is not undefined.
  MOZ_ASSERT(unwrappedStream->haveInFlightWriteRequest());

  // Step 2: Resolve stream.[[inFlightWriteRequest]] with undefined.
  Rooted<JSObject*> writeRequest(
      cx, &unwrappedStream->inFlightWriteRequest().toObject());
  if (!cx->compartment()->wrap(cx, &writeRequest)) {
    return false;
  }
  if (!ResolvePromise(cx, writeRequest, UndefinedHandleValue)) {
    return false;
  }

  // Step 3: Set stream.[[inFlightWriteRequest]] to undefined.
  unwrappedStream->clearInFlightWriteRequest(cx);
  MOZ_ASSERT(!unwrappedStream->haveInFlightWriteRequest());

  return true;
}

/**
 * Streams spec, 4.4.6.
 *      WritableStreamFinishInFlightWriteWithError ( stream, error )
 */
MOZ_MUST_USE bool js::WritableStreamFinishInFlightWriteWithError(
    JSContext* cx, Handle<WritableStream*> unwrappedStream,
    Handle<Value> error) {
  // Step 1: Assert: stream.[[inFlightWriteRequest]] is not undefined.
  MOZ_ASSERT(unwrappedStream->haveInFlightWriteRequest());

  // Step 2:  Reject stream.[[inFlightWriteRequest]] with error.
  Rooted<JSObject*> writeRequest(
      cx, &unwrappedStream->inFlightWriteRequest().toObject());
  if (!cx->compartment()->wrap(cx, &writeRequest)) {
    return false;
  }
  if (!RejectPromise(cx, writeRequest, error)) {
    return false;
  }

  // Step 3:  Set stream.[[inFlightWriteRequest]] to undefined.
  unwrappedStream->clearInFlightWriteRequest(cx);

  // Step 4:  Assert: stream.[[state]] is "writable" or "erroring".
  MOZ_ASSERT(unwrappedStream->writable() ^ unwrappedStream->erroring());

  // Step 5:  Perform ! WritableStreamDealWithRejection(stream, error).
  return WritableStreamDealWithRejection(cx, unwrappedStream, error);
}

/**
 * Streams spec, 4.4.7.
 *      WritableStreamFinishInFlightClose ( stream )
 */
MOZ_MUST_USE bool js::WritableStreamFinishInFlightClose(
    JSContext* cx, Handle<WritableStream*> unwrappedStream) {
  // Step 1: Assert: stream.[[inFlightCloseRequest]] is not undefined.
  MOZ_ASSERT(unwrappedStream->haveInFlightCloseRequest());

  // Step 2: Resolve stream.[[inFlightCloseRequest]] with undefined.
  {
    Rooted<JSObject*> inFlightCloseRequest(
        cx, &unwrappedStream->inFlightCloseRequest().toObject());
    if (!cx->compartment()->wrap(cx, &inFlightCloseRequest)) {
      return false;
    }
    if (!ResolvePromise(cx, inFlightCloseRequest, UndefinedHandleValue)) {
      return false;
    }
  }

  // Step 3: Set stream.[[inFlightCloseRequest]] to undefined.
  unwrappedStream->clearInFlightCloseRequest();
  MOZ_ASSERT(unwrappedStream->inFlightCloseRequest().isUndefined());

  // Step 4: Let state be stream.[[state]].
  // Step 5: Assert: stream.[[state]] is "writable" or "erroring".
  MOZ_ASSERT(unwrappedStream->writable() ^ unwrappedStream->erroring());

  // Step 6: If state is "erroring",
  if (unwrappedStream->erroring()) {
    // Step 6.a: Set stream.[[storedError]] to undefined.
    unwrappedStream->clearStoredError();

    // Step 6.b: If stream.[[pendingAbortRequest]] is not undefined,
    if (unwrappedStream->hasPendingAbortRequest()) {
      // Step 6.b.i: Resolve stream.[[pendingAbortRequest]].[[promise]] with
      //             undefined.
      Rooted<JSObject*> pendingAbortRequestPromise(
          cx, unwrappedStream->pendingAbortRequestPromise());
      if (!cx->compartment()->wrap(cx, &pendingAbortRequestPromise)) {
        return false;
      }
      if (!ResolvePromise(cx, pendingAbortRequestPromise,
                          UndefinedHandleValue)) {
        return false;
      }

      // Step 6.b.ii: Set stream.[[pendingAbortRequest]] to undefined.
      unwrappedStream->clearPendingAbortRequest();
    }
  }

  // Step 7: Set stream.[[state]] to "closed".
  unwrappedStream->setClosed();

  // Step 8: Let writer be stream.[[writer]].
  // Step 9: If writer is not undefined, resolve writer.[[closedPromise]] with
  //         undefined.
  if (unwrappedStream->hasWriter()) {
    Rooted<JSObject*> closedPromise(cx,
                                    unwrappedStream->writer()->closedPromise());
    if (!cx->compartment()->wrap(cx, &closedPromise)) {
      return false;
    }
    if (!ResolvePromise(cx, closedPromise, UndefinedHandleValue)) {
      return false;
    }
  }

  // Step 10: Assert: stream.[[pendingAbortRequest]] is undefined.
  MOZ_ASSERT(!unwrappedStream->hasPendingAbortRequest());

  // Step 11: Assert: stream.[[storedError]] is undefined.
  MOZ_ASSERT(unwrappedStream->storedError().isUndefined());

  return true;
}

/**
 * Streams spec, 4.4.9.
 *      WritableStreamCloseQueuedOrInFlight ( stream )
 */
bool js::WritableStreamCloseQueuedOrInFlight(
    const WritableStream* unwrappedStream) {
  // Step 1: If stream.[[closeRequest]] is undefined and
  //         stream.[[inFlightCloseRequest]] is undefined, return false.
  // Step 2: Return true.
  return unwrappedStream->haveCloseRequestOrInFlightCloseRequest();
}

#ifdef DEBUG
/**
 * Streams spec, 4.4.10.
 *      WritableStreamHasOperationMarkedInFlight ( stream )
 */
bool WritableStreamHasOperationMarkedInFlight(
    const WritableStream* unwrappedStream) {
  // Step 1: If stream.[[inFlightWriteRequest]] is undefined and
  //         controller.[[inFlightCloseRequest]] is undefined, return false.
  // Step 2: Return true.
  return unwrappedStream->haveInFlightWriteRequest() ||
         unwrappedStream->haveInFlightCloseRequest();
}
#endif  // DEBUG

/**
 * Streams spec, 4.4.11.
 *      WritableStreamMarkCloseRequestInFlight ( stream )
 */
void js::WritableStreamMarkCloseRequestInFlight(
    WritableStream* unwrappedStream) {
  // Step 1: Assert: stream.[[inFlightCloseRequest]] is undefined.
  MOZ_ASSERT(!unwrappedStream->haveInFlightCloseRequest());

  // Step 2: Assert: stream.[[closeRequest]] is not undefined.
  MOZ_ASSERT(!unwrappedStream->closeRequest().isUndefined());

  // Step 3: Set stream.[[inFlightCloseRequest]] to stream.[[closeRequest]].
  // Step 4: Set stream.[[closeRequest]] to undefined.
  unwrappedStream->convertCloseRequestToInFlightCloseRequest();
}

/**
 * Streams spec, 4.4.14.
 *      WritableStreamUpdateBackpressure ( stream, backpressure )
 */
MOZ_MUST_USE bool js::WritableStreamUpdateBackpressure(
    JSContext* cx, Handle<WritableStream*> unwrappedStream, bool backpressure) {
  // XXX jwalden flesh me out!
  JS_ReportErrorASCII(cx, "epic fail");
  return false;
}
