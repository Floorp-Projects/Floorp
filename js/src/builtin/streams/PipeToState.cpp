/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* ReadableStream.prototype.pipeTo state. */

#include "builtin/streams/PipeToState.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "jsapi.h"        // JS_ReportErrorNumberASCII
#include "jsfriendapi.h"  // js::GetErrorMessage, JSMSG_*

#include "builtin/Promise.h"  // js::RejectPromiseWithPendingError
#include "builtin/streams/ReadableStream.h"  // js::ReadableStream
#include "builtin/streams/ReadableStreamReader.h"  // js::CreateReadableStreamDefaultReader, js::ForAuthorCodeBool
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::CreateWritableStreamDefaultWriter
#include "js/Class.h"          // JSClass, JSCLASS_HAS_RESERVED_SLOTS
#include "js/RootingAPI.h"     // JS::Handle, JS::Rooted
#include "vm/PromiseObject.h"  // js::PromiseObject

#include "vm/JSContext-inl.h"  // JSContext::check
#include "vm/JSObject-inl.h"   // js::NewBuiltinClassInstance

using JS::Handle;
using JS::Int32Value;
using JS::ObjectValue;
using JS::Rooted;

using js::PipeToState;

/**
 * Stream spec, 3.4.11. ReadableStreamPipeTo ( source, dest,
 *                                             preventClose, preventAbort,
 *                                             preventCancel, signal )
 * Steps 4-11, 13-14.
 */
/* static */ PipeToState* PipeToState::create(
    JSContext* cx, Handle<PromiseObject*> promise,
    Handle<ReadableStream*> unwrappedSource,
    Handle<WritableStream*> unwrappedDest, bool preventClose, bool preventAbort,
    bool preventCancel, Handle<JSObject*> signal) {
  cx->check(promise);

  // Step 4. Assert: signal is undefined or signal is an instance of the
  //         AbortSignal interface.
#ifdef DEBUG
  if (signal) {
    // XXX jwalden need to add JSAPI hooks to recognize AbortSignal instances
  }
#endif

  // Step 5: Assert: ! IsReadableStreamLocked(source) is false.
  MOZ_ASSERT(!unwrappedSource->locked());

  // Step 6: Assert: ! IsWritableStreamLocked(dest) is false.
  MOZ_ASSERT(!unwrappedDest->isLocked());

  Rooted<PipeToState*> state(cx, NewBuiltinClassInstance<PipeToState>(cx));
  if (!state) {
    return nullptr;
  }

  MOZ_ASSERT(state->getFixedSlot(Slot_Promise).isUndefined());
  state->initFixedSlot(Slot_Promise, ObjectValue(*promise));

  // Step 7: If ! IsReadableByteStreamController(
  //                  source.[[readableStreamController]]) is true, let reader
  //         be either ! AcquireReadableStreamBYOBReader(source) or
  //         ! AcquireReadableStreamDefaultReader(source), at the user agent’s
  //         discretion.
  // Step 8: Otherwise, let reader be
  //         ! AcquireReadableStreamDefaultReader(source).
  // We don't implement byte streams, so we always acquire a default reader.
  {
    ReadableStreamDefaultReader* reader = CreateReadableStreamDefaultReader(
        cx, unwrappedSource, ForAuthorCodeBool::No);
    if (!reader) {
      return nullptr;
    }

    MOZ_ASSERT(state->getFixedSlot(Slot_Reader).isUndefined());
    state->initFixedSlot(Slot_Reader, ObjectValue(*reader));
  }

  // Step 9: Let writer be ! AcquireWritableStreamDefaultWriter(dest).
  {
    WritableStreamDefaultWriter* writer =
        CreateWritableStreamDefaultWriter(cx, unwrappedDest);
    if (!writer) {
      return nullptr;
    }

    MOZ_ASSERT(state->getFixedSlot(Slot_Writer).isUndefined());
    state->initFixedSlot(Slot_Writer, ObjectValue(*writer));
  }

  // Step 10: Set source.[[disturbed]] to true.
  unwrappedSource->setDisturbed();

  state->initFlags(preventClose, preventAbort, preventCancel);
  MOZ_ASSERT(state->preventClose() == preventClose);
  MOZ_ASSERT(state->preventAbort() == preventAbort);
  MOZ_ASSERT(state->preventCancel() == preventCancel);

  // Step 11: Let shuttingDown be false.
  MOZ_ASSERT(!state->shuttingDown(), "should be set to false by initFlags");

  // Step 12 ("Let promise be a new promise.") was performed by the caller and
  // |promise| was its result.

  // Step 13: If signal is not undefined,
  // XXX jwalden need JSAPI to add an algorithm/steps to an AbortSignal

  // Step 14: In parallel, using reader and writer, read all chunks from source
  //          and write them to dest.
  JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                            JSMSG_READABLESTREAM_METHOD_NOT_IMPLEMENTED,
                            "pipeTo");
  return nullptr;
}

const JSClass PipeToState::class_ = {"PipeToState",
                                     JSCLASS_HAS_RESERVED_SLOTS(SlotCount)};
