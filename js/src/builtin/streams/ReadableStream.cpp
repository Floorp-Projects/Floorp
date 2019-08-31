/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class ReadableStream. */

#include "builtin/streams/ReadableStream.h"

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"        // JS_ReportErrorNumberASCII
#include "jsfriendapi.h"  // js::GetErrorMessage, JSMSG_*
#include "jspubtd.h"      // JSProto_ReadableStream

#include "builtin/Array.h"                   // js::NewDenseFullyAllocatedArray
#include "builtin/streams/ClassSpecMacro.h"  // JS_STREAMS_CLASS_SPEC
#include "builtin/streams/MiscellaneousOperations.h"  // js::MakeSizeAlgorithmFromSizeFunction, js::ValidateAndNormalizeHighWaterMark, js::ReturnPromiseRejectedWithPendingError
#include "builtin/streams/ReadableStreamInternals.h"  // js::ReadableStreamCancel
#include "builtin/streams/ReadableStreamOperations.h"  // js::ReadableStreamTee
#include "builtin/streams/ReadableStreamReader.h"  // js::CreateReadableStreamDefaultReader, js::ForAuthorCodeBool
#include "js/CallArgs.h"                           // JS::CallArgs{,FromVp}
#include "js/Class.h"  // JSCLASS_PRIVATE_IS_NSISUPPORTS, JSCLASS_HAS_PRIVATE, JS_NULL_CLASS_OPS
#include "js/PropertySpec.h"  // JS{Function,Property}Spec, JS_FN, JS_PSG, JS_{FS,PS}_END
#include "js/RootingAPI.h"        // JS::Handle, JS::Rooted, js::CanGC
#include "js/Stream.h"            // JS::ReadableStreamUnderlyingSource
#include "js/Value.h"             // JS::Value
#include "vm/JSContext.h"         // JSContext
#include "vm/JSObject.h"          // js::GetPrototypeFromBuiltinConstructor
#include "vm/NativeObject.h"      // js::PlainObject
#include "vm/ObjectOperations.h"  // js::GetProperty
#include "vm/Runtime.h"           // JSAtomState
#include "vm/StringType.h"        // js::EqualStrings, js::ToString

#include "vm/Compartment-inl.h"   // js::UnwrapAndTypeCheckThis
#include "vm/JSObject-inl.h"      // js::NewBuiltinClassInstance
#include "vm/NativeObject-inl.h"  // js::ThrowIfNotConstructing

using js::CanGC;
using js::CreateReadableStreamDefaultReader;
using js::EqualStrings;
using js::ForAuthorCodeBool;
using js::GetErrorMessage;
using js::NativeObject;
using js::NewBuiltinClassInstance;
using js::NewDenseFullyAllocatedArray;
using js::PlainObject;
using js::ReadableStream;
using js::ReadableStreamTee;
using js::ReturnPromiseRejectedWithPendingError;
using js::ToString;
using js::UnwrapAndTypeCheckThis;

using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::Handle;
using JS::ObjectValue;
using JS::Rooted;
using JS::Value;

/*** 3.2. Class ReadableStream **********************************************/

ReadableStream* ReadableStream::createExternalSourceStream(
    JSContext* cx, JS::ReadableStreamUnderlyingSource* source,
    void* nsISupportsObject_alreadyAddreffed /* = nullptr */,
    Handle<JSObject*> proto /* = nullptr */) {
  Rooted<ReadableStream*> stream(
      cx, create(cx, nsISupportsObject_alreadyAddreffed, proto));
  if (!stream) {
    return nullptr;
  }

  if (!SetUpExternalReadableByteStreamController(cx, stream, source)) {
    return nullptr;
  }

  return stream;
}

/**
 * Streams spec, 3.2.3. new ReadableStream(underlyingSource = {}, strategy = {})
 */
