/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStreamDefaultWriter. */

#ifndef builtin_streams_WritableStreamDefaultWriter_inl_h
#define builtin_streams_WritableStreamDefaultWriter_inl_h

#include "builtin/streams/WritableStreamDefaultWriter.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE

#include "builtin/Promise.h"                 // js::PromiseObject
#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "js/RootingAPI.h"                   // JS::Handle
#include "js/Value.h"                        // JS::ObjectValue
#include "vm/NativeObject.h"                 // js::NativeObject

#include "vm/Compartment-inl.h"  // js::UnwrapInternalSlot

struct JSContext;

namespace js {

/**
 * Returns the stream associated with the given reader.
 */
inline MOZ_MUST_USE WritableStream* UnwrapStreamFromWriter(
    JSContext* cx, JS::Handle<WritableStreamDefaultWriter*> unwrappedWriter) {
  MOZ_ASSERT(unwrappedWriter->hasStream());
  return UnwrapInternalSlot<WritableStream>(
      cx, unwrappedWriter, WritableStreamDefaultWriter::Slot_Stream);
}

}  // namespace js

inline js::PromiseObject* js::WritableStreamDefaultWriter::closedPromise()
    const {
  return &getFixedSlot(Slot_ClosedPromise).toObject().as<PromiseObject>();
}

inline void js::WritableStreamDefaultWriter::setClosedPromise(
    PromiseObject* promise) {
  setFixedSlot(Slot_ClosedPromise, JS::ObjectValue(*promise));
}

#endif  // builtin_streams_WritableStreamDefaultWriter_inl_h
