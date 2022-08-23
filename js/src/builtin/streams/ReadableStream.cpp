/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class ReadableStream. */

#include "builtin/streams/ReadableStream.h"

#include "mozilla/Maybe.h"  // mozilla::Maybe, mozilla::Some

#include "jspubtd.h"  // JSProto_ReadableStream

#include "builtin/Array.h"                   // js::NewDenseFullyAllocatedArray
#include "builtin/streams/ClassSpecMacro.h"  // JS_STREAMS_CLASS_SPEC
#include "builtin/streams/MiscellaneousOperations.h"  // js::MakeSizeAlgorithmFromSizeFunction, js::ValidateAndNormalizeHighWaterMark, js::ReturnPromiseRejectedWithPendingError
#include "builtin/streams/ReadableStreamController.h"  // js::ReadableStream{,Default}Controller, js::ReadableByteStreamController
#include "builtin/streams/ReadableStreamDefaultControllerOperations.h"  // js::SetUpReadableStreamDefaultControllerFromUnderlyingSource
#include "builtin/streams/ReadableStreamInternals.h"  // js::ReadableStreamCancel
#include "builtin/streams/ReadableStreamOperations.h"  // js::ReadableStream{PipeTo,Tee}
#include "builtin/streams/ReadableStreamReader.h"  // js::CreateReadableStream{BYOB,Default}Reader, js::ForAuthorCodeBool
#include "js/CallArgs.h"     // JS::CallArgs{,FromVp}
#include "js/Class.h"        // JSCLASS_SLOT0_IS_NSISUPPORTS, JS_NULL_CLASS_OPS
#include "js/Conversions.h"  // JS::ToBoolean
#include "js/ErrorReport.h"  // JS_ReportErrorNumberASCII
#include "js/friend/ErrorMessages.h"  // js::GetErrorMessage, JSMSG_*
#include "js/PropertySpec.h"  // JS{Function,Property}Spec, JS_FN, JS_PSG, JS_{FS,PS}_END
#include "js/RootingAPI.h"        // JS::Handle, JS::Rooted, js::CanGC
#include "js/Stream.h"            // JS::ReadableStream{Mode,UnderlyingSource}
#include "js/Value.h"             // JS::Value
#include "vm/Interpreter.h"       // js::GetProperty
#include "vm/JSContext.h"         // JSContext
#include "vm/JSObject.h"          // js::GetPrototypeFromBuiltinConstructor
#include "vm/ObjectOperations.h"  // js::GetProperty
#include "vm/PlainObject.h"       // js::PlainObject
#include "vm/Runtime.h"           // JSAtomState, JSRuntime
#include "vm/StringType.h"        // js::EqualStrings, js::ToString

#include "vm/Compartment-inl.h"   // js::UnwrapAndTypeCheck{Argument,This,Value}
#include "vm/JSObject-inl.h"      // js::NewBuiltinClassInstance
#include "vm/NativeObject-inl.h"  // js::ThrowIfNotConstructing

using mozilla::Maybe;
using mozilla::Some;

using js::CanGC;
using js::ClassSpec;
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
using js::UnwrapAndTypeCheckArgument;
using js::UnwrapAndTypeCheckThis;
using js::UnwrapAndTypeCheckValue;

using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::Handle;
using JS::ObjectValue;
using JS::Rooted;
using JS::Value;

/*** 3.2. Class ReadableStream **********************************************/

