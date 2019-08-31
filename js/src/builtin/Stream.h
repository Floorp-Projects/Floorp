/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Stream_h
#define builtin_Stream_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/RootingAPI.h"  // JS::Handle
#include "js/Value.h"       // JS::Value

struct JSContext;

namespace js {

class ReadableByteStreamController;
class ReadableStreamController;
class ReadableStreamDefaultController;

extern MOZ_MUST_USE bool ReadableStreamDefaultControllerEnqueue(
    JSContext* cx,
    JS::Handle<ReadableStreamDefaultController*> unwrappedController,
    JS::Handle<JS::Value> chunk);

extern MOZ_MUST_USE bool ReadableStreamControllerError(
    JSContext* cx, JS::Handle<ReadableStreamController*> unwrappedController,
    JS::Handle<JS::Value> e);

extern MOZ_MUST_USE bool ReadableStreamDefaultControllerClose(
    JSContext* cx,
    JS::Handle<ReadableStreamDefaultController*> unwrappedController);

extern MOZ_MUST_USE double ReadableStreamControllerGetDesiredSizeUnchecked(
    ReadableStreamController* controller);

extern MOZ_MUST_USE bool ReadableByteStreamControllerClose(
    JSContext* cx,
    JS::Handle<ReadableByteStreamController*> unwrappedController);

extern MOZ_MUST_USE JSObject* ReadableStreamControllerPullSteps(
    JSContext* cx, JS::Handle<ReadableStreamController*> unwrappedController);

extern MOZ_MUST_USE bool ReadableStreamControllerCallPullIfNeeded(
    JSContext* cx, JS::Handle<ReadableStreamController*> unwrappedController);

extern void ReadableStreamControllerClearAlgorithms(
    JS::Handle<ReadableStreamController*> controller);

}  // namespace js

#endif /* builtin_Stream_h */
