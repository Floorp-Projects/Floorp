/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Class WritableStream. */

#ifndef builtin_streams_WritableStream_h
#define builtin_streams_WritableStream_h

#include "mozilla/Assertions.h"      // MOZ_ASSERT
#include "mozilla/Attributes.h"      // MOZ_MUST_USE
#include "mozilla/Casting.h"         // mozilla::AssertedCast
#include "mozilla/MathAlgorithms.h"  // mozilla::IsPowerOfTwo

#include <stdint.h>  // uint32_t

#include "js/Class.h"         // JSClass, js::ClassSpec
#include "js/RootingAPI.h"    // JS::Handle
#include "js/Value.h"         // JS::{,Int32,Object,Undefined}Value
#include "vm/List.h"          // js::ListObject
#include "vm/NativeObject.h"  // js::NativeObject

struct JSContext;

namespace js {

class WritableStreamDefaultController;

class WritableStream : public NativeObject {
 public:
  /**
   * Memory layout of WritableStream instances.
   *
   * See https://streams.spec.whatwg.org/#ws-internal-slots for details on
   * the stored state.  [[state]] and [[backpressure]] are stored in Slot_State
   * as a composite WritableStream::State enum value.
   *
   * XXX jwalden needs fleshin' out
   */
  enum Slots {
    /**
     * A WritableStream's associated controller is always created from under the
     * stream's constructor and thus cannot be in a different compartment.
     */
    Slot_Controller,
    Slot_Writer,
    Slot_State,
    Slot_StoredError,

    /**
     * A ListObject consisting of the value of the [[inFlightWriteRequest]] spec
     * field (if it is not |undefined|) followed by the elements of the
     * [[queue]] List.
     *
     * If the |HaveInFlightWriteRequest| flag is set, the first element of this
     * List is the non-|undefined| value of [[inFlightWriteRequest]].  If it is
     * unset, [[inFlightWriteRequest]] has the value |undefined|.
     */
    Slot_WriteRequests,

    /**
     * A slot storing both [[closeRequest]] and [[inFlightCloseRequest]].
     *
     * If this slot has the value |undefined|, then [[inFlightCloseRequest]]
     * and [[closeRequest]] are both |undefined|.  Otherwise one field has the
     * value |undefined| and the other has the value of this slot, and the value
     * of the |HaveInFlightCloseRequest| flag indicates which field is set.
     */
    Slot_CloseRequest,

    /**
     * In the spec the [[pendingAbortRequest]] field is either |undefined| or
     * Record { [[promise]]: Object, [[reason]]: value, [[wasAlreadyErroring]]:
     * boolean }.  We represent this as follows:
     *
     *   1) If Slot_PendingAbortRequestPromise contains |undefined|, then the
     *      spec field is |undefined|;
     *   2) Otherwise Slot_PendingAbortRequestPromise contains the value of
     *      [[pendingAbortRequest]].[[promise]], Slot_PendingAbortRequestReason
     *      contains the value of [[pendingAbortRequest]].[[reason]], and the
     *      |PendingAbortRequestWasAlreadyErroring| flag stores the value of
     *      [[pendingAbortRequest]].[[wasAlreadyErroring]].
     */
    Slot_PendingAbortRequestPromise,
    Slot_PendingAbortRequestReason,

    SlotCount
  };

 private:
  enum State : uint32_t {
    Writable = 0x0000'0000,
    Closed = 0x0000'0001,
    Erroring = 0x0000'0002,
    Errored = 0x0000'0003,
    StateBits = 0x0000'0003,
    StateMask = 0x0000'00ff,

    Backpressure = 0x0000'0100,
    HaveInFlightWriteRequest = 0x0000'0200,
    HaveInFlightCloseRequest = 0x0000'0400,
    PendingAbortRequestWasAlreadyErroring = 0x0000'0800,
    FlagBits = Backpressure | HaveInFlightWriteRequest |
               HaveInFlightCloseRequest | PendingAbortRequestWasAlreadyErroring,
    FlagMask = 0x0000'ff00,

