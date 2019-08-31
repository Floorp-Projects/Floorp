/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The interface between readable streams and controllers. */

#include "builtin/streams/ReadableStreamInternals.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT{,_IF}
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include <stdint.h>  // uint32_t

#include "jsfriendapi.h"  // js::AssertSameCompartment

#include "builtin/Promise.h"  // js::PromiseObject
#include "builtin/Stream.h"   // js::ReadableStreamController{,CancelSteps}
#include "builtin/streams/ReadableStreamReader.h"  // js::ReadableStream{,Default}Reader, js::ForAuthorCodeBool
#include "gc/AllocKind.h"  // js::gc::AllocKind
#include "js/CallArgs.h"   // JS::CallArgs{,FromVp}
#include "js/GCAPI.h"      // JS::AutoSuppressGCAnalysis
#include "js/Promise.h"  // JS::CallOriginalPromiseThen, JS::{Resolve,Reject}Promise
#include "js/Result.h"      // JS_TRY_VAR_OR_RETURN_NULL
#include "js/RootingAPI.h"  // JS::Handle, JS::Rooted
#include "js/Stream.h"  // JS::ReadableStreamUnderlyingSource, JS::ReadableStreamMode
#include "js/Value.h"  // JS::Value, JS::{Boolean,Object}Value, JS::UndefinedHandleValue
#include "vm/JSContext.h"     // JSContext
#include "vm/JSFunction.h"    // JSFunction, js::NewNativeFunction
#include "vm/NativeObject.h"  // js::NativeObject
#include "vm/ObjectGroup.h"   // js::GenericObject
#include "vm/Realm.h"         // JS::Realm
#include "vm/StringType.h"    // js::PropertyName

#include "builtin/streams/ReadableStreamReader-inl.h"  // js::js::UnwrapReaderFromStream{,NoThrow}
#include "vm/Compartment-inl.h"                        // JS::Compartment::wrap
#include "vm/JSContext-inl.h"                          // JSContext::check
#include "vm/List-inl.h"  // js::ListObject, js::AppendToListInFixedSlot, js::StoreNewListInFixedSlot
#include "vm/Realm-inl.h"  // JS::Realm

using js::ReadableStream;

using JS::BooleanValue;
using JS::CallArgs;
using JS::CallArgsFromVp;
using JS::Handle;
using JS::ObjectValue;
using JS::ResolvePromise;
using JS::Rooted;
using JS::UndefinedHandleValue;
using JS::Value;

JS::ReadableStreamMode ReadableStream::mode() const {
  ReadableStreamController* controller = this->controller();
  if (controller->is<ReadableStreamDefaultController>()) {
    return JS::ReadableStreamMode::Default;
  }
  return controller->as<ReadableByteStreamController>().hasExternalSource()
             ? JS::ReadableStreamMode::ExternalSource
             : JS::ReadableStreamMode::Byte;
}

/*** 3.5. The interface between readable streams and controllers ************/

/**
 * Streams spec, 3.5.1.
 *      ReadableStreamAddReadIntoRequest ( stream, forAuthorCode )
 * Streams spec, 3.5.2.
 *      ReadableStreamAddReadRequest ( stream, forAuthorCode )
 *
 * Our implementation does not pass around forAuthorCode parameters in the same
 * places as the standard, but the effect is the same. See the comment on
 * `ReadableStreamReader::forAuthorCode()`.
 */
