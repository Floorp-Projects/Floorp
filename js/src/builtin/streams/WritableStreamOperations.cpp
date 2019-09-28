/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream abstract operations. */

#include "builtin/streams/WritableStreamOperations.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"  // JS_SetPrivate

#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "vm/JSObject-inl.h"                 // js::NewObjectWithClassProto
#include "vm/List-inl.h"                     // js::StoreNewListInFixedSlot

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
