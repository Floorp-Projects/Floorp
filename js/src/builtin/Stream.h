/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Stream_h
#define builtin_Stream_h

#include "js/Stream.h"
#include "builtin/Promise.h"
#include "builtin/streams/ReadableStream.h"
#include "vm/List.h"
#include "vm/NativeObject.h"

namespace js {

/**
 * Common base class of both readable and writable stream controllers.
 */
class StreamController : public NativeObject {
 public:
  /**
   * Memory layout for stream controllers.
   *
   * Both ReadableStreamDefaultController and ReadableByteStreamController
   * are queue containers and must have these slots at identical offsets.
   *
   * The queue is guaranteed to be in the same compartment as the container,
   * but might contain wrappers for objects from other compartments.
   */
  enum Slots { Slot_Queue, Slot_TotalSize, SlotCount };

  ListObject* queue() const {
    return &getFixedSlot(Slot_Queue).toObject().as<ListObject>();
  }
  double queueTotalSize() const {
    return getFixedSlot(Slot_TotalSize).toNumber();
  }
  void setQueueTotalSize(double size) {
    setFixedSlot(Slot_TotalSize, NumberValue(size));
  }
};

class ReadableStreamController : public StreamController {
 public:
  /**
   * Memory layout for ReadableStream controllers, starting after the slots
   * reserved for queue container usage.
   *
   * Storage of the internal slots listed in the standard is fairly
   * straightforward except for [[pullAlgorithm]] and [[cancelAlgorithm]].
   * These algorithms are not stored as JSFunction objects. Rather, there are
   * three cases:
   *
   * -   Streams created with `new ReadableStream`: The methods are stored
   *     in Slot_PullMethod and Slot_CancelMethod. The underlying source
   *     object (`this` for these methods) is in Slot_UnderlyingSource.
   *
   * -   External source streams. Slot_UnderlyingSource is a PrivateValue
   *     pointing to the JS::ReadableStreamUnderlyingSource object. The
   *     algorithms are implemented using the .pull() and .cancel() methods
   *     of that object. Slot_Pull/CancelMethod are undefined.
   *
   * -   Tee streams. Slot_UnderlyingSource is a TeeState object. The
   *     pull/cancel algorithms are implemented as separate functions in
   *     Stream.cpp. Slot_Pull/CancelMethod are undefined.
   *
   * UnderlyingSource, PullMethod, and CancelMethod can be wrappers to objects
   * in other compartments.
   *
   * StrategyHWM and Flags are both primitive (numeric) values.
   */
  enum Slots {
    Slot_Stream = StreamController::SlotCount,
    Slot_UnderlyingSource,
    Slot_PullMethod,
    Slot_CancelMethod,
    Slot_StrategyHWM,
    Slot_Flags,
    SlotCount
  };

  enum ControllerFlags {
    Flag_Started = 1 << 0,
    Flag_Pulling = 1 << 1,
    Flag_PullAgain = 1 << 2,
    Flag_CloseRequested = 1 << 3,
    Flag_TeeBranch1 = 1 << 4,
    Flag_TeeBranch2 = 1 << 5,
    Flag_ExternalSource = 1 << 6,
    Flag_SourceLocked = 1 << 7,
  };

