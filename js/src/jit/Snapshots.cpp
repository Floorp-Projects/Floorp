/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Snapshots.h"

#include "jsscript.h"

#include "jit/CompileInfo.h"
#include "jit/IonSpewer.h"
#ifdef TRACK_SNAPSHOTS
# include "jit/LIR.h"
# include "jit/MIR.h"
#endif

using namespace js;
using namespace js::jit;

// Snapshot header:
//
//   [vwu] bits (n-31]: frame count
//         bits [0,n):  bailout kind (n = BAILOUT_KIND_BITS)
//
// Snapshot body, repeated "frame count" times, from oldest frame to newest frame.
// Note that the first frame doesn't have the "parent PC" field.
//
//   [ptr] Debug only: JSScript *
//   [vwu] pc offset
//   [vwu] # of slots, including nargs
//  [rva*] N recover value allocations entries,
//             where N = nargs + nfixed + stackDepth
//
// Encodings:
//   [ptr] A fixed-size pointer.
//   [vwu] A variable-width unsigned integer.
//   [vws] A variable-width signed integer.
//    [u8] An 8-bit unsigned integer.
//  [rva*] list of RValueAllocation which are indicating how to find a js::Value
//         on a call/bailout. The first byte is split as thus:
//           Bits 0-2: JSValueType
//           Bits 3-7: 5-bit register code ("reg").
//
//         JSVAL_TYPE_DOUBLE:
//              If "reg" is InvalidFloatReg, this byte is followed by a
//              [vws] stack offset. Otherwise, "reg" encodes an XMM register.
//
//         JSVAL_TYPE_INT32:
//         JSVAL_TYPE_OBJECT:
//         JSVAL_TYPE_BOOLEAN:
//         JSVAL_TYPE_STRING:
//              If "reg" is ESC_REG_FIELD_INDEX, this byte is followed by a
//              [vws] stack offset. Otherwise, "reg" encodes a GPR register.
//
//         JSVAL_TYPE_NULL:
//              Reg value:
//                 0-27: Constant integer; Int32Value(n)
//                   28: Variable float32; Float register code
//                   29: Variable float32; Stack index
//                   30: NullValue()
//                   31: Constant integer; Int32Value([vws])
//
//         JSVAL_TYPE_UNDEFINED:
//              Reg value:
//                 0-27: Constant value, index n into ionScript->constants()
//                28-29: unused
//                   30: UndefinedValue()
//                   31: Constant value, index [vwu] into
//                       ionScript->constants()
//
//         JSVAL_TYPE_MAGIC: (reg value is 30)
//              The value is a lazy argument object. Followed by extra fields
//              indicating the location of the payload.
//              [vwu] reg2 (0-29)
//              [vwu] reg2 (31) [vws] stack index
//
//         JSVAL_TYPE_MAGIC:
//              The type is not statically known. The meaning of this depends
//              on the boxing style.
//
//              NUNBOX32:
//                  Followed by a type and payload indicator that are one of
//                  the following:
//                   code=0 [vws] stack slot, [vws] stack slot
//                   code=1 [vws] stack slot, reg
//                   code=2 reg, [vws] stack slot
//                   code=3 reg, reg
//
//              PUNBOX64:
//                  "reg" is ESC_REG_FIELD_INDEX: byte is followed by a [vws]
//                  stack offset containing a Value.
//
//                  Otherwise, "reg" is a register containing a Value.
//

#ifdef JS_NUNBOX32
static const uint32_t NUNBOX32_STACK_STACK = 0;
static const uint32_t NUNBOX32_STACK_REG   = 1;
static const uint32_t NUNBOX32_REG_STACK   = 2;
static const uint32_t NUNBOX32_REG_REG     = 3;
#endif

static const uint32_t MAX_TYPE_FIELD_VALUE = 7;

static const uint32_t MAX_REG_FIELD_VALUE         = 31;
static const uint32_t ESC_REG_FIELD_INDEX         = 31;
static const uint32_t ESC_REG_FIELD_CONST         = 30;
static const uint32_t ESC_REG_FIELD_FLOAT32_STACK = 29;
static const uint32_t ESC_REG_FIELD_FLOAT32_REG   = 28;
static const uint32_t MIN_REG_FIELD_ESC           = 28;

