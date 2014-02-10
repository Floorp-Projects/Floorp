/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Slot_h
#define jit_Slot_h

#include "jit/Registers.h"

namespace js {
namespace jit {

class CompactBufferReader;
class CompactBufferWriter;

class Slot;

class Location
{
    friend class Slot;

    // An offset that is illegal for a local variable's stack allocation.
    static const int32_t InvalidStackSlot = -1;

    Register::Code reg_;
    int32_t stackSlot_;

    static Location From(const Register &reg) {
        Location loc;
        loc.reg_ = reg.code();
        loc.stackSlot_ = InvalidStackSlot;
        return loc;
    }
    static Location From(int32_t stackSlot) {
        JS_ASSERT(stackSlot != InvalidStackSlot);
        Location loc;
        loc.reg_ = Register::Code(0);      // Quell compiler warnings.
        loc.stackSlot_ = stackSlot;
        return loc;
    }

  public:
    Register reg() const {
        JS_ASSERT(!isStackSlot());
        return Register::FromCode(reg_);
    }
    int32_t stackSlot() const {
        JS_ASSERT(isStackSlot());
        return stackSlot_;
    }
    bool isStackSlot() const {
        return stackSlot_ != InvalidStackSlot;
    }

    void dump(FILE *fp) const;

  public:
    bool operator==(const Location &l) const {
        return reg_ == l.reg_ && stackSlot_ == l.stackSlot_;
    }
};

class Slot
{
  public:
    enum Mode
    {
        CONSTANT,           // An index into the constant pool.
        DOUBLE_REG,         // Type is double, payload is in a register.
        FLOAT32_REG,        // Type is float32, payload is in a register.
        FLOAT32_STACK,      // Type is float32, payload is on the stack.
        TYPED_REG,          // Type is constant, payload is in a register.
        TYPED_STACK,        // Type is constant, payload is on the stack.
        UNTYPED,            // Type is not known.
        JS_UNDEFINED,       // UndefinedValue()
        JS_NULL,            // NullValue()
        JS_INT32,           // Int32Value(n)
        INVALID
    };

  private:
    Mode mode_;

    union {
        // DOUBLE_REG or FLOAT32_REG
        FloatRegister::Code fpu_;

        // TYPED_REG or TYPED_STACK or FLOAT32_STACK
        struct {
            JSValueType type;
            Location payload;
        } known_type_;

        // UNTYPED
#if defined(JS_NUNBOX32)
        struct {
            Location type;
            Location payload;
        } unknown_type_;
#elif defined(JS_PUNBOX64)
        struct {
            Location value;
        } unknown_type_;
#endif

        // CONSTANT's index or JS_INT32
        int32_t value_;
    };

    Slot(Mode mode, JSValueType type, const Location &loc)
      : mode_(mode)
    {
        JS_ASSERT(mode == TYPED_REG || mode == TYPED_STACK);
        known_type_.type = type;
        known_type_.payload = loc;
    }
    Slot(Mode mode, const FloatRegister &reg)
      : mode_(mode)
    {
        JS_ASSERT(mode == FLOAT32_REG || mode == DOUBLE_REG);
        fpu_ = reg.code();
    }
    Slot(Mode mode, const Location &loc)
      : mode_(mode)
    {
        JS_ASSERT(mode == FLOAT32_STACK);
        known_type_.payload = loc;
    }
    Slot(Mode mode, int32_t index)
      : mode_(mode)
    {
        JS_ASSERT(mode == CONSTANT || mode == JS_INT32);
        value_ = index;
    }
    Slot(Mode mode)
      : mode_(mode)
    {
        JS_ASSERT(mode == JS_UNDEFINED || mode == JS_NULL || mode == UNTYPED);
    }

  public:
    Slot()
      : mode_(INVALID)
    { }

    // DOUBLE_REG
    static Slot DoubleSlot(const FloatRegister &reg) {
        return Slot(DOUBLE_REG, reg);
    }

    // FLOAT32_REG or FLOAT32_STACK
    static Slot Float32Slot(const FloatRegister &reg) {
        return Slot(FLOAT32_REG, reg);
    }
    static Slot Float32Slot(int32_t stackIndex) {
        return Slot(FLOAT32_STACK, Location::From(stackIndex));
    }

    // TYPED_REG or TYPED_STACK
    static Slot TypedSlot(JSValueType type, const Register &reg) {
        JS_ASSERT(type != JSVAL_TYPE_DOUBLE &&
                  type != JSVAL_TYPE_MAGIC &&
                  type != JSVAL_TYPE_NULL &&
                  type != JSVAL_TYPE_UNDEFINED);
        return Slot(TYPED_REG, type, Location::From(reg));
    }
    static Slot TypedSlot(JSValueType type, int32_t stackIndex) {
        JS_ASSERT(type != JSVAL_TYPE_MAGIC &&
                  type != JSVAL_TYPE_NULL &&
                  type != JSVAL_TYPE_UNDEFINED);
        return Slot(TYPED_STACK, type, Location::From(stackIndex));
    }

