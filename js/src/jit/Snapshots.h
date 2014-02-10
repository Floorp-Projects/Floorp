/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Snapshot_h
#define jit_Snapshot_h

#include "jsbytecode.h"

#include "jit/CompactBuffer.h"
#include "jit/IonTypes.h"
#include "jit/Registers.h"

namespace js {
namespace jit {

class RValueAllocation;

class Location
{
    friend class RValueAllocation;

    // An offset that is illegal for a local variable's stack allocation.
    static const int32_t InvalidStackOffset = -1;

    Register::Code reg_;
    int32_t stackOffset_;

    static Location From(const Register &reg) {
        Location loc;
        loc.reg_ = reg.code();
        loc.stackOffset_ = InvalidStackOffset;
        return loc;
    }
    static Location From(int32_t stackOffset) {
        JS_ASSERT(stackOffset != InvalidStackOffset);
        Location loc;
        loc.reg_ = Register::Code(0);      // Quell compiler warnings.
        loc.stackOffset_ = stackOffset;
        return loc;
    }

  public:
    Register reg() const {
        JS_ASSERT(!isStackOffset());
        return Register::FromCode(reg_);
    }
    int32_t stackOffset() const {
        JS_ASSERT(isStackOffset());
        return stackOffset_;
    }
    bool isStackOffset() const {
        return stackOffset_ != InvalidStackOffset;
    }

    void dump(FILE *fp) const;

  public:
    bool operator==(const Location &l) const {
        return reg_ == l.reg_ && stackOffset_ == l.stackOffset_;
    }
};

// A Recover Value Allocation mirror what is known at compiled time as being the
// MIRType and the LAllocation.  This is read out of the snapshot to recover the
// value which would be there if this frame was an interpreter frame instead of
// an Ion frame.
//
// It is used with the SnapshotIterator to recover a Value from the stack,
// spilled registers or the list of constant of the compiled script.
class RValueAllocation
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
#if defined(JS_NUNBOX32)
        UNTYPED_REG_REG,    // Type is not known, type & payload are is in
                            // registers.
        UNTYPED_REG_STACK,  // Type is not known, type is in a register and the
                            // payload is on the stack.
        UNTYPED_STACK_REG,  // Type is not known, type is on the stack and the
                            // payload is in a register.
        UNTYPED_STACK_STACK, // Type is not known, type & payload are on the
                            // stack.
#elif defined(JS_PUNBOX64)
        UNTYPED_REG,        // Type is not known, value is in a register.
        UNTYPED_STACK,      // Type is not known, value is on the stack.
#endif
        JS_UNDEFINED,       // UndefinedValue()
        JS_NULL,            // NullValue()
        JS_INT32,           // Int32Value(n)
        INVALID,

        // Assert that the mode is UNTYPED by checking the range.
#if defined(JS_NUNBOX32)
        UNTYPED_MIN = UNTYPED_REG_REG,
        UNTYPED_MAX = UNTYPED_STACK_STACK
#elif defined(JS_PUNBOX64)
        UNTYPED_MIN = UNTYPED_REG,
        UNTYPED_MAX = UNTYPED_STACK
#endif
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

    RValueAllocation(Mode mode, JSValueType type, const Location &loc)
      : mode_(mode)
    {
        JS_ASSERT(mode == TYPED_REG || mode == TYPED_STACK);
        known_type_.type = type;
        known_type_.payload = loc;
    }
    RValueAllocation(Mode mode, const FloatRegister &reg)
      : mode_(mode)
    {
        JS_ASSERT(mode == FLOAT32_REG || mode == DOUBLE_REG);
        fpu_ = reg.code();
    }
    RValueAllocation(Mode mode, const Location &loc)
      : mode_(mode)
    {
        JS_ASSERT(mode == FLOAT32_STACK);
        known_type_.payload = loc;
    }
    RValueAllocation(Mode mode, int32_t index)
      : mode_(mode)
    {
        JS_ASSERT(mode == CONSTANT || mode == JS_INT32);
        value_ = index;
    }
    RValueAllocation(Mode mode)
      : mode_(mode)
    {
        JS_ASSERT(mode == JS_UNDEFINED || mode == JS_NULL ||
                  (UNTYPED_MIN <= mode && mode <= UNTYPED_MAX));
    }

