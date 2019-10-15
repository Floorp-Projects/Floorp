/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStreamDefaultWriter. */

#include "builtin/streams/WritableStreamDefaultWriter.h"

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"        // JS_ReportErrorASCII, JS_ReportErrorNumberASCII
#include "jsfriendapi.h"  // js::GetErrorMessage, JSMSG_*

#include "builtin/streams/ClassSpecMacro.h"  // JS_STREAMS_CLASS_SPEC
#include "builtin/streams/MiscellaneousOperations.h"  // js::ReturnPromiseRejectedWithPendingError
#include "builtin/streams/WritableStreamWriterOperations.h"  // js::WritableStreamDefaultWriterGetDesiredSize
#include "js/CallArgs.h"                              // JS::CallArgs{,FromVp}
#include "js/Class.h"                        // js::ClassSpec, JS_NULL_CLASS_OPS
#include "js/PropertySpec.h"  // JS{Function,Property}Spec, JS_{FS,PS}_END
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
using js::WritableStreamDefaultWriter;
using js::WritableStreamDefaultWriterGetDesiredSize;

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
  Value v = WritableStreamDefaultWriterGetDesiredSize(unwrappedWriter);
  MOZ_ASSERT(v.isNull() || v.isNumber(),
             "expected a type that'll never require wrapping");
  args.rval().set(v);
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

static const JSPropertySpec WritableStreamDefaultWriter_properties[] = {
    JS_PSG("closed", WritableStream_closed, 0),
    JS_PSG("desiredSize", WritableStream_desiredSize, 0),
    JS_PSG("ready", WritableStream_ready, 0), JS_PS_END};

static const JSFunctionSpec WritableStreamDefaultWriter_methods[] = {JS_FS_END};

JS_STREAMS_CLASS_SPEC(WritableStreamDefaultWriter, 0, SlotCount,
                      ClassSpec::DontDefineConstructor, 0, JS_NULL_CLASS_OPS);
