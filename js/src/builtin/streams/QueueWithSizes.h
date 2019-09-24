/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Queue-with-sizes operations. */

#ifndef builtin_streams_QueueWithSizes_h
#define builtin_streams_QueueWithSizes_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/RootingAPI.h"  // JS::{,Mutable}Handle
#include "js/Value.h"       // JS::Value

struct JSContext;

namespace js {

class StreamController;

/**
 * Streams spec, 6.2.1. DequeueValue ( container ) nothrow
 */
extern MOZ_MUST_USE bool DequeueValue(
    JSContext* cx, JS::Handle<StreamController*> unwrappedContainer,
    JS::MutableHandle<JS::Value> chunk);
/**
 * Streams spec, 6.2.2. EnqueueValueWithSize ( container, value, size ) throws
 */
extern MOZ_MUST_USE bool EnqueueValueWithSize(
    JSContext* cx, JS::Handle<StreamController*> unwrappedContainer,
    JS::Handle<JS::Value> value, JS::Handle<JS::Value> sizeVal);

/**
 * Streams spec, 6.2.4. ResetQueue ( container ) nothrow
 */
extern MOZ_MUST_USE bool ResetQueue(
    JSContext* cx, JS::Handle<StreamController*> unwrappedContainer);

}  // namespace js

#endif  // builtin_streams_QueueWithSizes_h
