/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream writer abstract operations. */

#include "builtin/streams/WritableStreamWriterOperations.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"        // JS_ReportErrorNumberASCII, JS_ReportErrorASCII
#include "jsfriendapi.h"  // js::GetErrorMessage, JSMSG_*

#include "builtin/Promise.h"                          // js::PromiseObject
#include "builtin/streams/MiscellaneousOperations.h"  // js::PromiseRejectedWithPendingError
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "builtin/streams/WritableStreamDefaultController.h"  // js::WritableStream::controller
#include "builtin/streams/WritableStreamDefaultControllerOperations.h"  // js::WritableStreamDefaultController{Close,GetDesiredSize}
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::WritableStreamDefaultWriter
#include "builtin/streams/WritableStreamOperations.h"  // js::WritableStreamCloseQueuedOrInFlight
#include "js/Value.h"  // JS::Value, JS::{Int32,Null}Value
#include "vm/Compartment.h"  // JS::Compartment
#include "vm/JSContext.h"    // JSContext

#include "builtin/streams/MiscellaneousOperations-inl.h"  // js::ResolveUnwrappedPromiseWithUndefined
#include "builtin/streams/WritableStream-inl.h"  // js::WritableStream::setCloseRequest
#include "builtin/streams/WritableStreamDefaultWriter-inl.h"  // js::UnwrapStreamFromWriter
#include "vm/Compartment-inl.h"  // js::UnwrapAndTypeCheckThis
#include "vm/JSContext-inl.h"    // JSContext::check
#include "vm/Realm-inl.h"        // js::AutoRealm

using JS::Handle;
using JS::Int32Value;
using JS::MutableHandle;
using JS::NullValue;
using JS::NumberValue;
using JS::Rooted;
using JS::Value;

using js::PromiseObject;

/*** 4.6. Writable stream writer abstract operations ************************/

/**
 * Streams spec, 4.6.3.
 * WritableStreamDefaultWriterClose ( writer )
 */
JSObject* js::WritableStreamDefaultWriterClose(
    JSContext* cx, Handle<WritableStreamDefaultWriter*> unwrappedWriter) {
  // Step 1: Let stream be writer.[[ownerWritableStream]].
  // Step 2: Assert: stream is not undefined.
  MOZ_ASSERT(unwrappedWriter->hasStream());
  Rooted<WritableStream*> unwrappedStream(
      cx, UnwrapStreamFromWriter(cx, unwrappedWriter));
  if (!unwrappedStream) {
    return PromiseRejectedWithPendingError(cx);
  }

  // Step 3: Let state be stream.[[state]].
  // Step 4: If state is "closed" or "errored", return a promise rejected with a
  //         TypeError exception.
  if (unwrappedStream->closed() || unwrappedStream->errored()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAM_CLOSED_OR_ERRORED);
    return PromiseRejectedWithPendingError(cx);
  }

  // Step 5: Assert: state is "writable" or "erroring".
  MOZ_ASSERT(unwrappedStream->writable() ^ unwrappedStream->erroring());

  // Step 6: Assert: ! WritableStreamCloseQueuedOrInFlight(stream) is false.
  MOZ_ASSERT(!WritableStreamCloseQueuedOrInFlight(unwrappedStream));

  // Step 7: Let promise be a new promise.
  Rooted<PromiseObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return nullptr;
  }

  // Step 8: Set stream.[[closeRequest]] to promise.
  {
    AutoRealm ar(cx, unwrappedStream);
    Rooted<JSObject*> closeRequest(cx, promise);
    if (!cx->compartment()->wrap(cx, &closeRequest)) {
      return nullptr;
    }

    unwrappedStream->setCloseRequest(closeRequest);
  }

  // Step 9: If stream.[[backpressure]] is true and state is "writable", resolve
  //         writer.[[readyPromise]] with undefined.
  if (unwrappedStream->backpressure() && unwrappedStream->writable()) {
    if (!ResolveUnwrappedPromiseWithUndefined(
            cx, unwrappedWriter->readyPromise())) {
      return nullptr;
    }
  }

  // Step 10: Perform
  //          ! WritableStreamDefaultControllerClose(
  //              stream.[[writableStreamController]]).
  Rooted<WritableStreamDefaultController*> unwrappedController(
      cx, unwrappedStream->controller());
  if (!WritableStreamDefaultControllerClose(cx, unwrappedController)) {
    return nullptr;
  }

  // Step 11: Return promise.
  return promise;
}

/**
 * Streams spec, 4.6.6.
 *  WritableStreamDefaultWriterEnsureReadyPromiseRejected( writer, error )
 */
MOZ_MUST_USE bool js::WritableStreamDefaultWriterEnsureReadyPromiseRejected(
    JSContext* cx, Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    Handle<Value> error) {
  cx->check(error);

  // XXX jwalden flesh me out!
  JS_ReportErrorASCII(cx, "epic fail");
  return false;
}

