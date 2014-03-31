/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_Snapshot_h
#define jit_Snapshot_h

#include "jsalloc.h"
#include "jsbytecode.h"

#include "jit/CompactBuffer.h"
#include "jit/IonTypes.h"
#include "jit/Registers.h"

#include "js/HashTable.h"

namespace js {
namespace jit {

class RValueAllocation;

// A Recover Value Allocation mirror what is known at compiled time as being the
// MIRType and the LAllocation.  This is read out of the snapshot to recover the
// value which would be there if this frame was an interpreter frame instead of
// an Ion frame.
//
// It is used with the SnapshotIterator to recover a Value from the stack,
// spilled registers or the list of constant of the compiled script.
//
// Unit tests are located in jsapi-tests/testJitRValueAlloc.cpp.
class RValueAllocation
{
  public:

    // See RValueAllocation encoding in Snapshots.cpp
    enum Mode
    {
        CONSTANT            = 0x00,
        CST_UNDEFINED       = 0x01,
        CST_NULL            = 0x02,
        DOUBLE_REG          = 0x03,
        FLOAT32_REG         = 0x04,
        FLOAT32_STACK       = 0x05,
#if defined(JS_NUNBOX32)
        UNTYPED_REG_REG     = 0x06,
        UNTYPED_REG_STACK   = 0x07,
        UNTYPED_STACK_REG   = 0x08,
        UNTYPED_STACK_STACK = 0x09,
#elif defined(JS_PUNBOX64)
        UNTYPED_REG         = 0x06,
        UNTYPED_STACK       = 0x07,
#endif
        // The JSValueType is packed in the Mode.
        TYPED_REG_MIN       = 0x10,
        TYPED_REG_MAX       = 0x17,
        TYPED_REG = TYPED_REG_MIN,

        // The JSValueType is packed in the Mode.
        TYPED_STACK_MIN     = 0x18,
        TYPED_STACK_MAX     = 0x1f,
        TYPED_STACK = TYPED_STACK_MIN,

        INVALID = 0x100,
    };

    // See Payload encoding in Snapshots.cpp
    enum PayloadType {
        PAYLOAD_NONE,
        PAYLOAD_INDEX,
        PAYLOAD_STACK_OFFSET,
        PAYLOAD_GPR,
        PAYLOAD_FPU,
        PAYLOAD_PACKED_TAG
    };

    struct Layout {
        PayloadType type1;
        PayloadType type2;
        const char *name;
    };

  private:
    Mode mode_;

    // Additional information to recover the content of the allocation.
    union Payload {
        uint32_t index;
        int32_t stackOffset;
        Register gpr;
        FloatRegister fpu;
        JSValueType type;
    };

    Payload arg1_;
    Payload arg2_;

    static Payload payloadOfIndex(uint32_t index) {
        Payload p;
        p.index = index;
        return p;
    }
    static Payload payloadOfStackOffset(int32_t offset) {
        Payload p;
        p.stackOffset = offset;
        return p;
    }
    static Payload payloadOfRegister(Register reg) {
        Payload p;
        p.gpr = reg;
        return p;
    }
    static Payload payloadOfFloatRegister(FloatRegister reg) {
        Payload p;
        p.fpu = reg;
        return p;
    }
    static Payload payloadOfValueType(JSValueType type) {
        Payload p;
        p.type = type;
        return p;
    }

    static const Layout &layoutFromMode(Mode mode);

    static void readPayload(CompactBufferReader &reader, PayloadType t,
                            uint8_t *mode, Payload *p);
    static void writePayload(CompactBufferWriter &writer, PayloadType t,
                             Payload p);
    static void writePadding(CompactBufferWriter &writer);
    static void dumpPayload(FILE *fp, PayloadType t, Payload p);
    static bool equalPayloads(PayloadType t, Payload lhs, Payload rhs);

    RValueAllocation(Mode mode, Payload a1, Payload a2)
      : mode_(mode),
        arg1_(a1),
        arg2_(a2)
    {
    }

    RValueAllocation(Mode mode, Payload a1)
      : mode_(mode),
        arg1_(a1)
    {
    }

    RValueAllocation(Mode mode)
      : mode_(mode)
    {
    }

  public:
    RValueAllocation()
      : mode_(INVALID)
    { }

    // DOUBLE_REG
    static RValueAllocation Double(const FloatRegister &reg) {
        return RValueAllocation(DOUBLE_REG, payloadOfFloatRegister(reg));
    }

    // FLOAT32_REG or FLOAT32_STACK
    static RValueAllocation Float32(const FloatRegister &reg) {
        return RValueAllocation(FLOAT32_REG, payloadOfFloatRegister(reg));
    }
    static RValueAllocation Float32(int32_t offset) {
        return RValueAllocation(FLOAT32_STACK, payloadOfStackOffset(offset));
    }

