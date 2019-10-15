/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStreamDefaultWriter. */

#ifndef builtin_streams_WritableStreamDefaultWriter_inl_h
#define builtin_streams_WritableStreamDefaultWriter_inl_h

#include "builtin/streams/WritableStreamDefaultWriter.h"

#include "builtin/streams/WritableStream.h"  // js::WritableStream
#include "js/Value.h"                        // JS::{,Object}Value
#include "vm/JSObject.h"                     // JSObject
#include "vm/NativeObject.h"                 // js::NativeObject

inline js::WritableStream* js::WritableStreamDefaultWriter::stream() const {
  MOZ_ASSERT(hasStream());
  return &getFixedSlot(Slot_Stream).toObject().as<WritableStream>();
}

inline void js::WritableStreamDefaultWriter::setStream(WritableStream* stream) {
  setFixedSlot(Slot_Stream, JS::ObjectValue(*stream));
}

#endif  // builtin_streams_WritableStreamDefaultWriter_inl_h