  ReadableStream* stream() const {
    return &getFixedSlot(Slot_Stream).toObject().as<ReadableStream>();
  }
  void setStream(ReadableStream* stream) {
    setFixedSlot(Slot_Stream, ObjectValue(*stream));
  }
  Value underlyingSource() const { return getFixedSlot(Slot_UnderlyingSource); }
  void setUnderlyingSource(const Value& underlyingSource) {
    setFixedSlot(Slot_UnderlyingSource, underlyingSource);
  }
  Value pullMethod() const { return getFixedSlot(Slot_PullMethod); }
  void setPullMethod(const Value& pullMethod) {
    setFixedSlot(Slot_PullMethod, pullMethod);
  }
  Value cancelMethod() const { return getFixedSlot(Slot_CancelMethod); }
  void setCancelMethod(const Value& cancelMethod) {
    setFixedSlot(Slot_CancelMethod, cancelMethod);
  }
  JS::ReadableStreamUnderlyingSource* externalSource() const {
    static_assert(alignof(JS::ReadableStreamUnderlyingSource) >= 2,
                  "External underling sources are stored as PrivateValues, "
                  "so they must have even addresses");
    MOZ_ASSERT(hasExternalSource());
    return static_cast<JS::ReadableStreamUnderlyingSource*>(
        underlyingSource().toPrivate());
  }
  void setExternalSource(JS::ReadableStreamUnderlyingSource* underlyingSource) {
    setUnderlyingSource(JS::PrivateValue(underlyingSource));
    addFlags(Flag_ExternalSource);
  }
  static void clearUnderlyingSource(
      JS::Handle<ReadableStreamController*> controller,
      bool finalizeSource = true) {
    if (controller->hasExternalSource()) {
      if (finalizeSource) {
        controller->externalSource()->finalize();
      }
      controller->setFlags(controller->flags() & ~Flag_ExternalSource);
    }
    controller->setUnderlyingSource(JS::UndefinedHandleValue);
  }
  double strategyHWM() const {
    return getFixedSlot(Slot_StrategyHWM).toNumber();
  }
  void setStrategyHWM(double highWaterMark) {
    setFixedSlot(Slot_StrategyHWM, NumberValue(highWaterMark));
  }
  uint32_t flags() const { return getFixedSlot(Slot_Flags).toInt32(); }
  void setFlags(uint32_t flags) { setFixedSlot(Slot_Flags, Int32Value(flags)); }
  void addFlags(uint32_t flags) { setFlags(this->flags() | flags); }
  void removeFlags(uint32_t flags) { setFlags(this->flags() & ~flags); }
  bool started() const { return flags() & Flag_Started; }
  void setStarted() { addFlags(Flag_Started); }
  bool pulling() const { return flags() & Flag_Pulling; }
  void setPulling() { addFlags(Flag_Pulling); }
  void clearPullFlags() { removeFlags(Flag_Pulling | Flag_PullAgain); }
  bool pullAgain() const { return flags() & Flag_PullAgain; }
  void setPullAgain() { addFlags(Flag_PullAgain); }
  bool closeRequested() const { return flags() & Flag_CloseRequested; }
  void setCloseRequested() { addFlags(Flag_CloseRequested); }
  bool isTeeBranch1() const { return flags() & Flag_TeeBranch1; }
  void setTeeBranch1() {
    MOZ_ASSERT(!isTeeBranch2());
    addFlags(Flag_TeeBranch1);
  }
  bool isTeeBranch2() const { return flags() & Flag_TeeBranch2; }
  void setTeeBranch2() {
    MOZ_ASSERT(!isTeeBranch1());
    addFlags(Flag_TeeBranch2);
  }
  bool hasExternalSource() const { return flags() & Flag_ExternalSource; }
  bool sourceLocked() const { return flags() & Flag_SourceLocked; }
  void setSourceLocked() { addFlags(Flag_SourceLocked); }
  void clearSourceLocked() { removeFlags(Flag_SourceLocked); }
};

class ReadableStreamDefaultController : public ReadableStreamController {
 private:
  /**
   * Memory layout for ReadableStreamDefaultControllers, starting after the
   * slots shared among all types of controllers.
   *
   * StrategySize is treated as an opaque value when stored. The only use site
   * ensures that it's wrapped into the current cx compartment.
   */
  enum Slots {
    Slot_StrategySize = ReadableStreamController::SlotCount,
    SlotCount
  };

 public:
  Value strategySize() const { return getFixedSlot(Slot_StrategySize); }
  void setStrategySize(const Value& size) {
    setFixedSlot(Slot_StrategySize, size);
  }

