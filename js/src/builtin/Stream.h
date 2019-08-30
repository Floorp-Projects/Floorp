/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef builtin_Stream_h
#define builtin_Stream_h

#include "js/Stream.h"
#include "builtin/Promise.h"
#include "vm/List.h"
#include "vm/NativeObject.h"

namespace js {

class ReadableStreamReader;
class ReadableStreamController;

class ReadableStream : public NativeObject {
 public:
  /**
   * Memory layout of Stream instances.
   *
   * See https://streams.spec.whatwg.org/#rs-internal-slots for details on
   * the stored state. [[state]] and [[disturbed]] are stored in
   * StreamSlot_State as ReadableStream::State enum values.
   *
   * Of the stored values, Reader and StoredError might be cross-compartment
   * wrappers. This can happen if the Reader was created by applying a
   * different compartment's ReadableStream.prototype.getReader method.
   *
   * A stream's associated controller is always created from under the
   * stream's constructor and thus cannot be in a different compartment.
   */
  enum Slots {
    Slot_Controller,
    Slot_Reader,
    Slot_State,
    Slot_StoredError,
    SlotCount
  };

 private:
  enum StateBits {
    Readable = 0,
    Closed = 1,
    Errored = 2,
    StateMask = 0x000000ff,
    Disturbed = 0x00000100
  };

  uint32_t stateBits() const { return getFixedSlot(Slot_State).toInt32(); }
  void initStateBits(uint32_t stateBits) {
    MOZ_ASSERT((stateBits & ~Disturbed) <= Errored);
    setFixedSlot(Slot_State, Int32Value(stateBits));
  }
  void setStateBits(uint32_t stateBits) {
#ifdef DEBUG
    bool wasDisturbed = disturbed();
    bool wasClosedOrErrored = closed() || errored();
#endif
    initStateBits(stateBits);
    MOZ_ASSERT_IF(wasDisturbed, disturbed());
    MOZ_ASSERT_IF(wasClosedOrErrored, !readable());
  }

  StateBits state() const { return StateBits(stateBits() & StateMask); }
  void setState(StateBits state) {
    MOZ_ASSERT(state <= Errored);
    uint32_t current = stateBits() & ~StateMask;
    setStateBits(current | state);
  }

 public:
  bool readable() const { return state() == Readable; }
  bool closed() const { return state() == Closed; }
  void setClosed() { setState(Closed); }
  bool errored() const { return state() == Errored; }
  void setErrored() { setState(Errored); }
  bool disturbed() const { return stateBits() & Disturbed; }
  void setDisturbed() { setStateBits(stateBits() | Disturbed); }

  bool hasController() const {
    return !getFixedSlot(Slot_Controller).isUndefined();
  }
  inline ReadableStreamController* controller() const;
  inline void setController(ReadableStreamController* controller);
  void clearController() {
    setFixedSlot(Slot_Controller, JS::UndefinedValue());
  }

  bool hasReader() const { return !getFixedSlot(Slot_Reader).isUndefined(); }
  void setReader(JSObject* reader) {
    setFixedSlot(Slot_Reader, JS::ObjectValue(*reader));
  }
  void clearReader() { setFixedSlot(Slot_Reader, JS::UndefinedValue()); }

  Value storedError() const { return getFixedSlot(Slot_StoredError); }
  void setStoredError(HandleValue value) {
    setFixedSlot(Slot_StoredError, value);
  }

  JS::ReadableStreamMode mode() const;

  bool locked() const;

  static MOZ_MUST_USE ReadableStream* create(
      JSContext* cx, void* nsISupportsObject_alreadyAddreffed = nullptr,
      HandleObject proto = nullptr);
  static ReadableStream* createExternalSourceStream(
      JSContext* cx, JS::ReadableStreamUnderlyingSource* source,
      void* nsISupportsObject_alreadyAddreffed = nullptr,
      HandleObject proto = nullptr);