  public:
    RValueAllocation()
      : mode_(INVALID)
    { }

    // DOUBLE_REG
    static RValueAllocation Double(const FloatRegister &reg) {
        return RValueAllocation(DOUBLE_REG, reg);
    }

    // FLOAT32_REG or FLOAT32_STACK
    static RValueAllocation Float32(const FloatRegister &reg) {
        return RValueAllocation(FLOAT32_REG, reg);
    }
    static RValueAllocation Float32(int32_t stackIndex) {
        return RValueAllocation(FLOAT32_STACK, Location::From(stackIndex));
    }

    // TYPED_REG or TYPED_STACK
    static RValueAllocation Typed(JSValueType type, const Register &reg) {
        JS_ASSERT(type != JSVAL_TYPE_DOUBLE &&
                  type != JSVAL_TYPE_MAGIC &&
                  type != JSVAL_TYPE_NULL &&
                  type != JSVAL_TYPE_UNDEFINED);
        return RValueAllocation(TYPED_REG, type, Location::From(reg));
    }
    static RValueAllocation Typed(JSValueType type, int32_t stackIndex) {
        JS_ASSERT(type != JSVAL_TYPE_MAGIC &&
                  type != JSVAL_TYPE_NULL &&
                  type != JSVAL_TYPE_UNDEFINED);
        return RValueAllocation(TYPED_STACK, type, Location::From(stackIndex));
    }

    // UNTYPED
#if defined(JS_NUNBOX32)
    static RValueAllocation Untyped(const Register &type, const Register &payload) {
        RValueAllocation slot(UNTYPED_REG_REG);
        slot.unknown_type_.type = Location::From(type);
        slot.unknown_type_.payload = Location::From(payload);
        return slot;
    }

    static RValueAllocation Untyped(const Register &type, int32_t payloadStackIndex) {
        RValueAllocation slot(UNTYPED_REG_STACK);
        slot.unknown_type_.type = Location::From(type);
        slot.unknown_type_.payload = Location::From(payloadStackIndex);
        return slot;
    }

    static RValueAllocation Untyped(int32_t typeStackIndex, const Register &payload) {
        RValueAllocation slot(UNTYPED_STACK_REG);
        slot.unknown_type_.type = Location::From(typeStackIndex);
        slot.unknown_type_.payload = Location::From(payload);
        return slot;
    }

    static RValueAllocation Untyped(int32_t typeStackIndex, int32_t payloadStackIndex) {
        RValueAllocation slot(UNTYPED_STACK_STACK);
        slot.unknown_type_.type = Location::From(typeStackIndex);
        slot.unknown_type_.payload = Location::From(payloadStackIndex);
        return slot;
    }

#elif defined(JS_PUNBOX64)
    static RValueAllocation Untyped(const Register &value) {
        RValueAllocation slot(UNTYPED_REG);
        slot.unknown_type_.value = Location::From(value);
        return slot;
    }

    static RValueAllocation Untyped(int32_t stackOffset) {
        RValueAllocation slot(UNTYPED_STACK);
        slot.unknown_type_.value = Location::From(stackOffset);
        return slot;
    }
#endif

    // common constants.
    static RValueAllocation Undefined() {
        return RValueAllocation(JS_UNDEFINED);
    }
    static RValueAllocation Null() {
        return RValueAllocation(JS_NULL);
    }

    // JS_INT32
    static RValueAllocation Int32(int32_t value) {
        return RValueAllocation(JS_INT32, value);
    }

    // CONSTANT's index
    static RValueAllocation ConstantPool(uint32_t index) {
        return RValueAllocation(CONSTANT, int32_t(index));
    }