MOZ_MUST_USE JSObject* js::ReadableStreamAddReadOrReadIntoRequest(
    JSContext* cx, Handle<ReadableStream*> unwrappedStream) {
  // Step 1: Assert: ! IsReadableStream{BYOB,Default}Reader(stream.[[reader]])
  //         is true.
  // (Only default readers exist so far.)
  Rooted<ReadableStreamReader*> unwrappedReader(
      cx, UnwrapReaderFromStream(cx, unwrappedStream));
  if (!unwrappedReader) {
    return nullptr;
  }
  MOZ_ASSERT(unwrappedReader->is<ReadableStreamDefaultReader>());

  // Step 2 of 3.5.1: Assert: stream.[[state]] is "readable" or "closed".
  // Step 2 of 3.5.2: Assert: stream.[[state]] is "readable".
  MOZ_ASSERT(unwrappedStream->readable() || unwrappedStream->closed());
  MOZ_ASSERT_IF(unwrappedReader->is<ReadableStreamDefaultReader>(),
                unwrappedStream->readable());

  // Step 3: Let promise be a new promise.
  Rooted<JSObject*> promise(cx, PromiseObject::createSkippingExecutor(cx));
  if (!promise) {
    return nullptr;
  }

  // Step 4: Let read{Into}Request be
  //         Record {[[promise]]: promise, [[forAuthorCode]]: forAuthorCode}.
  // Step 5: Append read{Into}Request as the last element of
  //         stream.[[reader]].[[read{Into}Requests]].
  // Since we don't need the [[forAuthorCode]] field (see the comment on
  // `ReadableStreamReader::forAuthorCode()`), we elide the Record and store
  // only the promise.
  if (!AppendToListInFixedSlot(cx, unwrappedReader,
                               ReadableStreamReader::Slot_Requests, promise)) {
    return nullptr;
  }

  // Step 6: Return promise.
  return promise;
}

/**
 * Used for transforming the result of promise fulfillment/rejection.
 */
static bool ReturnUndefined(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  args.rval().setUndefined();
  return true;
}

/**
 * Streams spec, 3.5.3. ReadableStreamCancel ( stream, reason )
 */
MOZ_MUST_USE JSObject* js::ReadableStreamCancel(
    JSContext* cx, Handle<ReadableStream*> unwrappedStream,
    Handle<Value> reason) {
  AssertSameCompartment(cx, reason);

  // Step 1: Set stream.[[disturbed]] to true.
  unwrappedStream->setDisturbed();

  // Step 2: If stream.[[state]] is "closed", return a new promise resolved
  //         with undefined.
  if (unwrappedStream->closed()) {
    return PromiseObject::unforgeableResolve(cx, UndefinedHandleValue);
  }

  // Step 3: If stream.[[state]] is "errored", return a new promise rejected
  //         with stream.[[storedError]].
  if (unwrappedStream->errored()) {
    Rooted<Value> storedError(cx, unwrappedStream->storedError());
    if (!cx->compartment()->wrap(cx, &storedError)) {
      return nullptr;
    }
    return PromiseObject::unforgeableReject(cx, storedError);
  }

  // Step 4: Perform ! ReadableStreamClose(stream).
  if (!ReadableStreamCloseInternal(cx, unwrappedStream)) {
    return nullptr;
  }

  // Step 5: Let sourceCancelPromise be
  //         ! stream.[[readableStreamController]].[[CancelSteps]](reason).
  Rooted<ReadableStreamController*> unwrappedController(
      cx, unwrappedStream->controller());
  Rooted<JSObject*> sourceCancelPromise(
      cx, ReadableStreamControllerCancelSteps(cx, unwrappedController, reason));
  if (!sourceCancelPromise) {
    return nullptr;
  }

  // Step 6: Return the result of transforming sourceCancelPromise with a
  //         fulfillment handler that returns undefined.
  Handle<PropertyName*> funName = cx->names().empty;
  Rooted<JSFunction*> returnUndefined(
      cx, NewNativeFunction(cx, ReturnUndefined, 0, funName,
                            gc::AllocKind::FUNCTION, GenericObject));
  if (!returnUndefined) {
    return nullptr;
  }
  return JS::CallOriginalPromiseThen(cx, sourceCancelPromise, returnUndefined,
                                     nullptr);
}

/**
 * Streams spec, 3.5.4. ReadableStreamClose ( stream )
 */