  static bool constructor(JSContext* cx, unsigned argc, Value* vp);
  static const ClassSpec classSpec_;
  static const JSClass class_;
  static const ClassSpec protoClassSpec_;
  static const JSClass protoClass_;
};

/**
 * Tells whether or not read() result objects inherit from Object.prototype.
 * Generally, they should do so only if the reader was created by author code.
 * See <https://streams.spec.whatwg.org/#readable-stream-create-read-result>.
 */
enum class ForAuthorCodeBool { No, Yes };

class ReadableStreamReader : public NativeObject {
 public:
  /**
   * Memory layout of Stream Reader instances.
   *
   * See https://streams.spec.whatwg.org/#default-reader-internal-slots and
   * https://streams.spec.whatwg.org/#byob-reader-internal-slots for details.
   *
   * Note that [[readRequests]] and [[readIntoRequests]] are treated the same
   * in our implementation.
   *
   * Of the stored values, Stream and ClosedPromise might be
   * cross-compartment wrapper wrappers.
   *
   * For Stream, this can happen if the Reader was created by applying a
   * different compartment's ReadableStream.prototype.getReader method.
   *
   * For ClosedPromise, it can be caused by applying a different
   * compartment's ReadableStream*Reader.prototype.releaseLock method.
   *
   * Requests is guaranteed to be in the same compartment as the Reader, but
   * can contain wrapped request objects from other globals.
   */
  enum Slots {
    Slot_Stream,
    Slot_Requests,
    Slot_ClosedPromise,
    Slot_ForAuthorCode,
    SlotCount,
  };

  bool hasStream() const { return !getFixedSlot(Slot_Stream).isUndefined(); }
  void setStream(JSObject* stream) {
    setFixedSlot(Slot_Stream, ObjectValue(*stream));
  }
  void clearStream() { setFixedSlot(Slot_Stream, UndefinedValue()); }
  bool isClosed() { return !hasStream(); }

  /**
   * Tells whether this reader was created by author code.
   *
   * This returns Yes for readers created using `stream.getReader()`, and No
   * for readers created for the internal use of algorithms like
   * `stream.tee()` and `new Response(stream)`.
   *
   * The standard does not have this field. Instead, eight algorithms take a
   * forAuthorCode parameter, and a [[forAuthorCode]] field is part of each
   * read request. But the behavior is always equivalent to treating readers
   * created by author code as having a bit set on them. We implement it that
   * way for simplicity.
   */
  ForAuthorCodeBool forAuthorCode() const {
    return getFixedSlot(Slot_ForAuthorCode).toBoolean() ? ForAuthorCodeBool::Yes
                                                        : ForAuthorCodeBool::No;
  }
  void setForAuthorCode(ForAuthorCodeBool value) {
    setFixedSlot(Slot_ForAuthorCode,
                 BooleanValue(value == ForAuthorCodeBool::Yes));
  }

  ListObject* requests() const {
    return &getFixedSlot(Slot_Requests).toObject().as<ListObject>();
  }
  void clearRequests() { setFixedSlot(Slot_Requests, UndefinedValue()); }

  JSObject* closedPromise() const {
    return &getFixedSlot(Slot_ClosedPromise).toObject();
  }
  void setClosedPromise(JSObject* wrappedPromise) {
    setFixedSlot(Slot_ClosedPromise, ObjectValue(*wrappedPromise));
  }

  static const JSClass class_;
};

class ReadableStreamDefaultReader : public ReadableStreamReader {
 public:
  static bool constructor(JSContext* cx, unsigned argc, Value* vp);
  static const ClassSpec classSpec_;
  static const JSClass class_;
  static const ClassSpec protoClassSpec_;
  static const JSClass protoClass_;
};

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

template <>
inline bool JSObject::is<js::ReadableStreamReader>() const {
  return is<js::ReadableStreamDefaultReader>();
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

}  // namespace js

#endif /* builtin_Stream_h */