    SettableBits = uint32_t(StateBits | FlagBits)
  };

  bool stateIsInitialized() const { return getFixedSlot(Slot_State).isInt32(); }

  State state() const {
    MOZ_ASSERT(stateIsInitialized());

    uint32_t v = getFixedSlot(Slot_State).toInt32();
    MOZ_ASSERT((v & ~SettableBits) == 0);

    return static_cast<State>(v & StateMask);
  }

  State flags() const {
    MOZ_ASSERT(stateIsInitialized());

    uint32_t v = getFixedSlot(Slot_State).toInt32();
    MOZ_ASSERT((v & ~SettableBits) == 0);

    return static_cast<State>(v & FlagMask);
  }

  void initWritableState() {
    MOZ_ASSERT(!stateIsInitialized());

    setFixedSlot(Slot_State, JS::Int32Value(Writable));

    MOZ_ASSERT(writable());
    MOZ_ASSERT(!backpressure());
  }

  void setState(State newState) {
    MOZ_ASSERT(stateIsInitialized());
    MOZ_ASSERT((newState & ~StateBits) == 0);
    MOZ_ASSERT(newState <= Errored);

#ifdef DEBUG
    {
      auto current = state();
      if (current == Writable) {
        MOZ_ASSERT(newState == Closed || newState == Erroring);
      } else if (current == Erroring) {
        MOZ_ASSERT(newState == Errored);
      } else if (current == Closed || current == Errored) {
        MOZ_ASSERT_UNREACHABLE(
            "closed/errored stream shouldn't undergo state transitions");
      } else {
        MOZ_ASSERT_UNREACHABLE("smashed state bits?");
      }
    }
#endif

    uint32_t newValue = static_cast<uint32_t>(newState) |
                        (getFixedSlot(Slot_State).toInt32() & FlagMask);
    setFixedSlot(Slot_State,
                 JS::Int32Value(mozilla::AssertedCast<int32_t>(newValue)));
  }

  void setFlag(State flag, bool set) {
    MOZ_ASSERT(stateIsInitialized());
    MOZ_ASSERT(mozilla::IsPowerOfTwo(uint32_t(flag)));
    MOZ_ASSERT((flag & FlagBits) != 0);

    uint32_t v = getFixedSlot(Slot_State).toInt32();
    MOZ_ASSERT((v & ~SettableBits) == 0);

    uint32_t newValue = set ? (v | flag) : (v & ~flag);
    setFixedSlot(Slot_State,
                 JS::Int32Value(mozilla::AssertedCast<int32_t>(newValue)));
  }

  void setBackpressure(bool pressure) { setFlag(Backpressure, pressure); }

 public:
  bool writable() const { return state() == Writable; }
  bool closed() const { return state() == Closed; }
  bool erroring() const { return state() == Erroring; }
  bool errored() const { return state() == Errored; }

  bool backpressure() const { return flags() & Backpressure; }

  bool haveInFlightWriteRequest() const {
    return flags() & HaveInFlightWriteRequest;
  }
  bool haveInFlightCloseRequest() const {
    return flags() & HaveInFlightCloseRequest;
  }

  bool hasController() const {
    return !getFixedSlot(Slot_Controller).isUndefined();
  }
  inline WritableStreamDefaultController* controller() const;
  inline void setController(WritableStreamDefaultController* controller);
  void clearController() {
    setFixedSlot(Slot_Controller, JS::UndefinedValue());
  }

  bool hasWriter() const { return !getFixedSlot(Slot_Writer).isUndefined(); }
  void setWriter(JSObject* writer) {
    setFixedSlot(Slot_Writer, JS::ObjectValue(*writer));
  }
  void clearWriter() { setFixedSlot(Slot_Writer, JS::UndefinedValue()); }

