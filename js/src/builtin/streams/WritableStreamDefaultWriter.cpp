/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStreamDefaultWriter. */

#include "builtin/streams/WritableStreamDefaultWriter-inl.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"        // JS_ReportErrorASCII, JS_ReportErrorNumberASCII
#include "jsfriendapi.h"  // js::GetErrorMessage, JSMSG_*

#include "builtin/streams/ClassSpecMacro.h"  // JS_STREAMS_CLASS_SPEC
#include "builtin/streams/MiscellaneousOperations.h"  // js::ReturnPromiseRejectedWithPendingError
#include "builtin/streams/WritableStream.h"           // js::WritableStream
#include "builtin/streams/WritableStreamOperations.h"  // js::WritableStreamCloseQueuedOrInFlight
#include "builtin/streams/WritableStreamWriterOperations.h"  // js::WritableStreamDefaultWriter{Abort,GetDesiredSize,Release,Write}
#include "js/CallArgs.h"                              // JS::CallArgs{,FromVp}
#include "js/Class.h"                        // js::ClassSpec, JS_NULL_CLASS_OPS
#include "js/PropertySpec.h"  // JS{Function,Property}Spec, JS_{FS,PS}_END, JS_{FN,PSG}
#include "js/RootingAPI.h"    // JS::Handle
#include "js/Value.h"         // JS::Value
#include "vm/Compartment.h"   // JS::Compartment
#include "vm/JSContext.h"     // JSContext

#include "vm/Compartment-inl.h"  // JS::Compartment::wrap, js::UnwrapAndTypeCheckThis

using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::Handle;
using JS::Rooted;
using JS::Value;

using js::ClassSpec;
using js::GetErrorMessage;
using js::ReturnPromiseRejectedWithPendingError;
using js::UnwrapAndTypeCheckThis;
using js::WritableStream;
using js::WritableStreamCloseQueuedOrInFlight;
using js::WritableStreamDefaultWriter;
using js::WritableStreamDefaultWriterGetDesiredSize;
using js::WritableStreamDefaultWriterRelease;
using js::WritableStreamDefaultWriterWrite;

/*** 4.5. Class WritableStreamDefaultWriter *********************************/

MOZ_MUST_USE WritableStreamDefaultWriter* js::CreateWritableStreamDefaultWriter(
    JSContext* cx, Handle<WritableStream*> unwrappedStream) {
  // XXX jwalden flesh me out!
  JS_ReportErrorASCII(cx, "epic fail");
  return nullptr;
}

/**
 * Streams spec, 4.5.
 * new WritableStreamDefaultWriter(stream)
 */
bool WritableStreamDefaultWriter::constructor(JSContext* cx, unsigned argc,
                                              Value* vp) {
  // XXX jwalden flesh me out!
  JS_ReportErrorASCII(cx, "epic fail");
  return false;
}

/**
 * Streams spec, 4.5.4.1. get closed
 */
static MOZ_MUST_USE bool WritableStream_closed(JSContext* cx, unsigned argc,
                                               Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsWritableStreamDefaultWriter(this) is false, return a promise
  //         rejected with a TypeError exception.
  Rooted<WritableStreamDefaultWriter*> unwrappedWriter(
      cx, UnwrapAndTypeCheckThis<WritableStreamDefaultWriter>(cx, args,
                                                              "get closed"));
  if (!unwrappedWriter) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 2: Return this.[[closedPromise]].
  Rooted<JSObject*> closedPromise(cx, unwrappedWriter->closedPromise());
  if (!cx->compartment()->wrap(cx, &closedPromise)) {
    return false;
  }

  args.rval().setObject(*closedPromise);
  return true;
}

/**
 * Streams spec, 4.5.4.2. get desiredSize
 */
static MOZ_MUST_USE bool WritableStream_desiredSize(JSContext* cx,
                                                    unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsWritableStreamDefaultWriter(this) is false, throw a
  //         TypeError exception.
  Rooted<WritableStreamDefaultWriter*> unwrappedWriter(
      cx, UnwrapAndTypeCheckThis<WritableStreamDefaultWriter>(
              cx, args, "get desiredSize"));
  if (!unwrappedWriter) {
    return false;
  }

  // Step 2: If this.[[ownerWritableStream]] is undefined, throw a TypeError
  //         exception.
  if (!unwrappedWriter->hasStream()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAMWRITER_NOT_OWNED,
                              "get desiredSize");
    return false;
  }

  // Step 3: Return ! WritableStreamDefaultWriterGetDesiredSize(this).
  if (!WritableStreamDefaultWriterGetDesiredSize(cx, unwrappedWriter,
                                                 args.rval())) {
    return false;
  }

  MOZ_ASSERT(args.rval().isNull() || args.rval().isNumber(),
             "expected a type that'll never require wrapping");
  return true;
}

/**
 * Streams spec, 4.5.4.3. get ready
 */
static MOZ_MUST_USE bool WritableStream_ready(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsWritableStreamDefaultWriter(this) is false, return a promise
  //         rejected with a TypeError exception.
  Rooted<WritableStreamDefaultWriter*> unwrappedWriter(
      cx, UnwrapAndTypeCheckThis<WritableStreamDefaultWriter>(cx, args,
                                                              "get ready"));
  if (!unwrappedWriter) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 2: Return this.[[readyPromise]].
  Rooted<JSObject*> readyPromise(cx, unwrappedWriter->readyPromise());
  if (!cx->compartment()->wrap(cx, &readyPromise)) {
    return false;
  }

  args.rval().setObject(*readyPromise);
  return true;
}

