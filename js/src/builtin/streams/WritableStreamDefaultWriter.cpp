/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStreamDefaultWriter. */

#include "builtin/streams/WritableStreamDefaultWriter.h"

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"  // JS_ReportErrorASCII

#include "builtin/streams/ClassSpecMacro.h"  // JS_STREAMS_CLASS_SPEC
#include "builtin/streams/MiscellaneousOperations.h"  // js::ReturnPromiseRejectedWithPendingError
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
using js::ReturnPromiseRejectedWithPendingError;
using js::UnwrapAndTypeCheckThis;
using js::WritableStreamDefaultWriter;

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

static const JSPropertySpec WritableStreamDefaultWriter_properties[] = {
    JS_PSG("closed", WritableStream_closed, 0), JS_PS_END};

static const JSFunctionSpec WritableStreamDefaultWriter_methods[] = {JS_FS_END};

JS_STREAMS_CLASS_SPEC(WritableStreamDefaultWriter, 0, SlotCount,
                      ClassSpec::DontDefineConstructor, 0, JS_NULL_CLASS_OPS);