  JS::Value storedError() const { return getFixedSlot(Slot_StoredError); }
  void setStoredError(JS::Handle<JS::Value> value) {
    setFixedSlot(Slot_StoredError, value);
  }

  JS::Value inFlightWriteRequest() const {
    MOZ_ASSERT(stateIsInitialized());

    // The in-flight write request is the first element of |writeRequests()| --
    // if there is a request in flight.
    if (haveInFlightWriteRequest()) {
      MOZ_ASSERT(writeRequests()->length() > 0);
      return writeRequests()->get(0);
    }

    return JS::UndefinedValue();
  }

  void clearInFlightWriteRequest(JSContext* cx);

  JS::Value closeRequest() const {
    JS::Value v = getFixedSlot(Slot_CloseRequest);
    if (v.isUndefined()) {
      // In principle |haveInFlightCloseRequest()| only distinguishes whether
      // the close-request slot is [[closeRequest]] or [[inFlightCloseRequest]].
      // In practice, for greater implementation strictness to try to head off
      // more bugs, we require that the HaveInFlightCloseRequest flag be unset
      // when [[closeRequest]] and [[inFlightCloseRequest]] are both undefined.
      MOZ_ASSERT(!haveInFlightCloseRequest());
      return JS::UndefinedValue();
    }

    if (!haveInFlightCloseRequest()) {
      return v;
    }

    return JS::UndefinedValue();
  }

  JS::Value inFlightCloseRequest() const {
    JS::Value v = getFixedSlot(Slot_CloseRequest);
    if (v.isUndefined()) {
      // In principle |haveInFlightCloseRequest()| only distinguishes whether
      // the close-request slot is [[closeRequest]] or [[inFlightCloseRequest]].
      // In practice, for greater implementation strictness to try to head off
      // more bugs, we require that the HaveInFlightCloseRequest flag be unset
      // when [[closeRequest]] and [[inFlightCloseRequest]] are both undefined.
      MOZ_ASSERT(!haveInFlightCloseRequest());
      return JS::UndefinedValue();
    }

    if (haveInFlightCloseRequest()) {
      return v;
    }

    return JS::UndefinedValue();
  }

  ListObject* writeRequests() const {
    return &getFixedSlot(Slot_WriteRequests).toObject().as<ListObject>();
  }
  void clearWriteRequests() {
    MOZ_ASSERT(stateIsInitialized());
    MOZ_ASSERT(!haveInFlightWriteRequest(),
               "must clear the in-flight request flag before clearing "
               "requests");
    setFixedSlot(Slot_WriteRequests, JS::UndefinedValue());
  }

  bool hasPendingAbortRequest() const {
    MOZ_ASSERT(stateIsInitialized());
    return !getFixedSlot(Slot_PendingAbortRequestPromise).isUndefined();
  }
  JSObject* pendingAbortRequestPromise() const {
    MOZ_ASSERT(hasPendingAbortRequest());
    return &getFixedSlot(Slot_PendingAbortRequestPromise).toObject();
  }
  JS::Value pendingAbortRequestReason() const {
    MOZ_ASSERT(hasPendingAbortRequest());
    return getFixedSlot(Slot_PendingAbortRequestReason);
  }
  bool pendingAbortRequestWasAlreadyErroring() const {
    MOZ_ASSERT(hasPendingAbortRequest());
    return flags() & PendingAbortRequestWasAlreadyErroring;
  }

  static MOZ_MUST_USE WritableStream* create(
      JSContext* cx, void* nsISupportsObject_alreadyAddreffed = nullptr,
      JS::Handle<JSObject*> proto = nullptr);

  static bool constructor(JSContext* cx, unsigned argc, JS::Value* vp);

  static const ClassSpec classSpec_;
  static const JSClass class_;
  static const ClassSpec protoClassSpec_;
  static const JSClass protoClass_;
};

}  // namespace js

#endif  // builtin_streams_WritableStream_h
