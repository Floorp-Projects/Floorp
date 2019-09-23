/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream default controller abstract operations. */

#include "builtin/streams/WritableStreamDefaultControllerOperations.h"

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"  // JS_ReportErrorASCII

#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "vm/JSObject-inl.h"                 // js::NewObjectWithClassProto

using JS::Handle;
using JS::Value;

/*** 4.8. Writable stream default controller abstract operations ************/

/**
 * Streams spec, 4.8.3.
 *      SetUpWritableStreamDefaultControllerFromUnderlyingSink( stream,
 *          underlyingSink, highWaterMark, sizeAlgorithm )
 */
MOZ_MUST_USE bool js::SetUpWritableStreamDefaultControllerFromUnderlyingSink(
    JSContext* cx, Handle<WritableStream*> stream, Handle<Value> underlyingSink,
    double highWaterMark, Handle<Value> sizeAlgorithm) {
  // XXX jwalden flesh me out
  JS_ReportErrorASCII(cx, "epic fail");
  return false;
}