    void writeHeader(CompactBufferWriter &writer, JSValueType type, uint32_t regCode) const;
  public:
    static RValueAllocation read(CompactBufferReader &reader);
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
    int32_t stackOffset() const {
        JS_ASSERT(mode() == TYPED_STACK || mode() == FLOAT32_STACK);
        return known_type_.payload.stackOffset();
    }
#if defined(JS_NUNBOX32)
    Location payload() const {
        JS_ASSERT((UNTYPED_MIN <= mode() && mode() <= UNTYPED_MAX));
        return unknown_type_.payload;
    }
    Location type() const {
        JS_ASSERT((UNTYPED_MIN <= mode() && mode() <= UNTYPED_MAX));
        return unknown_type_.type;
    }
#elif defined(JS_PUNBOX64)
    Location value() const {
        JS_ASSERT((UNTYPED_MIN <= mode() && mode() <= UNTYPED_MAX));
        return unknown_type_.value;
    }
#endif

  public:
    const char *modeToString() const;
    void dump(FILE *fp) const;
    void dump() const;

  public:
    bool operator==(const RValueAllocation &s) const {
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
#if defined(JS_NUNBOX32)
          case UNTYPED_REG_REG:
          case UNTYPED_REG_STACK:
          case UNTYPED_STACK_REG:
          case UNTYPED_STACK_STACK:
            return unknown_type_.type == s.unknown_type_.type &&
                unknown_type_.payload == s.unknown_type_.payload;
#else
          case UNTYPED_REG:
          case UNTYPED_STACK:
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

// Collects snapshots in a contiguous buffer, which is copied into IonScript
// memory after code generation.
class SnapshotWriter
{
    CompactBufferWriter writer_;

    // These are only used to assert sanity.
    uint32_t nallocs_;
    uint32_t allocWritten_;
    uint32_t nframes_;
    uint32_t framesWritten_;
    SnapshotOffset lastStart_;

  public:
    SnapshotOffset startSnapshot(uint32_t frameCount, BailoutKind kind, bool resumeAfter);
    void startFrame(JSFunction *fun, JSScript *script, jsbytecode *pc, uint32_t exprStack);
#ifdef TRACK_SNAPSHOTS
    void trackFrame(uint32_t pcOpcode, uint32_t mirOpcode, uint32_t mirId,
                    uint32_t lirOpcode, uint32_t lirId);
#endif
    void endFrame();

    void add(const RValueAllocation &slot);

    void endSnapshot();

    bool oom() const {
        return writer_.oom() || writer_.length() >= MAX_BUFFER_SIZE;
    }

    size_t size() const {
        return writer_.length();
    }
    const uint8_t *buffer() const {
        return writer_.buffer();
    }
};

// A snapshot reader reads the entries out of the compressed snapshot buffer in
// a script. These entries describe the equivalent interpreter frames at a given
// position in JIT code. Each entry is an Ion's value allocations, used to
// recover the corresponding Value from an Ion frame.
class SnapshotReader
{
    CompactBufferReader reader_;

    uint32_t pcOffset_;           // Offset from script->code.
    uint32_t allocCount_;         // Number of slots.
    uint32_t frameCount_;
    BailoutKind bailoutKind_;
    uint32_t framesRead_;         // Number of frame headers that have been read.
    uint32_t allocRead_;          // Number of slots that have been read.
    bool resumeAfter_;

#ifdef TRACK_SNAPSHOTS
  private:
    uint32_t pcOpcode_;
    uint32_t mirOpcode_;
    uint32_t mirId_;
    uint32_t lirOpcode_;
    uint32_t lirId_;

  public:
    void spewBailingFrom() const;
#endif

  private:
    void readSnapshotHeader();
    void readFrameHeader();

  public:
    SnapshotReader(const uint8_t *buffer, const uint8_t *end);

    uint32_t pcOffset() const {
        return pcOffset_;
    }
    uint32_t allocations() const {
        return allocCount_;
    }
    BailoutKind bailoutKind() const {
        return bailoutKind_;
    }
    bool resumeAfter() const {
        if (moreFrames())
            return false;
        return resumeAfter_;
    }
    bool moreFrames() const {
        return framesRead_ < frameCount_;
    }
    void nextFrame() {
        readFrameHeader();
    }
    RValueAllocation readAllocation();

    bool moreAllocations() const {
        return allocRead_ < allocCount_;
    }
    uint32_t frameCount() const {
        return frameCount_;
    }
};

}
}

#endif /* jit_Snapshot_h */