MOZ_MUST_USE bool js::ReadableStreamCloseInternal(
    JSContext* cx, Handle<ReadableStream*> unwrappedStream) {
  // Step 1: Assert: stream.[[state]] is "readable".
  MOZ_ASSERT(unwrappedStream->readable());

  // Step 2: Set stream.[[state]] to "closed".
  unwrappedStream->setClosed();

  // Step 4: If reader is undefined, return (reordered).
  if (!unwrappedStream->hasReader()) {
    return true;
  }

  // Step 3: Let reader be stream.[[reader]].
  Rooted<ReadableStreamReader*> unwrappedReader(
      cx, UnwrapReaderFromStream(cx, unwrappedStream));
  if (!unwrappedReader) {
    return false;
  }

  // Step 5: If ! IsReadableStreamDefaultReader(reader) is true,
  if (unwrappedReader->is<ReadableStreamDefaultReader>()) {
    ForAuthorCodeBool forAuthorCode = unwrappedReader->forAuthorCode();

    // Step a: Repeat for each readRequest that is an element of
    //         reader.[[readRequests]],
    Rooted<ListObject*> unwrappedReadRequests(cx, unwrappedReader->requests());
    uint32_t len = unwrappedReadRequests->length();
    Rooted<JSObject*> readRequest(cx);
    Rooted<JSObject*> resultObj(cx);
    Rooted<Value> resultVal(cx);
    for (uint32_t i = 0; i < len; i++) {
      // Step i: Resolve readRequest.[[promise]] with
      //         ! ReadableStreamCreateReadResult(undefined, true,
      //                                          readRequest.[[forAuthorCode]]).
      readRequest = &unwrappedReadRequests->getAs<JSObject>(i);
      if (!cx->compartment()->wrap(cx, &readRequest)) {
        return false;
      }

      resultObj = js::ReadableStreamCreateReadResult(cx, UndefinedHandleValue,
                                                     true, forAuthorCode);
      if (!resultObj) {
        return false;
      }
      resultVal = ObjectValue(*resultObj);
      if (!ResolvePromise(cx, readRequest, resultVal)) {
        return false;
      }
    }

    // Step b: Set reader.[[readRequests]] to an empty List.
    unwrappedReader->clearRequests();
  }

  // Step 6: Resolve reader.[[closedPromise]] with undefined.
  Rooted<JSObject*> closedPromise(cx, unwrappedReader->closedPromise());
  if (!cx->compartment()->wrap(cx, &closedPromise)) {
    return false;
  }
  if (!ResolvePromise(cx, closedPromise, UndefinedHandleValue)) {
    return false;
  }

  if (unwrappedStream->mode() == JS::ReadableStreamMode::ExternalSource) {
    // Make sure we're in the stream's compartment.
    AutoRealm ar(cx, unwrappedStream);
    JS::ReadableStreamUnderlyingSource* source =
        unwrappedStream->controller()->externalSource();
    source->onClosed(cx, unwrappedStream);
  }

  return true;
}

/**
 * Streams spec, 3.5.5. ReadableStreamCreateReadResult ( value, done,
 *                                                       forAuthorCode )
 */
MOZ_MUST_USE JSObject* js::ReadableStreamCreateReadResult(
    JSContext* cx, Handle<Value> value, bool done,
    ForAuthorCodeBool forAuthorCode) {
  // Step 1: Let prototype be null.
  // Step 2: If forAuthorCode is true, set prototype to %ObjectPrototype%.
  Rooted<JSObject*> templateObject(
      cx,
      forAuthorCode == ForAuthorCodeBool::Yes
          ? cx->realm()->getOrCreateIterResultTemplateObject(cx)
          : cx->realm()->getOrCreateIterResultWithoutPrototypeTemplateObject(
                cx));
  if (!templateObject) {
    return nullptr;
  }

  // Step 3: Assert: Type(done) is Boolean (implicit).

  // Step 4: Let obj be ObjectCreate(prototype).
  NativeObject* obj;
  JS_TRY_VAR_OR_RETURN_NULL(
      cx, obj, NativeObject::createWithTemplate(cx, templateObject));

  // Step 5: Perform CreateDataProperty(obj, "value", value).
  obj->setSlot(Realm::IterResultObjectValueSlot, value);

  // Step 6: Perform CreateDataProperty(obj, "done", done).
  obj->setSlot(Realm::IterResultObjectDoneSlot, BooleanValue(done));

  // Step 7: Return obj.
  return obj;
}