RValueAllocation
RValueAllocation::read(CompactBufferReader &reader)
{
    uint8_t b = reader.readByte();

    JSValueType type = JSValueType(b & 0x7);
    uint32_t code = b >> 3;

    switch (type) {
      case JSVAL_TYPE_DOUBLE:
        if (code < MIN_REG_FIELD_ESC)
            return Double(FloatRegister::FromCode(code));
        JS_ASSERT(code == ESC_REG_FIELD_INDEX);
        return Typed(type, reader.readSigned());

      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_STRING:
      case JSVAL_TYPE_OBJECT:
      case JSVAL_TYPE_BOOLEAN:
        if (code < MIN_REG_FIELD_ESC)
            return Typed(type, Register::FromCode(code));
        JS_ASSERT(code == ESC_REG_FIELD_INDEX);
        return Typed(type, reader.readSigned());

      case JSVAL_TYPE_NULL:
        if (code == ESC_REG_FIELD_CONST)
            return Null();
        if (code == ESC_REG_FIELD_INDEX)
            return Int32(reader.readSigned());
        if (code == ESC_REG_FIELD_FLOAT32_REG)
            return Float32(FloatRegister::FromCode(reader.readUnsigned()));
        if (code == ESC_REG_FIELD_FLOAT32_STACK)
            return Float32(reader.readSigned());
        return Int32(code);

      case JSVAL_TYPE_UNDEFINED:
        if (code == ESC_REG_FIELD_CONST)
            return Undefined();
        if (code == ESC_REG_FIELD_INDEX)
            return ConstantPool(reader.readUnsigned());
        return ConstantPool(code);

      case JSVAL_TYPE_MAGIC:
      {
        if (code == ESC_REG_FIELD_CONST) {
            uint8_t reg2 = reader.readUnsigned();
            if (reg2 != ESC_REG_FIELD_INDEX)
                return Typed(type, Register::FromCode(reg2));
            return Typed(type, reader.readSigned());
        }

#ifdef JS_NUNBOX32
        int32_t type, payload;
        switch (code) {
          case NUNBOX32_STACK_STACK:
            type = reader.readSigned();
            payload = reader.readSigned();
            return Untyped(type, payload);
          case NUNBOX32_STACK_REG:
            type = reader.readSigned();
            payload = reader.readByte();
            return Untyped(type, Register::FromCode(payload));
          case NUNBOX32_REG_STACK:
            type = reader.readByte();
            payload = reader.readSigned();
            return Untyped(Register::FromCode(type), payload);
          case NUNBOX32_REG_REG:
            type = reader.readByte();
            payload = reader.readByte();
            return Untyped(Register::FromCode(type),
                               Register::FromCode(payload));
          default:
            MOZ_ASSUME_UNREACHABLE("bad code");
            break;
        }
#elif JS_PUNBOX64
        if (code < MIN_REG_FIELD_ESC)
            return Untyped(Register::FromCode(code));
         JS_ASSERT(code == ESC_REG_FIELD_INDEX);
         return Untyped(reader.readSigned());
#endif
      }

      default:
        MOZ_ASSUME_UNREACHABLE("bad type");
        break;
    }

    MOZ_ASSUME_UNREACHABLE("huh?");
}

void
RValueAllocation::writeHeader(CompactBufferWriter &writer,
                              JSValueType type,
                              uint32_t regCode) const
{
    JS_ASSERT(uint32_t(type) <= MAX_TYPE_FIELD_VALUE);
    JS_ASSERT(uint32_t(regCode) <= MAX_REG_FIELD_VALUE);
    JS_STATIC_ASSERT(Registers::Total < MIN_REG_FIELD_ESC);

    uint8_t byte = uint32_t(type) | (regCode << 3);
    writer.writeByte(byte);
}