    // UNTYPED
#if defined(JS_NUNBOX32)
    static Slot UntypedSlot(const Register &type, const Register &payload) {
        Slot slot(UNTYPED);
        slot.unknown_type_.type = Location::From(type);
        slot.unknown_type_.payload = Location::From(payload);
        return slot;
    }

    static Slot UntypedSlot(const Register &type, int32_t payloadStackIndex) {
        Slot slot(UNTYPED);
        slot.unknown_type_.type = Location::From(type);
        slot.unknown_type_.payload = Location::From(payloadStackIndex);
        return slot;
    }

    static Slot UntypedSlot(int32_t typeStackIndex, const Register &payload) {
        Slot slot(UNTYPED);
        slot.unknown_type_.type = Location::From(typeStackIndex);
        slot.unknown_type_.payload = Location::From(payload);
        return slot;
    }

    static Slot UntypedSlot(int32_t typeStackIndex, int32_t payloadStackIndex) {
        Slot slot(UNTYPED);
        slot.unknown_type_.type = Location::From(typeStackIndex);
        slot.unknown_type_.payload = Location::From(payloadStackIndex);
        return slot;
    }

#elif defined(JS_PUNBOX64)
    static Slot UntypedSlot(const Register &value) {
        Slot slot(UNTYPED);
        slot.unknown_type_.value = Location::From(value);
        return slot;
    }

    static Slot UntypedSlot(int32_t valueStackSlot) {
        Slot slot(UNTYPED);
        slot.unknown_type_.value = Location::From(valueStackSlot);
        return slot;
    }
#endif

    // common constants.
    static Slot UndefinedSlot() {
        return Slot(JS_UNDEFINED);
    }
    static Slot NullSlot() {
        return Slot(JS_NULL);
    }

    // JS_INT32
    static Slot Int32Slot(int32_t value) {
        return Slot(JS_INT32, value);
    }

    // CONSTANT's index
    static Slot ConstantPoolSlot(uint32_t index) {
        return Slot(CONSTANT, int32_t(index));
    }

    void writeSlotHeader(CompactBufferWriter &writer, JSValueType type, uint32_t regCode) const;
  public:
    static Slot read(CompactBufferReader &reader);
    void write(CompactBufferWriter &writer) const;

  public:
    Mode mode() const {
        return mode_;
    }
    uint32_t constantIndex() const {
        JS_ASSERT(mode() == CONSTANT);
        return value_;
    }
    int32_t int32Value() const {
        JS_ASSERT(mode() == JS_INT32);
        return value_;
    }
    JSValueType knownType() const {
        JS_ASSERT(mode() == TYPED_REG || mode() == TYPED_STACK);
        return known_type_.type;
    }
    Register reg() const {
        JS_ASSERT(mode() == TYPED_REG && knownType() != JSVAL_TYPE_DOUBLE);
        return known_type_.payload.reg();
    }
    FloatRegister floatReg() const {
        JS_ASSERT(mode() == DOUBLE_REG || mode() == FLOAT32_REG);
        return FloatRegister::FromCode(fpu_);
    }
    int32_t stackSlot() const {
        JS_ASSERT(mode() == TYPED_STACK || mode() == FLOAT32_STACK);
        return known_type_.payload.stackSlot();
    }
#if defined(JS_NUNBOX32)
    Location payload() const {
        JS_ASSERT(mode() == UNTYPED);
        return unknown_type_.payload;
    }
    Location type() const {
        JS_ASSERT(mode() == UNTYPED);
        return unknown_type_.type;
    }
#elif defined(JS_PUNBOX64)
    Location value() const {
        JS_ASSERT(mode() == UNTYPED);
        return unknown_type_.value;
    }
#endif

  public:
    const char *modeToString() const;
    void dump(FILE *fp) const;
    void dump() const;

  public:
    bool operator==(const Slot &s) const {
        if (mode_ != s.mode_)
            return false;

        switch (mode_) {
          case DOUBLE_REG:
          case FLOAT32_REG:
            return fpu_ == s.fpu_;
          case TYPED_REG:
          case TYPED_STACK:
          case FLOAT32_STACK:
            return known_type_.type == s.known_type_.type &&
                known_type_.payload == s.known_type_.payload;
          case UNTYPED:
#if defined(JS_NUNBOX32)
            return unknown_type_.type == s.unknown_type_.type &&
                unknown_type_.payload == s.unknown_type_.payload;
#else
            return unknown_type_.value == s.unknown_type_.value;
#endif
          case CONSTANT:
          case JS_INT32:
            return value_ == s.value_;
          default:
            return true;
        }
    }
};

}
}

#endif // jit_Slot_h