/**
 * Streams spec, 4.6.7.
 * WritableStreamDefaultWriterGetDesiredSize ( writer )
 */
bool js::WritableStreamDefaultWriterGetDesiredSize(
    JSContext* cx, Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    MutableHandle<Value> size) {
  // Step 1: Let stream be writer.[[ownerWritableStream]].
  const WritableStream* unwrappedStream =
      UnwrapStreamFromWriter(cx, unwrappedWriter);
  if (!unwrappedStream) {
    return false;
  }

  // Step 2: Let state be stream.[[state]].
  // Step 3: If state is "errored" or "erroring", return null.
  if (unwrappedStream->errored() || unwrappedStream->erroring()) {
    size.setNull();
  }
  // Step 4: If state is "closed", return 0.
  else if (unwrappedStream->closed()) {
    size.setInt32(0);
  }
  // Step 5: Return
  //         ! WritableStreamDefaultControllerGetDesiredSize(
  //             stream.[[writableStreamController]]).
  else {
    size.setNumber(WritableStreamDefaultControllerGetDesiredSize(
        unwrappedStream->controller()));
  }

  return true;
}

/**
 * Streams spec, 4.6.9.
 * WritableStreamDefaultWriterWrite ( writer, chunk )
 */
JSObject* js::WritableStreamDefaultWriterWrite(
    JSContext* cx, Handle<WritableStreamDefaultWriter*> unwrappedWriter,
    Handle<Value> chunk) {
  cx->check(chunk);

  // Step 1: Let stream be writer.[[ownerWritableStream]].
  // Step 2: Assert: stream is not undefined.
  MOZ_ASSERT(unwrappedWriter->hasStream());
  Rooted<WritableStream*> unwrappedStream(
      cx, UnwrapStreamFromWriter(cx, unwrappedWriter));
  if (!unwrappedStream) {
    return nullptr;
  }

  // Step 3: Let controller be stream.[[writableStreamController]].
  Rooted<WritableStreamDefaultController*> unwrappedController(
      cx, unwrappedStream->controller());

  // Step 4: Let chunkSize be
  //         ! WritableStreamDefaultControllerGetChunkSize(controller, chunk).
  Rooted<Value> chunkSize(cx);
  if (!WritableStreamDefaultControllerGetChunkSize(cx, unwrappedController,
                                                   chunk, &chunkSize)) {
    return nullptr;
  }
  cx->check(chunkSize);

  // Step 5: If stream is not equal to writer.[[ownerWritableStream]], return a
  //         promise rejected with a TypeError exception.
  // (This is just an obscure way of saying "If step 4 caused writer's lock on
  // stream to be released", or concretely, "If writer.[[ownerWritableStream]]
  // is [now, newly] undefined".)
  if (!unwrappedWriter->hasStream()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAM_RELEASED_DURING_WRITE);
    return PromiseRejectedWithPendingError(cx);
  }

  auto RejectWithStoredError =
      [](JSContext* cx, Handle<WritableStream*> unwrappedStream) -> JSObject* {
    Rooted<Value> storedError(cx, unwrappedStream->storedError());
    if (!cx->compartment()->wrap(cx, &storedError)) {
      return nullptr;
    }

    return PromiseObject::unforgeableReject(cx, storedError);
  };

  // Step 6: Let state be stream.[[state]].
  // Step 7: If state is "errored", return a promise rejected with
  //         stream.[[storedError]].
  if (unwrappedStream->errored()) {
    return RejectWithStoredError(cx, unwrappedStream);
  }

  // Step 8: If ! WritableStreamCloseQueuedOrInFlight(stream) is true or state
  //         is "closed", return a promise rejected with a TypeError exception
  //         indicating that the stream is closing or closed.
  if (WritableStreamCloseQueuedOrInFlight(unwrappedStream) ||
      unwrappedStream->closed()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAM_WRITE_CLOSING_OR_CLOSED);
    return PromiseRejectedWithPendingError(cx);
  }

  // Step 9: If state is "erroring", return a promise rejected with
  //         stream.[[storedError]].
  if (unwrappedStream->erroring()) {
    return RejectWithStoredError(cx, unwrappedStream);
  }

  // Step 10: Assert: state is "writable".
  MOZ_ASSERT(unwrappedStream->writable());

  // Step 11: Let promise be ! WritableStreamAddWriteRequest(stream).
  Rooted<PromiseObject*> promise(
      cx, WritableStreamAddWriteRequest(cx, unwrappedStream));
  if (!promise) {
    return nullptr;
  }

  // Step 12: Perform
  //          ! WritableStreamDefaultControllerWrite(controller, chunk,
  //                                                 chunkSize).
  if (!WritableStreamDefaultControllerWrite(cx, unwrappedController, chunk,
                                            chunkSize)) {
    return nullptr;
  }

  // Step 13: Return promise.
  return promise;
}