void
RValueAllocation::write(CompactBufferWriter &writer) const
{
    switch (mode()) {
      case CONSTANT: {
        if (constantIndex() < MIN_REG_FIELD_ESC) {
            writeHeader(writer, JSVAL_TYPE_UNDEFINED, constantIndex());
        } else {
            writeHeader(writer, JSVAL_TYPE_UNDEFINED, ESC_REG_FIELD_INDEX);
            writer.writeUnsigned(constantIndex());
        }
        break;
      }

      case DOUBLE_REG: {
        writeHeader(writer, JSVAL_TYPE_DOUBLE, floatReg().code());
        break;
      }

      case FLOAT32_REG: {
        writeHeader(writer, JSVAL_TYPE_NULL, ESC_REG_FIELD_FLOAT32_REG);
        writer.writeUnsigned(floatReg().code());
        break;
      }

      case FLOAT32_STACK: {
        writeHeader(writer, JSVAL_TYPE_NULL, ESC_REG_FIELD_FLOAT32_STACK);
        writer.writeSigned(stackOffset());
        break;
      }

      case TYPED_REG: {
        writeHeader(writer, knownType(), reg().code());
        break;
      }
      case TYPED_STACK: {
        writeHeader(writer, knownType(), ESC_REG_FIELD_INDEX);
        writer.writeSigned(stackOffset());
        break;
      }
#if defined(JS_NUNBOX32)
      case UNTYPED_REG_REG: {
        writeHeader(writer, JSVAL_TYPE_MAGIC, NUNBOX32_REG_REG);
        writer.writeByte(type().reg().code());
        writer.writeByte(payload().reg().code());
        break;
      }
      case UNTYPED_REG_STACK: {
        writeHeader(writer, JSVAL_TYPE_MAGIC, NUNBOX32_REG_STACK);
        writer.writeByte(type().reg().code());
        writer.writeSigned(payload().stackOffset());
        break;
      }
      case UNTYPED_STACK_REG: {
        writeHeader(writer, JSVAL_TYPE_MAGIC, NUNBOX32_STACK_REG);
        writer.writeSigned(type().stackOffset());
        writer.writeByte(payload().reg().code());
        break;
      }
      case UNTYPED_STACK_STACK: {
        writeHeader(writer, JSVAL_TYPE_MAGIC, NUNBOX32_STACK_STACK);
        writer.writeSigned(type().stackOffset());
        writer.writeSigned(payload().stackOffset());
        break;
      }
#elif defined(JS_PUNBOX64)
      case UNTYPED_REG: {
        writeHeader(writer, JSVAL_TYPE_MAGIC, value().reg().code());
        break;
      }
      case UNTYPED_STACK: {
        writeHeader(writer, JSVAL_TYPE_MAGIC, ESC_REG_FIELD_INDEX);
        writer.writeSigned(value().stackOffset());
        break;
      }
#endif
      case JS_UNDEFINED: {
        writeHeader(writer, JSVAL_TYPE_UNDEFINED, ESC_REG_FIELD_CONST);
        break;
      }
      case JS_NULL: {
        writeHeader(writer, JSVAL_TYPE_NULL, ESC_REG_FIELD_CONST);
        break;
      }
      case JS_INT32: {
        if (int32Value() >= 0 && uint32_t(int32Value()) < MIN_REG_FIELD_ESC) {
            writeHeader(writer, JSVAL_TYPE_NULL, int32Value());
        } else {
            writeHeader(writer, JSVAL_TYPE_NULL, ESC_REG_FIELD_INDEX);
            writer.writeSigned(int32Value());
        }
        break;
      }
      case INVALID: {
        MOZ_ASSUME_UNREACHABLE("not initialized");
        break;
      }
    }
}

HashNumber
RValueAllocation::hash() const {
    CompactBufferWriter writer;
    write(writer);

    // We should never oom because the compact buffer writer has 32 inlined
    // bytes, and in the worse case scenario, only encode 11 bytes (= header
    // + signed + signed).
    MOZ_ASSERT(!writer.oom());
    MOZ_ASSERT(writer.length() <= 11);

    HashNumber res = 0;
    for (size_t i = 0; i < writer.length(); i++) {
        res = ((res << 8) | (res >> (sizeof(res) - 1)));
        res ^= writer.buffer()[i];
    }
    return res;
}