/**
 * Streams spec, 4.5.4.4. abort(reason)
 */
static MOZ_MUST_USE bool WritableStream_abort(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsWritableStreamDefaultWriter(this) is false, return a promise
  //         rejected with a TypeError exception.
  Rooted<WritableStreamDefaultWriter*> unwrappedWriter(
      cx,
      UnwrapAndTypeCheckThis<WritableStreamDefaultWriter>(cx, args, "abort"));
  if (!unwrappedWriter) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 2: If this.[[ownerWritableStream]] is undefined, return a promise
  //         rejected with a TypeError exception.
  if (!unwrappedWriter->hasStream()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAMWRITER_NOT_OWNED, "abort");
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 3: Return ! WritableStreamDefaultWriterAbort(this, reason).
  JSObject* promise =
      WritableStreamDefaultWriterAbort(cx, unwrappedWriter, args.get(0));
  if (!promise) {
    return false;
  }
  cx->check(promise);

  args.rval().setObject(*promise);
  return true;
}

/**
 * Streams spec, 4.5.4.5. close()
 */
static MOZ_MUST_USE bool WritableStream_close(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsWritableStreamDefaultWriter(this) is false, return a promise
  //         rejected with a TypeError exception.
  Rooted<WritableStreamDefaultWriter*> unwrappedWriter(
      cx,
      UnwrapAndTypeCheckThis<WritableStreamDefaultWriter>(cx, args, "close"));
  if (!unwrappedWriter) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 2: Let stream be this.[[ownerWritableStream]].
  // Step 3: If stream is undefined, return a promise rejected with a TypeError
  //         exception.
  if (!unwrappedWriter->hasStream()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAMWRITER_NOT_OWNED, "write");
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  WritableStream* unwrappedStream = UnwrapStreamFromWriter(cx, unwrappedWriter);
  if (!unwrappedStream) {
    return false;
  }

  // Step 4: If ! WritableStreamCloseQueuedOrInFlight(stream) is true, return a
  //         promise rejected with a TypeError exception.
  if (WritableStreamCloseQueuedOrInFlight(unwrappedStream)) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAM_CLOSE_CLOSING_OR_CLOSED);
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 5: Return ! WritableStreamDefaultWriterClose(this).
  JSObject* promise = WritableStreamDefaultWriterClose(cx, unwrappedWriter);
  if (!promise) {
    return false;
  }
  cx->check(promise);

  args.rval().setObject(*promise);
  return true;
}

/**
 * Streams spec, 4.5.4.6. releaseLock()
 */
static MOZ_MUST_USE bool WritableStream_releaseLock(JSContext* cx,
                                                    unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsWritableStreamDefaultWriter(this) is false, return a promise
  //         rejected with a TypeError exception.
  Rooted<WritableStreamDefaultWriter*> unwrappedWriter(
      cx,
      UnwrapAndTypeCheckThis<WritableStreamDefaultWriter>(cx, args, "close"));
  if (!unwrappedWriter) {
    return false;
  }

  // Step 2: Let stream be this.[[ownerWritableStream]].
  // Step 3: If stream is undefined, return.
  if (!unwrappedWriter->hasStream()) {
    args.rval().setUndefined();
    return true;
  }

  // Step 4: Assert: stream.[[writer]] is not undefined.
#ifdef DEBUG
  {
    WritableStream* unwrappedStream =
        UnwrapStreamFromWriter(cx, unwrappedWriter);
    if (!unwrappedStream) {
      return false;
    }
    MOZ_ASSERT(unwrappedStream->hasWriter());
  }
#endif

  // Step 5: Perform ! WritableStreamDefaultWriterRelease(this).
  if (!WritableStreamDefaultWriterRelease(cx, unwrappedWriter)) {
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/**
 * Streams spec, 4.5.4.7. write(chunk)
 */
static MOZ_MUST_USE bool WritableStream_write(JSContext* cx, unsigned argc,
                                              Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsWritableStreamDefaultWriter(this) is false, return a promise
  //         rejected with a TypeError exception.
  Rooted<WritableStreamDefaultWriter*> unwrappedWriter(
      cx,
      UnwrapAndTypeCheckThis<WritableStreamDefaultWriter>(cx, args, "write"));
  if (!unwrappedWriter) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 2: If this.[[ownerWritableStream]] is undefined, return a promise
  //         rejected with a TypeError exception.
  if (!unwrappedWriter->hasStream()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_WRITABLESTREAMWRITER_NOT_OWNED, "write");
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 3: Return this.[[readyPromise]].
  JSObject* promise =
      WritableStreamDefaultWriterWrite(cx, unwrappedWriter, args.get(0));
  if (!promise) {
    return false;
  }
  cx->check(promise);

  args.rval().setObject(*promise);
  return true;
}

static const JSPropertySpec WritableStreamDefaultWriter_properties[] = {
    JS_PSG("closed", WritableStream_closed, 0),
    JS_PSG("desiredSize", WritableStream_desiredSize, 0),
    JS_PSG("ready", WritableStream_ready, 0), JS_PS_END};

static const JSFunctionSpec WritableStreamDefaultWriter_methods[] = {
    JS_FN("abort", WritableStream_abort, 1, 0),
    JS_FN("close", WritableStream_close, 0, 0),
    JS_FN("releaseLock", WritableStream_releaseLock, 0, 0),
    JS_FN("write", WritableStream_write, 1, 0), JS_FS_END};

JS_STREAMS_CLASS_SPEC(WritableStreamDefaultWriter, 0, SlotCount,
                      ClassSpec::DontDefineConstructor, 0, JS_NULL_CLASS_OPS);
