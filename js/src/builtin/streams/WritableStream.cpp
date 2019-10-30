/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStream. */

#include "builtin/streams/WritableStream.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"    // JS_ReportErrorNumberASCII
#include "jspubtd.h"  // JSProto_WritableStream

#include "builtin/streams/ClassSpecMacro.h"  // JS_STREAMS_CLASS_SPEC
#include "builtin/streams/MiscellaneousOperations.h"  // js::MakeSizeAlgorithmFromSizeFunction, js::ValidateAndNormalizeHighWaterMark
#include "builtin/streams/WritableStreamDefaultControllerOperations.h"  // js::SetUpWritableStreamDefaultControllerFromUnderlyingSink
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::CreateWritableStreamDefaultWriter
#include "js/CallArgs.h"  // JS::CallArgs{,FromVp}
#include "js/Class.h"  // JS{Function,Property}Spec, JS_{FS,PS}_END, JSCLASS_PRIVATE_IS_NSISUPPORTS, JSCLASS_HAS_PRIVATE, JS_NULL_CLASS_OPS
#include "js/RealmOptions.h"      // JS::RealmCreationOptions
#include "js/RootingAPI.h"        // JS::Handle, JS::Rooted
#include "js/Value.h"             // JS::{,Object}Value
#include "vm/JSContext.h"         // JSContext
#include "vm/JSObject.h"          // js::GetPrototypeFromBuiltinConstructor
#include "vm/NativeObject.h"      // js::PlainObject
#include "vm/ObjectOperations.h"  // js::GetProperty
#include "vm/Realm.h"             // JS::Realm

#include "vm/Compartment-inl.h"   // js::UnwrapAndTypeCheckThis
#include "vm/JSObject-inl.h"      // js::NewBuiltinClassInstance
#include "vm/NativeObject-inl.h"  // js::ThrowIfNotConstructing

using js::CreateWritableStreamDefaultWriter;
using js::UnwrapAndTypeCheckThis;
using js::WritableStream;

using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::Handle;
using JS::ObjectValue;
using JS::Rooted;
using JS::Value;

/*** 4.2. Class WritableStream **********************************************/

/**
 * Streams spec, 4.2.3. new WritableStream(underlyingSink = {}, strategy = {})
 */
bool WritableStream::constructor(JSContext* cx, unsigned argc, Value* vp) {
  MOZ_ASSERT(cx->realm()->creationOptions().getWritableStreamsEnabled(),
             "WritableStream should be enabled in this realm if we reach here");

  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "WritableStream")) {
    return false;
  }

  // Implicit in the spec: argument default values.
  Rooted<Value> underlyingSink(cx, args.get(0));
  if (underlyingSink.isUndefined()) {
    JSObject* emptyObj = NewBuiltinClassInstance<PlainObject>(cx);
    if (!emptyObj) {
      return false;
    }
    underlyingSink = ObjectValue(*emptyObj);
  }

  Rooted<Value> strategy(cx, args.get(1));
  if (strategy.isUndefined()) {
    JSObject* emptyObj = NewBuiltinClassInstance<PlainObject>(cx);
    if (!emptyObj) {
      return false;
    }
    strategy = ObjectValue(*emptyObj);
  }

  // Implicit in the spec: Set this to
  //     OrdinaryCreateFromConstructor(NewTarget, ...).
  // Step 1: Perform ! InitializeWritableStream(this).
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_WritableStream,
                                          &proto)) {
    return false;
  }
  Rooted<WritableStream*> stream(cx,
                                 WritableStream::create(cx, nullptr, proto));
  if (!stream) {
    return false;
  }

  // Step 2: Let size be ? GetV(strategy, "size").
  Rooted<Value> size(cx);
  if (!GetProperty(cx, strategy, cx->names().size, &size)) {
    return false;
  }

  // Step 3: Let highWaterMark be ? GetV(strategy, "highWaterMark").
  Rooted<Value> highWaterMarkVal(cx);
  if (!GetProperty(cx, strategy, cx->names().highWaterMark,
                   &highWaterMarkVal)) {
    return false;
  }

  // Step 4: Let type be ? GetV(underlyingSink, "type").
  Rooted<Value> type(cx);
  if (!GetProperty(cx, underlyingSink, cx->names().type, &type)) {
    return false;
  }

  // Step 5: If type is not undefined, throw a RangeError exception.
  if (!type.isUndefined()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_READABLESTREAM_UNDERLYINGSINK_TYPE_WRONG);
    return false;
  }

  // Step 6: Let sizeAlgorithm be ? MakeSizeAlgorithmFromSizeFunction(size).
  if (!MakeSizeAlgorithmFromSizeFunction(cx, size)) {
    return false;
  }

  // Step 7: If highWaterMark is undefined, let highWaterMark be 1.
  double highWaterMark;
  if (highWaterMarkVal.isUndefined()) {
    highWaterMark = 1;
  } else {
    // Step 8: Set highWaterMark to ?
    // ValidateAndNormalizeHighWaterMark(highWaterMark).
    if (!ValidateAndNormalizeHighWaterMark(cx, highWaterMarkVal,
                                           &highWaterMark)) {
      return false;
    }
  }

  // Step 9: Perform
  //         ? SetUpWritableStreamDefaultControllerFromUnderlyingSink(
  //         this, underlyingSink, highWaterMark, sizeAlgorithm).
  if (!SetUpWritableStreamDefaultControllerFromUnderlyingSink(
          cx, stream, underlyingSink, highWaterMark, size)) {
    return false;
  }

  args.rval().setObject(*stream);
  return true;
}

/**
 * Streams spec, 4.2.5.3. getWriter()
 */
static bool WritableStream_getWriter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! WritableStream(this) is false, throw a TypeError exception.
  Rooted<WritableStream*> unwrappedStream(
      cx, UnwrapAndTypeCheckThis<WritableStream>(cx, args, "getWriter"));
  if (!unwrappedStream) {
    return false;
  }

  auto* writer = CreateWritableStreamDefaultWriter(cx, unwrappedStream);
  if (!writer) {
    return false;
  }

  args.rval().setObject(*writer);
  return true;
}

static const JSFunctionSpec WritableStream_methods[] = {
    JS_FN("getWriter", WritableStream_getWriter, 0, 0), JS_FS_END};

static const JSPropertySpec WritableStream_properties[] = {JS_PS_END};

JS_STREAMS_CLASS_SPEC(WritableStream, 0, SlotCount, 0,
                      JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_HAS_PRIVATE,
                      JS_NULL_CLASS_OPS);
