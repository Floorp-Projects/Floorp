/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* The interface between readable streams and controllers. */

#ifndef builtin_streams_ReadableStreamInternals_h
#define builtin_streams_ReadableStreamInternals_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "builtin/streams/ReadableStreamReader.h"  // js::ForAuthorCodeBool
#include "js/RootingAPI.h"                         // JS::Handle
#include "js/Value.h"                              // JS::Value

struct JSContext;

namespace js {

class ReadableStream;

extern MOZ_MUST_USE JSObject* ReadableStreamAddReadOrReadIntoRequest(
    JSContext* cx, JS::Handle<ReadableStream*> unwrappedStream);

extern MOZ_MUST_USE JSObject* ReadableStreamCancel(
    JSContext* cx, JS::Handle<ReadableStream*> unwrappedStream,
    JS::Handle<JS::Value> reason);

extern MOZ_MUST_USE bool ReadableStreamCloseInternal(
    JSContext* cx, JS::Handle<ReadableStream*> unwrappedStream);

extern MOZ_MUST_USE JSObject* ReadableStreamCreateReadResult(
    JSContext* cx, JS::Handle<JS::Value> value, bool done,
    ForAuthorCodeBool forAuthorCode);

extern MOZ_MUST_USE bool ReadableStreamErrorInternal(
    JSContext* cx, JS::Handle<ReadableStream*> unwrappedStream,
    JS::Handle<JS::Value> e);

extern MOZ_MUST_USE bool ReadableStreamFulfillReadOrReadIntoRequest(
    JSContext* cx, JS::Handle<ReadableStream*> unwrappedStream,
    JS::Handle<JS::Value> chunk, bool done);

extern uint32_t ReadableStreamGetNumReadRequests(ReadableStream* stream);

extern MOZ_MUST_USE bool ReadableStreamHasDefaultReader(
    JSContext* cx, JS::Handle<ReadableStream*> unwrappedStream, bool* result);

}  // namespace js

#endif  // builtin_streams_ReadableStreamInternals_h
