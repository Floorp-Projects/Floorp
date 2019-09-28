/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* WritableStream controller classes and functions. */

#ifndef builtin_streams_WritableStreamDefaultController_h
#define builtin_streams_WritableStreamDefaultController_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT

#include <stdint.h>  // uint32_t

#include "builtin/streams/StreamController.h"  // js::StreamController
#include "builtin/streams/WritableStream.h"    // js::WritableStream
#include "js/Class.h"                          // JSClass, js::ClassSpec
#include "js/RootingAPI.h"                     // JS::Handle
#include "js/Stream.h"  // JS::WritableStreamUnderlyingSink
#include "js/Value.h"  // JS::Value, JS::{Number,Object,Private,Undefined}Value, JS::UndefinedHandleValue
#include "vm/NativeObject.h"  // js::NativeObject

namespace js {

class WritableStreamDefaultController : public StreamController {
 public:
  /**
   * Memory layout for WritableStream default controllers, starting after the
   * slots reserved for queue container usage.  (Note that this is the only
   * writable stream controller class in the spec: ReadableByteStreamController
   * exists, but WritableByteStreamController does not.)
   */
  enum Slots {
    Slot_Stream = StreamController::SlotCount,
    Slot_UnderlyingSink,

    /** Number stored as DoubleValue. */
    Slot_StrategyHWM,

    /**
     * Either undefined if each chunk has size 1, or a callable object to be
     * invoked on each chunk to determine its size.  See
     * MakeSizeAlgorithmFromSizeFunction.
     */
    Slot_StrategySize,

    /** Bit field stored as Int32Value. */
    Slot_Flags,

    SlotCount
  };

  enum ControllerFlags {
    Flag_Started = 0b0001,
    Flag_ExternalSink = 0b0010,
  };

  WritableStream* stream() const {
    return &getFixedSlot(Slot_Stream).toObject().as<WritableStream>();
  }
  void setStream(WritableStream* stream) {
    setFixedSlot(Slot_Stream, JS::ObjectValue(*stream));
  }

  JS::Value underlyingSink() const { return getFixedSlot(Slot_UnderlyingSink); }
  void setUnderlyingSink(const JS::Value& underlyingSink) {
    setFixedSlot(Slot_UnderlyingSink, underlyingSink);
  }

  JS::WritableStreamUnderlyingSink* externalSink() const {
    static_assert(alignof(JS::WritableStreamUnderlyingSink) >= 2,
                  "external underling sinks are stored as PrivateValues, so "
                  "they must have even addresses");
    MOZ_ASSERT(hasExternalSink());
    return static_cast<JS::WritableStreamUnderlyingSink*>(
        underlyingSink().toPrivate());
  }
  void setExternalSink(JS::WritableStreamUnderlyingSink* underlyingSink) {
    setUnderlyingSink(JS::PrivateValue(underlyingSink));
    addFlags(Flag_ExternalSink);
  }
  static void clearUnderlyingSink(
      JS::Handle<WritableStreamDefaultController*> controller,
      bool finalizeSink = true) {
    if (controller->hasExternalSink()) {
      if (finalizeSink) {
        controller->externalSink()->finalize();
      }
      controller->setFlags(controller->flags() & ~Flag_ExternalSink);
    }
    controller->setUnderlyingSink(JS::UndefinedHandleValue);
  }

  void setWriteMethod(const JS::Value& writeMethod) {}

  void setCloseMethod(const JS::Value& closeMethod) {}

  void setAbortMethod(const JS::Value& abortMethod) {}

  double strategyHWM() const {
    return getFixedSlot(Slot_StrategyHWM).toDouble();
  }
  void setStrategyHWM(double highWaterMark) {
    setFixedSlot(Slot_StrategyHWM, DoubleValue(highWaterMark));
  }

  void setStrategySize(const JS::Value& size) {}

  uint32_t flags() const { return getFixedSlot(Slot_Flags).toInt32(); }
  void setFlags(uint32_t flags) { setFixedSlot(Slot_Flags, Int32Value(flags)); }
  void addFlags(uint32_t flags) { setFlags(this->flags() | flags); }
  void removeFlags(uint32_t flags) { setFlags(this->flags() & ~flags); }

  bool started() const { return flags() & Flag_Started; }
  void setStarted() { addFlags(Flag_Started); }

  bool hasExternalSink() const { return flags() & Flag_ExternalSink; }

  static bool constructor(JSContext* cx, unsigned argc, JS::Value* vp);
  static const ClassSpec classSpec_;
  static const JSClass class_;
  static const ClassSpec protoClassSpec_;
  static const JSClass protoClass_;
};

inline WritableStreamDefaultController* WritableStream::controller() const {
  return &getFixedSlot(Slot_Controller)
              .toObject()
              .as<WritableStreamDefaultController>();
}

inline void WritableStream::setController(
    WritableStreamDefaultController* controller) {
  setFixedSlot(Slot_Controller, JS::ObjectValue(*controller));
}

}  // namespace js

#endif  // builtin_streams_WritableStreamDefaultController_h
