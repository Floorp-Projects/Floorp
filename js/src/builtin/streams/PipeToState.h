/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* ReadableStream.prototype.pipeTo state. */

#ifndef builtin_streams_PipeToState_h
#define builtin_streams_PipeToState_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include <stdint.h>  // uint32_t

#include "js/Class.h"         // JSClass
#include "js/Value.h"         // JS::Int32Value
#include "vm/NativeObject.h"  // js::NativeObject

namespace js {

/**
 * PipeToState objects implement the local variables in Streams spec 3.4.11
 * ReadableStreamPipeTo across all sub-operations that occur in that algorithm.
 */
class PipeToState : public NativeObject {
 public:
  /**
   * Memory layout for PipeToState instances.
   */
  enum Slots { Slot_Flags, SlotCount };

 private:
  enum Flags {
    Flag_ShuttingDown = 1 << 0,
  };

  uint32_t flags() const { return getFixedSlot(Slot_Flags).toInt32(); }
  void setFlags(uint32_t flags) {
    setFixedSlot(Slot_Flags, JS::Int32Value(flags));
  }

 public:
  static const JSClass class_;

  bool shuttingDown() const { return flags() & Flag_ShuttingDown; }
  void setShuttingDown() {
    MOZ_ASSERT(!shuttingDown());
    setFlags(flags() | Flag_ShuttingDown);
  }

  static PipeToState* create(JSContext* cx);
};

}  // namespace js

#endif  // builtin_streams_PipeToState_h