void
Location::dump(FILE *fp) const
{
    if (isStackOffset())
        fprintf(fp, "stack %d", stackOffset());
    else
        fprintf(fp, "reg %s", reg().name());
}

static const char *
ValTypeToString(JSValueType type)
{
    switch (type) {
      case JSVAL_TYPE_INT32:
        return "int32_t";
      case JSVAL_TYPE_DOUBLE:
        return "double";
      case JSVAL_TYPE_STRING:
        return "string";
      case JSVAL_TYPE_BOOLEAN:
        return "boolean";
      case JSVAL_TYPE_OBJECT:
        return "object";
      case JSVAL_TYPE_MAGIC:
        return "magic";
      default:
        MOZ_ASSUME_UNREACHABLE("no payload");
    }
}

void
RValueAllocation::dump(FILE *fp) const
{
    switch (mode()) {
      case CONSTANT:
        fprintf(fp, "constant (pool index %u)", constantIndex());
        break;

      case DOUBLE_REG:
        fprintf(fp, "double (reg %s)", floatReg().name());
        break;

      case FLOAT32_REG:
        fprintf(fp, "float32 (reg %s)", floatReg().name());
        break;

      case FLOAT32_STACK:
        fprintf(fp, "float32 (");
        known_type_.payload.dump(fp);
        fprintf(fp, ")");
        break;

      case TYPED_REG:
      case TYPED_STACK:
        fprintf(fp, "%s (", ValTypeToString(knownType()));
        known_type_.payload.dump(fp);
        fprintf(fp, ")");
        break;

#if defined(JS_NUNBOX32)
      case UNTYPED_REG_REG:
      case UNTYPED_REG_STACK:
      case UNTYPED_STACK_REG:
      case UNTYPED_STACK_STACK:
        fprintf(fp, "value (type = ");
        type().dump(fp);
        fprintf(fp, ", payload = ");
        payload().dump(fp);
        fprintf(fp, ")");
        break;
#elif defined(JS_PUNBOX64)
      case UNTYPED_REG:
      case UNTYPED_STACK:
        fprintf(fp, "value (");
        value().dump(fp);
        fprintf(fp, ")");
        break;
#endif

      case JS_UNDEFINED:
        fprintf(fp, "undefined");
        break;

      case JS_NULL:
        fprintf(fp, "null");
        break;

      case JS_INT32:
        fprintf(fp, "int32_t %d", int32Value());
        break;

      case INVALID:
        fprintf(fp, "invalid");
        break;
    }
}

SnapshotReader::SnapshotReader(const uint8_t *snapshots, uint32_t offset,
                               uint32_t RVATableSize, uint32_t listSize)
  : reader_(snapshots + offset, snapshots + listSize),
    allocReader_(snapshots + listSize, snapshots + listSize + RVATableSize),
    allocTable_(snapshots + listSize),
    allocCount_(0),
    frameCount_(0),
    allocRead_(0)
{
    if (!snapshots)
        return;
    IonSpew(IonSpew_Snapshots, "Creating snapshot reader");
    readSnapshotHeader();
    nextFrame();
}

static const uint32_t BAILOUT_KIND_SHIFT = 0;
static const uint32_t BAILOUT_KIND_MASK = (1 << BAILOUT_KIND_BITS) - 1;
static const uint32_t BAILOUT_RESUME_SHIFT = BAILOUT_KIND_SHIFT + BAILOUT_KIND_BITS;
static const uint32_t BAILOUT_FRAMECOUNT_SHIFT = BAILOUT_KIND_BITS + BAILOUT_RESUME_BITS;
static const uint32_t BAILOUT_FRAMECOUNT_BITS = (8 * sizeof(uint32_t)) - BAILOUT_FRAMECOUNT_SHIFT;