bool ReadableStream::constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (!ThrowIfNotConstructing(cx, args, "ReadableStream")) {
    return false;
  }

  // Implicit in the spec: argument default values.
  Rooted<Value> underlyingSource(cx, args.get(0));
  if (underlyingSource.isUndefined()) {
    JSObject* emptyObj = NewBuiltinClassInstance<PlainObject>(cx);
    if (!emptyObj) {
      return false;
    }
    underlyingSource = ObjectValue(*emptyObj);
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
  // Step 1: Perform ! InitializeReadableStream(this).
  Rooted<JSObject*> proto(cx);
  if (!GetPrototypeFromBuiltinConstructor(cx, args, JSProto_ReadableStream,
                                          &proto)) {
    return false;
  }
  Rooted<ReadableStream*> stream(cx,
                                 ReadableStream::create(cx, nullptr, proto));
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

  // Step 4: Let type be ? GetV(underlyingSource, "type").
  Rooted<Value> type(cx);
  if (!GetProperty(cx, underlyingSource, cx->names().type, &type)) {
    return false;
  }

  // Step 5: Let typeString be ? ToString(type).
  Rooted<JSString*> typeString(cx, ToString<CanGC>(cx, type));
  if (!typeString) {
    return false;
  }

  // Step 6: If typeString is "bytes",
  bool equal;
  if (!EqualStrings(cx, typeString, cx->names().bytes, &equal)) {
    return false;
  }
  if (equal) {
    // The rest of step 6 is unimplemented, since we don't support
    // user-defined byte streams yet.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_READABLESTREAM_BYTES_TYPE_NOT_IMPLEMENTED);
    return false;
  }

  // Step 7: Otherwise, if type is undefined,
  if (type.isUndefined()) {
    // Step 7.a: Let sizeAlgorithm be ? MakeSizeAlgorithmFromSizeFunction(size).
    if (!MakeSizeAlgorithmFromSizeFunction(cx, size)) {
      return false;
    }

    // Step 7.b: If highWaterMark is undefined, let highWaterMark be 1.
    double highWaterMark;
    if (highWaterMarkVal.isUndefined()) {
      highWaterMark = 1;
    } else {
      // Step 7.c: Set highWaterMark to ?
      // ValidateAndNormalizeHighWaterMark(highWaterMark).
      if (!ValidateAndNormalizeHighWaterMark(cx, highWaterMarkVal,
                                             &highWaterMark)) {
        return false;
      }
    }

    // Step 7.d: Perform
    //           ? SetUpReadableStreamDefaultControllerFromUnderlyingSource(
    //           this, underlyingSource, highWaterMark, sizeAlgorithm).
    if (!SetUpReadableStreamDefaultControllerFromUnderlyingSource(
            cx, stream, underlyingSource, highWaterMark, size)) {
      return false;
    }

    args.rval().setObject(*stream);
    return true;
  }

  // Step 8: Otherwise, throw a RangeError exception.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_READABLESTREAM_UNDERLYINGSOURCE_TYPE_WRONG);
  return false;
}

/**
 * Streams spec, 3.2.5.1. get locked
 */
static MOZ_MUST_USE bool ReadableStream_locked(JSContext* cx, unsigned argc,
                                               JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception.
  Rooted<ReadableStream*> unwrappedStream(
      cx, UnwrapAndTypeCheckThis<ReadableStream>(cx, args, "get locked"));
  if (!unwrappedStream) {
    return false;
  }

  // Step 2: Return ! IsReadableStreamLocked(this).
  args.rval().setBoolean(unwrappedStream->locked());
  return true;
}

/**
 * Streams spec, 3.2.5.2. cancel ( reason )
 */
static MOZ_MUST_USE bool ReadableStream_cancel(JSContext* cx, unsigned argc,
                                               JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsReadableStream(this) is false, return a promise rejected
  //         with a TypeError exception.
  Rooted<ReadableStream*> unwrappedStream(
      cx, UnwrapAndTypeCheckThis<ReadableStream>(cx, args, "cancel"));
  if (!unwrappedStream) {
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 2: If ! IsReadableStreamLocked(this) is true, return a promise
  //         rejected with a TypeError exception.
  if (unwrappedStream->locked()) {
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_READABLESTREAM_LOCKED_METHOD, "cancel");
    return ReturnPromiseRejectedWithPendingError(cx, args);
  }

  // Step 3: Return ! ReadableStreamCancel(this, reason).
  Rooted<JSObject*> cancelPromise(
      cx, js::ReadableStreamCancel(cx, unwrappedStream, args.get(0)));
  if (!cancelPromise) {
    return false;
  }
  args.rval().setObject(*cancelPromise);
  return true;
}

// Streams spec, 3.2.5.3.
//      getIterator({ preventCancel } = {})
//
// Not implemented.

/**
 * Streams spec, 3.2.5.4. getReader({ mode } = {})
 */