    // TYPED_REG or TYPED_STACK
    static RValueAllocation Typed(JSValueType type, const Register &reg) {
        JS_ASSERT(type != JSVAL_TYPE_DOUBLE &&
                  type != JSVAL_TYPE_MAGIC &&
                  type != JSVAL_TYPE_NULL &&
                  type != JSVAL_TYPE_UNDEFINED);
        return RValueAllocation(TYPED_REG, payloadOfValueType(type),
                                payloadOfRegister(reg));
    }
    static RValueAllocation Typed(JSValueType type, int32_t offset) {
        JS_ASSERT(type != JSVAL_TYPE_MAGIC &&
                  type != JSVAL_TYPE_NULL &&
                  type != JSVAL_TYPE_UNDEFINED);
        return RValueAllocation(TYPED_STACK, payloadOfValueType(type),
                                payloadOfStackOffset(offset));
    }

    // UNTYPED
#if defined(JS_NUNBOX32)
    static RValueAllocation Untyped(const Register &type, const Register &payload) {
        return RValueAllocation(UNTYPED_REG_REG,
                                payloadOfRegister(type),
                                payloadOfRegister(payload));
    }

    static RValueAllocation Untyped(const Register &type, int32_t payloadStackOffset) {
        return RValueAllocation(UNTYPED_REG_STACK,
                                payloadOfRegister(type),
                                payloadOfStackOffset(payloadStackOffset));
    }

    static RValueAllocation Untyped(int32_t typeStackOffset, const Register &payload) {
        return RValueAllocation(UNTYPED_STACK_REG,
                                payloadOfStackOffset(typeStackOffset),
                                payloadOfRegister(payload));
    }

    static RValueAllocation Untyped(int32_t typeStackOffset, int32_t payloadStackOffset) {
        return RValueAllocation(UNTYPED_STACK_STACK,
                                payloadOfStackOffset(typeStackOffset),
                                payloadOfStackOffset(payloadStackOffset));
    }

#elif defined(JS_PUNBOX64)
    static RValueAllocation Untyped(const Register &reg) {
        return RValueAllocation(UNTYPED_REG, payloadOfRegister(reg));
    }

    static RValueAllocation Untyped(int32_t stackOffset) {
        return RValueAllocation(UNTYPED_STACK, payloadOfStackOffset(stackOffset));
    }
#endif

    // common constants.
    static RValueAllocation Undefined() {
        return RValueAllocation(CST_UNDEFINED);
    }
    static RValueAllocation Null() {
        return RValueAllocation(CST_NULL);
    }

    // CONSTANT's index
    static RValueAllocation ConstantPool(uint32_t index) {
        return RValueAllocation(CONSTANT, payloadOfIndex(index));
    }

    void writeHeader(CompactBufferWriter &writer, JSValueType type, uint32_t regCode) const;
  public:
    static RValueAllocation read(CompactBufferReader &reader);
    void write(CompactBufferWriter &writer) const;

  public:
    Mode mode() const {
        return mode_;
    }

    uint32_t index() const {
        JS_ASSERT(layoutFromMode(mode()).type1 == PAYLOAD_INDEX);
        return arg1_.index;
    }
    int32_t stackOffset() const {
        JS_ASSERT(layoutFromMode(mode()).type1 == PAYLOAD_STACK_OFFSET);
        return arg1_.stackOffset;
    }
    Register reg() const {
        JS_ASSERT(layoutFromMode(mode()).type1 == PAYLOAD_GPR);
        return arg1_.gpr;
    }
    FloatRegister fpuReg() const {
        JS_ASSERT(layoutFromMode(mode()).type1 == PAYLOAD_FPU);
        return arg1_.fpu;
    }
    JSValueType knownType() const {
        JS_ASSERT(layoutFromMode(mode()).type1 == PAYLOAD_PACKED_TAG);
        return arg1_.type;
    }

    int32_t stackOffset2() const {
        JS_ASSERT(layoutFromMode(mode()).type2 == PAYLOAD_STACK_OFFSET);
        return arg2_.stackOffset;
    }
    Register reg2() const {
        JS_ASSERT(layoutFromMode(mode()).type2 == PAYLOAD_GPR);
        return arg2_.gpr;
    }

  public:
    void dump(FILE *fp) const;

  public:
    bool operator==(const RValueAllocation &rhs) const {
        if (mode_ != rhs.mode_)
            return false;

        const Layout &layout = layoutFromMode(mode());
        return equalPayloads(layout.type1, arg1_, rhs.arg1_) &&
            equalPayloads(layout.type2, arg2_, rhs.arg2_);
    }

    HashNumber hash() const;

    struct Hasher
    {
        typedef RValueAllocation Key;
        typedef Key Lookup;
        static HashNumber hash(const Lookup &v) {
            return v.hash();
        }
        static bool match(const Key &k, const Lookup &l) {
            return k == l;
        }
    };
};

typedef uint32_t RecoverOffset;

class RecoverWriter;

// Collects snapshots in a contiguous buffer, which is copied into IonScript
// memory after code generation.
class SnapshotWriter
{
    friend class RecoverWriter;
    CompactBufferWriter writer_;
    CompactBufferWriter allocWriter_;