void
SnapshotReader::readSnapshotHeader()
{
    uint32_t bits = reader_.readUnsigned();
    frameCount_ = bits >> BAILOUT_FRAMECOUNT_SHIFT;
    JS_ASSERT(frameCount_ > 0);
    bailoutKind_ = BailoutKind((bits >> BAILOUT_KIND_SHIFT) & BAILOUT_KIND_MASK);
    resumeAfter_ = !!(bits & (1 << BAILOUT_RESUME_SHIFT));
    framesRead_ = 0;

    IonSpew(IonSpew_Snapshots, "Read snapshot header with frameCount %u, bailout kind %u (ra: %d)",
            frameCount_, bailoutKind_, resumeAfter_);
}

void
SnapshotReader::readFrameHeader()
{
    JS_ASSERT(moreFrames());
    JS_ASSERT(allocRead_ == allocCount_);

    pcOffset_ = reader_.readUnsigned();
    allocCount_ = reader_.readUnsigned();
#ifdef TRACK_SNAPSHOTS
    pcOpcode_  = reader_.readUnsigned();
    mirOpcode_ = reader_.readUnsigned();
    mirId_     = reader_.readUnsigned();
    lirOpcode_ = reader_.readUnsigned();
    lirId_     = reader_.readUnsigned();
#endif
    IonSpew(IonSpew_Snapshots, "Read pc offset %u, nslots %u", pcOffset_, allocCount_);

    framesRead_++;
    allocRead_ = 0;
}

#ifdef TRACK_SNAPSHOTS
void
SnapshotReader::spewBailingFrom() const
{
    if (IonSpewEnabled(IonSpew_Bailouts)) {
        IonSpewHeader(IonSpew_Bailouts);
        fprintf(IonSpewFile, " bailing from bytecode: %s, MIR: ", js_CodeName[pcOpcode_]);
        MDefinition::PrintOpcodeName(IonSpewFile, MDefinition::Opcode(mirOpcode_));
        fprintf(IonSpewFile, " [%u], LIR: ", mirId_);
        LInstruction::printName(IonSpewFile, LInstruction::Opcode(lirOpcode_));
        fprintf(IonSpewFile, " [%u]", lirId_);
        fprintf(IonSpewFile, "\n");
    }
}
#endif

// Pad serialized RValueAllocations by a multiple of X bytes in the allocation
// buffer.  By padding serialized value allocations, we are building an
// indexable table of elements of X bytes, and thus we can safely divide any
// offset within the buffer by X to obtain an index.
//
// By padding, we are loosing space within the allocation buffer, but we
// multiple by X the number of indexes that we can store on one byte in each
// snapshots.
//
// Some value allocations are taking more than X bytes to be encoded, in which
// case we will pad to a multiple of X, and we are wasting indexes. The choice
// of X should be balanced between the wasted padding of serialized value
// allocation, and the saving made in snapshot indexes.
static const size_t ALLOCATION_TABLE_ALIGNMENT = 2; /* bytes */

RValueAllocation
SnapshotReader::readAllocation()
{
    JS_ASSERT(allocRead_ < allocCount_);
    IonSpew(IonSpew_Snapshots, "Reading slot %u", allocRead_);
    allocRead_++;

    uint32_t offset = reader_.readUnsigned() * ALLOCATION_TABLE_ALIGNMENT;
    allocReader_.seek(allocTable_, offset);
    return RValueAllocation::read(allocReader_);
}

bool
SnapshotWriter::init()
{
    // Based on the measurements made in Bug 962555 comment 20, this should be
    // enough to prevent the reallocation of the hash table for at least half of
    // the compilations.
    return allocMap_.init(32);
}

SnapshotOffset
SnapshotWriter::startSnapshot(uint32_t frameCount, BailoutKind kind, bool resumeAfter)
{
    nframes_ = frameCount;
    framesWritten_ = 0;

    lastStart_ = writer_.length();

    IonSpew(IonSpew_Snapshots, "starting snapshot with frameCount %u, bailout kind %u",
            frameCount, kind);
    JS_ASSERT(frameCount > 0);
    JS_ASSERT(frameCount < (1 << BAILOUT_FRAMECOUNT_BITS));
    JS_ASSERT(uint32_t(kind) < (1 << BAILOUT_KIND_BITS));

    uint32_t bits = (uint32_t(kind) << BAILOUT_KIND_SHIFT) |
                  (frameCount << BAILOUT_FRAMECOUNT_SHIFT);
    if (resumeAfter)
        bits |= (1 << BAILOUT_RESUME_SHIFT);

    writer_.writeUnsigned(bits);
    return lastStart_;
}

