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
#include "js/RootingAPI.h"  // JS::Handle, JS::Rooted
#include "js/Value.h"       // JS::Value, JS::ObjecValue

#include "vm/JSObject-inl.h"  // js::NewObjectWithClassProto
#include "vm/List-inl.h"      // js::StoreNewListInFixedSlot

using js::WritableStream;

using JS::Handle;
using JS::ObjectValue;
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
 * Streams spec, 4.4.14.
 *      WritableStreamUpdateBackpressure ( stream, backpressure )
 */
MOZ_MUST_USE bool js::WritableStreamUpdateBackpressure(
    JSContext* cx, Handle<WritableStream*> unwrappedStream, bool backpressure) {
  // XXX jwalden flesh me out!
  JS_ReportErrorASCII(cx, "epic fail");
  return false;
}