static MOZ_MUST_USE bool ReadableStream_getReader(JSContext* cx, unsigned argc,
                                                  JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Implicit in the spec: Argument defaults and destructuring.
  Rooted<Value> optionsVal(cx, args.get(0));
  if (optionsVal.isUndefined()) {
    JSObject* emptyObj = NewBuiltinClassInstance<PlainObject>(cx);
    if (!emptyObj) {
      return false;
    }
    optionsVal.setObject(*emptyObj);
  }
  Rooted<Value> modeVal(cx);
  if (!GetProperty(cx, optionsVal, cx->names().mode, &modeVal)) {
    return false;
  }

  // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception.
  Rooted<ReadableStream*> unwrappedStream(
      cx, UnwrapAndTypeCheckThis<ReadableStream>(cx, args, "getReader"));
  if (!unwrappedStream) {
    return false;
  }

  // Step 2: If mode is undefined, return
  //         ? AcquireReadableStreamDefaultReader(this).
  Rooted<JSObject*> reader(cx);
  if (modeVal.isUndefined()) {
    reader = CreateReadableStreamDefaultReader(cx, unwrappedStream,
                                               ForAuthorCodeBool::Yes);
  } else {
    // Step 3: Set mode to ? ToString(mode) (implicit).
    Rooted<JSString*> mode(cx, ToString<CanGC>(cx, modeVal));
    if (!mode) {
      return false;
    }

    // Step 4: If mode is "byob",
    //         return ? AcquireReadableStreamBYOBReader(this).
    bool equal;
    if (!EqualStrings(cx, mode, cx->names().byob, &equal)) {
      return false;
    }
    if (equal) {
      // BYOB readers aren't implemented yet.
      JS_ReportErrorNumberASCII(
          cx, GetErrorMessage, nullptr,
          JSMSG_READABLESTREAM_BYTES_TYPE_NOT_IMPLEMENTED);
      return false;
    }

    // Step 5: Throw a RangeError exception.
    JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                              JSMSG_READABLESTREAM_INVALID_READER_MODE);
    return false;
  }

  // Reordered second part of steps 2 and 4.
  if (!reader) {
    return false;
  }
  args.rval().setObject(*reader);
  return true;
}

// Streams spec, 3.2.5.5.
//      pipeThrough({ writable, readable },
//                  { preventClose, preventAbort, preventCancel, signal })
//
// Not implemented.

// Streams spec, 3.2.5.6.
//      pipeTo(dest, { preventClose, preventAbort, preventCancel, signal } = {})
//
// Not implemented.

/**
 * Streams spec, 3.2.5.7. tee()
 */
static bool ReadableStream_tee(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Step 1: If ! IsReadableStream(this) is false, throw a TypeError exception.
  Rooted<ReadableStream*> unwrappedStream(
      cx, UnwrapAndTypeCheckThis<ReadableStream>(cx, args, "tee"));
  if (!unwrappedStream) {
    return false;
  }

  // Step 2: Let branches be ? ReadableStreamTee(this, false).
  Rooted<ReadableStream*> branch1(cx);
  Rooted<ReadableStream*> branch2(cx);
  if (!ReadableStreamTee(cx, unwrappedStream, false, &branch1, &branch2)) {
    return false;
  }

  // Step 3: Return ! CreateArrayFromList(branches).
  Rooted<NativeObject*> branches(cx, NewDenseFullyAllocatedArray(cx, 2));
  if (!branches) {
    return false;
  }
  branches->setDenseInitializedLength(2);
  branches->initDenseElement(0, ObjectValue(*branch1));
  branches->initDenseElement(1, ObjectValue(*branch2));

  args.rval().setObject(*branches);
  return true;
}

// Streams spec, 3.2.5.8.
//      [@@asyncIterator]({ preventCancel } = {})
//
// Not implemented.

static const JSFunctionSpec ReadableStream_methods[] = {
    JS_FN("cancel", ReadableStream_cancel, 1, 0),
    JS_FN("getReader", ReadableStream_getReader, 0, 0),
    JS_FN("tee", ReadableStream_tee, 0, 0), JS_FS_END};

static const JSPropertySpec ReadableStream_properties[] = {
    JS_PSG("locked", ReadableStream_locked, 0), JS_PS_END};

JS_STREAMS_CLASS_SPEC(ReadableStream, 0, SlotCount, 0,
                      JSCLASS_PRIVATE_IS_NSISUPPORTS | JSCLASS_HAS_PRIVATE,
                      JS_NULL_CLASS_OPS);
