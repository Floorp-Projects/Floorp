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

#include "builtin/streams/ReadableStreamReader.h"  // js::ReadableStreamDefaultReader
#include "builtin/streams/WritableStreamDefaultWriter.h"  // js::WritableStreamDefaultWriter
#include "js/Class.h"                                     // JSClass
#include "js/RootingAPI.h"                                // JS::Handle
#include "js/Value.h"          // JS::Int32Value, JS::ObjectValue
#include "vm/NativeObject.h"   // js::NativeObject
#include "vm/PromiseObject.h"  // js::PromiseObject

class JS_PUBLIC_API JSObject;

namespace js {

class ReadableStream;
class WritableStream;

/**
 * PipeToState objects implement the local variables in Streams spec 3.4.11
 * ReadableStreamPipeTo across all sub-operations that occur in that algorithm.
 */
class PipeToState : public NativeObject {
 public:
  /**
   * Memory layout for PipeToState instances.
   */
  enum Slots {
    /** Integer bit field of various flags. */
    Slot_Flags = 0,

    /**
     * The promise resolved or rejected when the overall pipe-to operation
     * completes.
     *
     * This promise is created directly under |ReadableStreamPipeTo|, at the
     * same time the corresponding |PipeToState| is created, so it is always
     * same-compartment with this and is guaranteed to hold a |PromiseObject*|
     * if initialization succeeded.
     */
    Slot_Promise,

    /**
     * A |ReadableStreamDefaultReader| used to read from the readable stream
     * being piped from.
     *
     * This reader is created at the same time as its |PipeToState|, so this
     * reader is same-compartment with this and is guaranteed to be a
     * |ReadableStreamDefaultReader*| if initialization succeeds.
     */
    Slot_Reader,

    /**
     * A |WritableStreamDefaultWriter| used to write to the writable stream
     * being piped to.
     *
     * This writer is created at the same time as its |PipeToState|, so this
     * writer is same-compartment with this and is guaranteed to be a
     * |WritableStreamDefaultWriter*| if initialization succeeds.
     */
    Slot_Writer,

    /**
     * The |PromiseObject*| of the last write performed to the destinationg
     * |WritableStream| using the writer in |Slot_Writer|.  If no writes have
     * yet been performed, this slot contains |undefined|.
     *
     * This promise is created inside a handler function in the same compartment
     * and realm as this |PipeToState|, so it is always a |PromiseObject*| and
     * never a wrapper around one.
     */
    Slot_LastWriteRequest,

    SlotCount,
  };

 private:
  enum Flags : uint32_t {
    Flag_ShuttingDown = 0b0001,

    Flag_PreventClose = 0b0010,
    Flag_PreventAbort = 0b0100,
    Flag_PreventCancel = 0b1000,

    Flag_ReadPending = 0b1'0000,
  };

  uint32_t flags() const { return getFixedSlot(Slot_Flags).toInt32(); }
  void setFlags(uint32_t flags) {
    setFixedSlot(Slot_Flags, JS::Int32Value(flags));
  }

 public:
  static const JSClass class_;

  PromiseObject* promise() const {
    return &getFixedSlot(Slot_Promise).toObject().as<PromiseObject>();
  }

  ReadableStreamDefaultReader* reader() const {
    return &getFixedSlot(Slot_Reader)
                .toObject()
                .as<ReadableStreamDefaultReader>();
  }

  WritableStreamDefaultWriter* writer() const {
    return &getFixedSlot(Slot_Writer)
                .toObject()
                .as<WritableStreamDefaultWriter>();
  }

  PromiseObject* lastWriteRequest() const {
    const auto& slot = getFixedSlot(Slot_LastWriteRequest);
    if (slot.isUndefined()) {
      return nullptr;
    }

    return &slot.toObject().as<PromiseObject>();
  }

  void updateLastWriteRequest(PromiseObject* writeRequest) {
    MOZ_ASSERT(writeRequest != nullptr);
    setFixedSlot(Slot_LastWriteRequest, JS::ObjectValue(*writeRequest));
  }

  bool shuttingDown() const { return flags() & Flag_ShuttingDown; }
  void setShuttingDown() {
    MOZ_ASSERT(!shuttingDown());
    setFlags(flags() | Flag_ShuttingDown);
  }

  bool preventClose() const { return flags() & Flag_PreventClose; }
  bool preventAbort() const { return flags() & Flag_PreventAbort; }
  bool preventCancel() const { return flags() & Flag_PreventCancel; }

  bool isReadPending() const { return flags() & Flag_ReadPending; }
  void setReadPending() {
    MOZ_ASSERT(!isReadPending());
    setFlags(flags() | Flag_ReadPending);
  }
  void clearReadPending() {
    MOZ_ASSERT(isReadPending());
    setFlags(flags() & ~Flag_ReadPending);
  }

  void initFlags(bool preventClose, bool preventAbort, bool preventCancel) {
    MOZ_ASSERT(getFixedSlot(Slot_Flags).isUndefined());

    uint32_t flagBits = (preventClose ? Flag_PreventClose : 0) |
                        (preventAbort ? Flag_PreventAbort : 0) |
                        (preventCancel ? Flag_PreventCancel : 0);
    setFlags(flagBits);
  }

  static PipeToState* create(JSContext* cx, JS::Handle<PromiseObject*> promise,
                             JS::Handle<ReadableStream*> unwrappedSource,
                             JS::Handle<WritableStream*> unwrappedDest,
                             bool preventClose, bool preventAbort,
                             bool preventCancel, JS::Handle<JSObject*> signal);
};

}  // namespace js

#endif  // builtin_streams_PipeToState_h