JS::ReadableStreamMode ReadableStream::mode() const {
  ReadableStreamController* controller = this->controller();
  if (controller->is<ReadableStreamDefaultController>()) {
    return JS::ReadableStreamMode::Default;
  }
  return controller->as<ReadableByteStreamController>().hasExternalSource()
             ? JS::ReadableStreamMode::ExternalSource
             : JS::ReadableStreamMode::Byte;
}

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
    JSObject* emptyObj = NewPlainObject(cx);
    if (!emptyObj) {
      return false;
    }
    underlyingSource = ObjectValue(*emptyObj);
  }

  Rooted<Value> strategy(cx, args.get(1));
  if (strategy.isUndefined()) {
    JSObject* emptyObj = NewPlainObject(cx);
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
[[nodiscard]] static bool ReadableStream_locked(JSContext* cx, unsigned argc,
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
[[nodiscard]] static bool ReadableStream_cancel(JSContext* cx, unsigned argc,
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
 * https://streams.spec.whatwg.org/#rs-get-reader
 * ReadableStreamReader getReader(optional ReadableStreamGetReaderOptions
 * options = {});
 */
[[nodiscard]] static bool ReadableStream_getReader(JSContext* cx, unsigned argc,
                                                   JS::Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  // Implicit |this| check.
  Rooted<ReadableStream*> unwrappedStream(
      cx, UnwrapAndTypeCheckThis<ReadableStream>(cx, args, "getReader"));
  if (!unwrappedStream) {
    return false;
  }

  // Implicit in the spec: Dictionary destructuring.
  // https://heycam.github.io/webidl/#es-dictionary
  // 3.2.17. Dictionary types

  Rooted<Value> optionsVal(cx, args.get(0));
  // Step 1.
  if (!optionsVal.isNullOrUndefined() && !optionsVal.isObject()) {
    ReportValueError(cx, JSMSG_CANT_CONVERT_TO, JSDVG_IGNORE_STACK, optionsVal,
                     nullptr, "dictionary");
    return false;
  }

  Maybe<JS::ReadableStreamReaderMode> mode;
  // Step 4: ...
  //
  // - Optimized for one dictionary member.
  // - Treat non-object options as non-existing "mode" member.
  if (optionsVal.isObject()) {
    Rooted<Value> modeVal(cx);
    if (!GetProperty(cx, optionsVal, cx->names().mode, &modeVal)) {
      return false;
    }

    // Step 4.1.3: If esMemberValue is not undefined, then: ...
    if (!modeVal.isUndefined()) {
      // https://heycam.github.io/webidl/#es-enumeration
      // 3.2.18. Enumeration types

      // Step 1:  Let S be the result of calling ToString(V).
      Rooted<JSString*> modeStr(cx, ToString<CanGC>(cx, modeVal));
      if (!modeStr) {
        return false;
      }

      // Step 2: If S is not one of E's enumeration values,
      //         then throw a TypeError.
      //
      // Note: We only have one valid value "byob".
      bool equal;
      if (!EqualStrings(cx, modeStr, cx->names().byob, &equal)) {
        return false;
      }
      if (!equal) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                  JSMSG_READABLESTREAM_INVALID_READER_MODE);
        return false;
      }

      mode = Some(JS::ReadableStreamReaderMode::Byob);
    }
  }

  // Step 1: If options["mode"] does not exist,
  //         return ? AcquireReadableStreamDefaultReader(this).
  Rooted<JSObject*> reader(cx);
  if (mode.isNothing()) {
    reader = CreateReadableStreamDefaultReader(cx, unwrappedStream,
                                               ForAuthorCodeBool::Yes);
  } else {
    // Step 2: Assert: options["mode"] is "byob".
    MOZ_ASSERT(mode.value() == JS::ReadableStreamReaderMode::Byob);

    // Step 3: Return ? AcquireReadableStreamBYOBReader(this).
    reader = CreateReadableStreamBYOBReader(cx, unwrappedStream,
                                            ForAuthorCodeBool::Yes);
  }

  if (!reader) {
    return false;
  }

  args.rval().setObject(*reader);
  return true;
}

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
    JS_FN("cancel", ReadableStream_cancel, 0, JSPROP_ENUMERATE),
    JS_FN("getReader", ReadableStream_getReader, 0, JSPROP_ENUMERATE),
    // pipeTo is only conditionally supported right now, so it must be manually
    // added below if desired.
    JS_FN("tee", ReadableStream_tee, 0, JSPROP_ENUMERATE), JS_FS_END};

static const JSPropertySpec ReadableStream_properties[] = {
    JS_PSG("locked", ReadableStream_locked, JSPROP_ENUMERATE),
    JS_STRING_SYM_PS(toStringTag, "ReadableStream", JSPROP_READONLY),
    JS_PS_END};

const ClassSpec ReadableStream::classSpec_ = {
    js::GenericCreateConstructor<ReadableStream::constructor, 0,
                                 js::gc::AllocKind::FUNCTION>,
    js::GenericCreatePrototype<ReadableStream>,
    nullptr,
    nullptr,
    ReadableStream_methods,
    ReadableStream_properties,
    nullptr,
    0};

const JSClass ReadableStream::class_ = {
    "ReadableStream",
    JSCLASS_HAS_RESERVED_SLOTS(ReadableStream::SlotCount) |
        JSCLASS_HAS_CACHED_PROTO(JSProto_ReadableStream) |
        JSCLASS_SLOT0_IS_NSISUPPORTS,
    JS_NULL_CLASS_OPS, &ReadableStream::classSpec_};

const JSClass ReadableStream::protoClass_ = {
    "ReadableStream.prototype",
    JSCLASS_HAS_CACHED_PROTO(JSProto_ReadableStream), JS_NULL_CLASS_OPS,
    &ReadableStream::classSpec_};
