/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* General readable stream abstract operations. */

#ifndef builtin_streams_ReadableStreamOperations_h
#define builtin_streams_ReadableStreamOperations_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/RootingAPI.h"  // JS::Handle
#include "js/Value.h"       // JS::Value

namespace js {

class ReadableStream;
class ReadableStreamDefaultController;
class TeeState;

extern MOZ_MUST_USE JSObject* ReadableStreamTee_Pull(
    JSContext* cx, JS::Handle<TeeState*> unwrappedTeeState);

extern MOZ_MUST_USE JSObject* ReadableStreamTee_Cancel(
    JSContext* cx, JS::Handle<TeeState*> unwrappedTeeState,
    JS::Handle<ReadableStreamDefaultController*> unwrappedBranch,
    JS::Handle<JS::Value> reason);

extern MOZ_MUST_USE bool ReadableStreamTee(
    JSContext* cx, JS::Handle<ReadableStream*> unwrappedStream,
    bool cloneForBranch2, JS::MutableHandle<ReadableStream*> branch1Stream,
    JS::MutableHandle<ReadableStream*> branch2Stream);

}  // namespace js

#endif  // builtin_streams_ReadableStreamOperations_h