  static bool constructor(JSContext* cx, unsigned argc, Value* vp);
  static const ClassSpec classSpec_;
  static const JSClass class_;
  static const ClassSpec protoClassSpec_;
  static const JSClass protoClass_;
};

class ReadableByteStreamController : public ReadableStreamController {
 public:
  /**
   * Memory layout for ReadableByteStreamControllers, starting after the
   * slots shared among all types of controllers.
   *
   * PendingPullIntos is guaranteed to be in the same compartment as the
   * controller, but might contain wrappers for objects from other
   * compartments.
   *
   * AutoAllocateSize is a primitive (numeric) value.
   */
  enum Slots {
    Slot_BYOBRequest = ReadableStreamController::SlotCount,
    Slot_PendingPullIntos,
    Slot_AutoAllocateSize,
    SlotCount
  };

  Value byobRequest() const { return getFixedSlot(Slot_BYOBRequest); }
  void clearBYOBRequest() {
    setFixedSlot(Slot_BYOBRequest, JS::UndefinedValue());
  }
  ListObject* pendingPullIntos() const {
    return &getFixedSlot(Slot_PendingPullIntos).toObject().as<ListObject>();
  }
  Value autoAllocateChunkSize() const {
    return getFixedSlot(Slot_AutoAllocateSize);
  }
  void setAutoAllocateChunkSize(const Value& size) {
    setFixedSlot(Slot_AutoAllocateSize, size);
  }

  static bool constructor(JSContext* cx, unsigned argc, Value* vp);
  static const ClassSpec classSpec_;
  static const JSClass class_;
  static const ClassSpec protoClassSpec_;
  static const JSClass protoClass_;
};

}  // namespace js

template <>
inline bool JSObject::is<js::StreamController>() const {
  return is<js::ReadableStreamDefaultController>() ||
         is<js::ReadableByteStreamController>();
}

template <>
inline bool JSObject::is<js::ReadableStreamController>() const {
  return is<js::ReadableStreamDefaultController>() ||
         is<js::ReadableByteStreamController>();
}

namespace js {

inline ReadableStreamController* ReadableStream::controller() const {
  return &getFixedSlot(Slot_Controller)
              .toObject()
              .as<ReadableStreamController>();
}

inline void ReadableStream::setController(
    ReadableStreamController* controller) {
  setFixedSlot(Slot_Controller, JS::ObjectValue(*controller));
}

extern MOZ_MUST_USE bool ReadableStreamDefaultControllerEnqueue(
    JSContext* cx,
    JS::Handle<ReadableStreamDefaultController*> unwrappedController,
    JS::Handle<JS::Value> chunk);

extern MOZ_MUST_USE bool ReadableStreamControllerError(
    JSContext* cx, JS::Handle<ReadableStreamController*> unwrappedController,
    JS::Handle<JS::Value> e);

extern MOZ_MUST_USE bool ReadableStreamDefaultControllerClose(
    JSContext* cx,
    JS::Handle<ReadableStreamDefaultController*> unwrappedController);

extern MOZ_MUST_USE double ReadableStreamControllerGetDesiredSizeUnchecked(
    ReadableStreamController* controller);

extern MOZ_MUST_USE bool CheckReadableStreamControllerCanCloseOrEnqueue(
    JSContext* cx, JS::Handle<ReadableStreamController*> unwrappedController,
    const char* action);

extern MOZ_MUST_USE bool ReadableByteStreamControllerClose(
    JSContext* cx,
    JS::Handle<ReadableByteStreamController*> unwrappedController);

extern MOZ_MUST_USE JSObject* ReadableStreamControllerPullSteps(
    JSContext* cx, JS::Handle<ReadableStreamController*> unwrappedController);

extern MOZ_MUST_USE JSObject* ReadableStreamControllerCancelSteps(
    JSContext* cx, JS::Handle<ReadableStreamController*> unwrappedController,
    JS::Handle<JS::Value> reason);

}  // namespace js

#endif /* builtin_Stream_h */