    // Map RValueAllocations to an offset in the allocWriter_ buffer.  This is
    // useful as value allocations are repeated frequently.
    typedef RValueAllocation RVA;
    typedef HashMap<RVA, uint32_t, RVA::Hasher, SystemAllocPolicy> RValueAllocMap;
    RValueAllocMap allocMap_;

    // This is only used to assert sanity.
    uint32_t allocWritten_;

    // Used to report size of the snapshot in the spew messages.
    SnapshotOffset lastStart_;

  public:
    bool init();

    SnapshotOffset startSnapshot(RecoverOffset recoverOffset, BailoutKind kind);
#ifdef TRACK_SNAPSHOTS
    void trackSnapshot(uint32_t pcOpcode, uint32_t mirOpcode, uint32_t mirId,
                       uint32_t lirOpcode, uint32_t lirId);
#endif
    bool add(const RValueAllocation &slot);

    void endSnapshot();

    bool oom() const {
        return writer_.oom() || writer_.length() >= MAX_BUFFER_SIZE ||
            allocWriter_.oom() || allocWriter_.length() >= MAX_BUFFER_SIZE;
    }

    size_t listSize() const {
        return writer_.length();
    }
    const uint8_t *listBuffer() const {
        return writer_.buffer();
    }

    size_t RVATableSize() const {
        return allocWriter_.length();
    }
    const uint8_t *RVATableBuffer() const {
        return allocWriter_.buffer();
    }
};

class RecoverWriter
{
    CompactBufferWriter writer_;
    SnapshotWriter &snapshot_;

    uint32_t nallocs_;

    uint32_t nframes_;
    uint32_t framesWritten_;

  public:
    RecoverWriter(SnapshotWriter &snapshot);

    SnapshotOffset startRecover(uint32_t frameCount, BailoutKind kind, bool resumeAfter);

    void startFrame(JSFunction *fun, JSScript *script, jsbytecode *pc, uint32_t exprStack);
    void endFrame();

    void endRecover();

    size_t size() const {
        return writer_.length();
    }
    const uint8_t *buffer() const {
        return writer_.buffer();
    }

    bool oom() const {
        return writer_.oom() || writer_.length() >= MAX_BUFFER_SIZE;
    }
};

class RecoverReader;

// A snapshot reader reads the entries out of the compressed snapshot buffer in
// a script. These entries describe the equivalent interpreter frames at a given
// position in JIT code. Each entry is an Ion's value allocations, used to
// recover the corresponding Value from an Ion frame.
class SnapshotReader
{
    friend class RecoverReader;

    CompactBufferReader reader_;
    CompactBufferReader allocReader_;
    const uint8_t* allocTable_;

    BailoutKind bailoutKind_;
    uint32_t allocRead_;          // Number of slots that have been read.
    RecoverOffset recoverOffset_; // Offset of the recover instructions.

#ifdef TRACK_SNAPSHOTS
  private:
    uint32_t pcOpcode_;
    uint32_t mirOpcode_;
    uint32_t mirId_;
    uint32_t lirOpcode_;
    uint32_t lirId_;

  public:
    void readTrackSnapshot();
    void spewBailingFrom() const;
#endif

  private:
    void readSnapshotHeader();
    void readFrameHeader();

  public:
    SnapshotReader(const uint8_t *snapshots, uint32_t offset,
                   uint32_t RVATableSize, uint32_t listSize);

    RValueAllocation readAllocation();

    BailoutKind bailoutKind() const {
        return bailoutKind_;
    }
    RecoverOffset recoverOffset() const {
        return recoverOffset_;
    }
};

class RecoverReader
{
    CompactBufferReader reader_;

    uint32_t frameCount_;
    uint32_t framesRead_;         // Number of frame headers that have been read.
    uint32_t pcOffset_;           // Offset from script->code.
    uint32_t allocCount_;         // Number of slots.
    bool resumeAfter_;

  private:
    void readRecoverHeader();
    void readFrame(SnapshotReader &snapshot);

  public:
    RecoverReader(SnapshotReader &snapshot, const uint8_t *recovers, uint32_t size);

    bool moreFrames() const {
        return framesRead_ < frameCount_;
    }
    void nextFrame(SnapshotReader &snapshot) {
        readFrame(snapshot);
    }
    uint32_t frameCount() const {
        return frameCount_;
    }

    uint32_t pcOffset() const {
        return pcOffset_;
    }
    bool resumeAfter() const {
        return resumeAfter_;
    }

    uint32_t allocations() const {
        return allocCount_;
    }
    bool moreAllocations(const SnapshotReader &snapshot) const {
        return snapshot.allocRead_ < allocCount_;
    }
};

}
}

#endif /* jit_Snapshot_h */
