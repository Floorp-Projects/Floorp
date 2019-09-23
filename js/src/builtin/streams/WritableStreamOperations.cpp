/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Writable stream abstract operations. */

#include "builtin/streams/WritableStreamOperations.h"

#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "jsapi.h"  // JS_SetPrivate

#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "vm/JSObject-inl.h"                 // js::NewObjectWithClassProto

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

  // XXX jwalden need to keep fleshin' out here
  return stream;
}
