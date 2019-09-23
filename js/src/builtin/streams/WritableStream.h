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
#include "js/Value.h"         // JS::{,Int32}Value
#include "vm/NativeObject.h"  // js::NativeObject

struct JSContext;

namespace js {

class WritableStream : public NativeObject {
 public:
  /**
   * Memory layout of WritableStream instances.
   *
   * See https://streams.spec.whatwg.org/#ws-internal-slots for details on
   * the stored state.  [[state]] and [[backpressure]] are stored in Slot_State
   * as WritableStream::State enum values.
   *
   * XXX jwalden needs fleshin' out
   */
  enum Slots { Slot_State, SlotCount };

 private:
  enum State : uint32_t {
    Writable = 0x0000'0000,
    Closed = 0x0000'0001,
    Erroring = 0x0000'0002,
    Errored = 0x0000'0003,
    StateBits = 0x0000'0003,
    StateMask = 0x0000'00ff,

    Backpressure = 0x0000'0100,
    FlagBits = Backpressure,
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