void
SnapshotWriter::startFrame(JSFunction *fun, JSScript *script, jsbytecode *pc, uint32_t exprStack)
{
    // Test if we honor the maximum of arguments at all times.
    // This is a sanity check and not an algorithm limit. So check might be a bit too loose.
    // +4 to account for scope chain, return value, this value and maybe arguments_object.
    JS_ASSERT(CountArgSlots(script, fun) < SNAPSHOT_MAX_NARGS + 4);

    uint32_t implicit = StartArgSlot(script);
    uint32_t formalArgs = CountArgSlots(script, fun);

    nallocs_ = formalArgs + script->nfixed() + exprStack;
    allocWritten_ = 0;

    IonSpew(IonSpew_Snapshots, "Starting frame; implicit %u, formals %u, fixed %u, exprs %u",
            implicit, formalArgs - implicit, script->nfixed(), exprStack);

    uint32_t pcoff = script->pcToOffset(pc);
    IonSpew(IonSpew_Snapshots, "Writing pc offset %u, nslots %u", pcoff, nallocs_);
    writer_.writeUnsigned(pcoff);
    writer_.writeUnsigned(nallocs_);
}

#ifdef TRACK_SNAPSHOTS
void
SnapshotWriter::trackFrame(uint32_t pcOpcode, uint32_t mirOpcode, uint32_t mirId,
                                            uint32_t lirOpcode, uint32_t lirId)
{
    writer_.writeUnsigned(pcOpcode);
    writer_.writeUnsigned(mirOpcode);
    writer_.writeUnsigned(mirId);
    writer_.writeUnsigned(lirOpcode);
    writer_.writeUnsigned(lirId);
}
#endif

bool
SnapshotWriter::add(const RValueAllocation &alloc)
{
    MOZ_ASSERT(allocMap_.initialized());

    uint32_t offset;
    RValueAllocMap::AddPtr p = allocMap_.lookupForAdd(alloc);
    if (!p) {
        // Write 0x7f in all padding bytes.
        while (allocWriter_.length() % ALLOCATION_TABLE_ALIGNMENT)
            allocWriter_.writeByte(0x7f);

        offset = allocWriter_.length();
        JS_ASSERT(offset % ALLOCATION_TABLE_ALIGNMENT == 0);
        alloc.write(allocWriter_);
        if (!allocMap_.add(p, alloc, offset))
            return false;
    } else {
        offset = p->value();
    }

    if (IonSpewEnabled(IonSpew_Snapshots)) {
        IonSpewHeader(IonSpew_Snapshots);
        fprintf(IonSpewFile, "    slot %u (%d): ", allocWritten_, offset);
        alloc.dump(IonSpewFile);
        fprintf(IonSpewFile, "\n");
    }

    allocWritten_++;
    JS_ASSERT(allocWritten_ <= nallocs_);
    writer_.writeUnsigned(offset / ALLOCATION_TABLE_ALIGNMENT);
    return true;
}

void
SnapshotWriter::endFrame()
{
    // Check that the last write succeeded.
    JS_ASSERT(nallocs_ == allocWritten_);
    nallocs_ = allocWritten_ = 0;
    framesWritten_++;
}

void
SnapshotWriter::endSnapshot()
{
    JS_ASSERT(nframes_ == framesWritten_);

    // Place a sentinel for asserting on the other end.
#ifdef DEBUG
    writer_.writeSigned(-1);
#endif
    
    IonSpew(IonSpew_Snapshots, "ending snapshot total size: %u bytes (start %u)",
            uint32_t(writer_.length() - lastStart_), lastStart_);
}