/**
 * Streams spec, 3.5.6. ReadableStreamError ( stream, e )
 */
MOZ_MUST_USE bool js::ReadableStreamErrorInternal(
    JSContext* cx, Handle<ReadableStream*> unwrappedStream, Handle<Value> e) {
  // Step 1: Assert: ! IsReadableStream(stream) is true (implicit).

  // Step 2: Assert: stream.[[state]] is "readable".
  MOZ_ASSERT(unwrappedStream->readable());

  // Step 3: Set stream.[[state]] to "errored".
  unwrappedStream->setErrored();

  // Step 4: Set stream.[[storedError]] to e.
  {
    AutoRealm ar(cx, unwrappedStream);
    Rooted<Value> wrappedError(cx, e);
    if (!cx->compartment()->wrap(cx, &wrappedError)) {
      return false;
    }
    unwrappedStream->setStoredError(wrappedError);
  }

  // Step 6: If reader is undefined, return (reordered).
  if (!unwrappedStream->hasReader()) {
    return true;
  }

  // Step 5: Let reader be stream.[[reader]].
  Rooted<ReadableStreamReader*> unwrappedReader(
      cx, UnwrapReaderFromStream(cx, unwrappedStream));
  if (!unwrappedReader) {
    return false;
  }

  // Steps 7-8: (Identical in our implementation.)
  // Step 7.a/8.b: Repeat for each read{Into}Request that is an element of
  //               reader.[[read{Into}Requests]],
  Rooted<ListObject*> unwrappedReadRequests(cx, unwrappedReader->requests());
  Rooted<JSObject*> readRequest(cx);
  Rooted<Value> val(cx);
  uint32_t len = unwrappedReadRequests->length();
  for (uint32_t i = 0; i < len; i++) {
    // Step i: Reject read{Into}Request.[[promise]] with e.
    val = unwrappedReadRequests->get(i);
    readRequest = &val.toObject();

    // Responses have to be created in the compartment from which the
    // error was triggered, which might not be the same as the one the
    // request was created in, so we have to wrap requests here.
    if (!cx->compartment()->wrap(cx, &readRequest)) {
      return false;
    }

    if (!RejectPromise(cx, readRequest, e)) {
      return false;
    }
  }

  // Step 7.b/8.c: Set reader.[[read{Into}Requests]] to a new empty List.
  if (!StoreNewListInFixedSlot(cx, unwrappedReader,
                               ReadableStreamReader::Slot_Requests)) {
    return false;
  }

  // Step 9: Reject reader.[[closedPromise]] with e.
  //
  // The closedPromise might have been created in another compartment.
  // RejectPromise can deal with wrapped Promise objects, but all its arguments
  // must be same-compartment with cx, so we do need to wrap the Promise.
  Rooted<JSObject*> closedPromise(cx, unwrappedReader->closedPromise());
  if (!cx->compartment()->wrap(cx, &closedPromise)) {
    return false;
  }
  if (!RejectPromise(cx, closedPromise, e)) {
    return false;
  }

  if (unwrappedStream->mode() == JS::ReadableStreamMode::ExternalSource) {
    // Make sure we're in the stream's compartment.
    AutoRealm ar(cx, unwrappedStream);
    JS::ReadableStreamUnderlyingSource* source =
        unwrappedStream->controller()->externalSource();

    // Ensure that the embedding doesn't have to deal with
    // mixed-compartment arguments to the callback.
    Rooted<Value> error(cx, e);
    if (!cx->compartment()->wrap(cx, &error)) {
      return false;
    }
    source->onErrored(cx, unwrappedStream, error);
  }

  return true;
}

