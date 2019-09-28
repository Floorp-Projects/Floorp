/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStream. */

#ifndef builtin_streams_WritableStream_inl_h
#define builtin_streams_WritableStream_inl_h

#include "builtin/streams/WritableStream.h"

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::WritableStreamDefaultWriter
#include "js/Value.h"                                     // JS::{,Object}Value

inline js::WritableStreamDefaultWriter* js::WritableStream::writer() const {
  MOZ_ASSERT(hasWriter());
  return &getFixedSlot(Slot_Writer)
              .toObject()
              .as<WritableStreamDefaultWriter>();
}

inline void js::WritableStream::setWriter(WritableStreamDefaultWriter* writer) {
  setFixedSlot(Slot_Writer, JS::ObjectValue(*writer));
}

#endif  // builtin_streams_WritableStream_inl_h
