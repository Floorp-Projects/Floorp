/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream writer abstract operations. */

#ifndef builtin_streams_WritableStreamWriterOperations_h
#define builtin_streams_WritableStreamWriterOperations_h

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "js/RootingAPI.h"  // JS::{,Mutable}Handle
#include "js/Value.h"       // JS::Value

class JSObject;

namespace js {

class WritableStreamDefaultWriter;

extern JSObject* WritableStreamDefaultWriterClose(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter);

extern JS::Value WritableStreamDefaultWriterGetDesiredSize(
    const WritableStreamDefaultWriter* unwrappedWriter);

}  // namespace js

#endif  // builtin_streams_WritableStreamWriterOperations_h