/**
 * Streams spec, 3.5.7.
 *      ReadableStreamFulfillReadIntoRequest( stream, chunk, done )
 * Streams spec, 3.5.8.
 *      ReadableStreamFulfillReadRequest ( stream, chunk, done )
 * These two spec functions are identical in our implementation.
 */
MOZ_MUST_USE bool js::ReadableStreamFulfillReadOrReadIntoRequest(
    JSContext* cx, Handle<ReadableStream*> unwrappedStream, Handle<Value> chunk,
    bool done) {
  cx->check(chunk);

  // Step 1: Let reader be stream.[[reader]].
  Rooted<ReadableStreamReader*> unwrappedReader(
      cx, UnwrapReaderFromStream(cx, unwrappedStream));
  if (!unwrappedReader) {
    return false;
  }

  // Step 2: Let read{Into}Request be the first element of
  //         reader.[[read{Into}Requests]].
  // Step 3: Remove read{Into}Request from reader.[[read{Into}Requests]],
  //         shifting all other elements downward (so that the second becomes
  //         the first, and so on).
  Rooted<ListObject*> unwrappedReadIntoRequests(cx,
                                                unwrappedReader->requests());
  Rooted<JSObject*> readIntoRequest(
      cx, &unwrappedReadIntoRequests->popFirstAs<JSObject>(cx));
  MOZ_ASSERT(readIntoRequest);
  if (!cx->compartment()->wrap(cx, &readIntoRequest)) {
    return false;
  }

  // Step 4: Resolve read{Into}Request.[[promise]] with
  //         ! ReadableStreamCreateReadResult(chunk, done,
  //         readIntoRequest.[[forAuthorCode]]).
  Rooted<JSObject*> iterResult(
      cx, ReadableStreamCreateReadResult(cx, chunk, done,
                                         unwrappedReader->forAuthorCode()));
  if (!iterResult) {
    return false;
  }
  Rooted<Value> val(cx, ObjectValue(*iterResult));
  return ResolvePromise(cx, readIntoRequest, val);
}

/**
 * Streams spec, 3.5.9. ReadableStreamGetNumReadIntoRequests ( stream )
 * Streams spec, 3.5.10. ReadableStreamGetNumReadRequests ( stream )
 * (Identical implementation.)
 */
uint32_t js::ReadableStreamGetNumReadRequests(ReadableStream* stream) {
  // Step 1: Return the number of elements in
  //         stream.[[reader]].[[read{Into}Requests]].
  if (!stream->hasReader()) {
    return 0;
  }

  JS::AutoSuppressGCAnalysis nogc;
  ReadableStreamReader* reader = UnwrapReaderFromStreamNoThrow(stream);

  // Reader is a dead wrapper, treat it as non-existent.
  if (!reader) {
    return 0;
  }

  return reader->requests()->length();
}

// Streams spec, 3.5.11. ReadableStreamHasBYOBReader ( stream )
//
// Not implemented.

/**
 * Streams spec 3.5.12. ReadableStreamHasDefaultReader ( stream )
 */
MOZ_MUST_USE bool js::ReadableStreamHasDefaultReader(
    JSContext* cx, Handle<ReadableStream*> unwrappedStream, bool* result) {
  // Step 1: Let reader be stream.[[reader]].
  // Step 2: If reader is undefined, return false.
  if (!unwrappedStream->hasReader()) {
    *result = false;
    return true;
  }
  Rooted<ReadableStreamReader*> unwrappedReader(
      cx, UnwrapReaderFromStream(cx, unwrappedStream));
  if (!unwrappedReader) {
    return false;
  }

  // Step 3: If ! ReadableStreamDefaultReader(reader) is false, return false.
  // Step 4: Return true.
  *result = unwrappedReader->is<ReadableStreamDefaultReader>();
  return true;
}
