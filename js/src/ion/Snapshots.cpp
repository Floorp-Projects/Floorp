/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MIRGenerator.h"
#include "IonFrames.h"
#include "jsscript.h"
#include "IonLinker.h"
#include "IonSpewer.h"
#include "SnapshotReader.h"
#include "SnapshotWriter.h"

#ifdef TRACK_SNAPSHOTS
#include "MIR.h"
#include "LIR.h"
#endif

#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

// Snapshot header:
//
//   [vwu] bits (n-31]: frame count
//         bits [0,n):  bailout kind (n = BAILOUT_KIND_BITS)
//
// Snapshot body, repeated "frame count" times, from oldest frame to newest frame.
// Note that the first frame doesn't have the "parent PC" field.
//
//   [ptr] Debug only: RawScript
//   [vwu] pc offset
//   [vwu] # of slots, including nargs
// [slot*] N slot entries, where N = nargs + nfixed + stackDepth
//
// Encodings:
//   [ptr] A fixed-size pointer.
//   [vwu] A variable-width unsigned integer.
//   [vws] A variable-width signed integer.
//    [u8] An 8-bit unsigned integer.
// [slot*] Information on how to reify a js::Value from an Ion frame. The
//         first byte is split as thus:
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
//              If "reg" is InvalidReg1, this byte is followed by a [vws]
//              stack offset. Otherwise, "reg" encodes a GPR register.
//
//         JSVAL_TYPE_NULL:
//              Reg value:
//                 0-29: Constant integer; Int32Value(n)
//                   30: NullValue()
//                   31: Constant integer; Int32Value([vws])
//
//         JSVAL_TYPE_UNDEFINED:
//              Reg value:
//                 0-29: Constant value, index n into ionScript->constants()
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
//                  "reg" is InvalidReg1: byte is followed by a [vws] stack
//                  offset containing a Value.
//
//                  Otherwise, "reg" is a register containing a Value.
//        

SnapshotReader::SnapshotReader(const uint8_t *buffer, const uint8_t *end)
  : reader_(buffer, end),
    slotCount_(0),
    frameCount_(0),
    slotsRead_(0)
{
    if (!buffer)
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
    JS_ASSERT(slotsRead_ == slotCount_);

    pcOffset_ = reader_.readUnsigned();
    slotCount_ = reader_.readUnsigned();
    IonSpew(IonSpew_Snapshots, "Read pc offset %u, nslots %u", pcOffset_, slotCount_);

#ifdef TRACK_SNAPSHOTS
    pcOpcode_  = reader_.readUnsigned();
    mirOpcode_ = reader_.readUnsigned();
    mirId_     = reader_.readUnsigned();
    lirOpcode_ = reader_.readUnsigned();
    lirId_     = reader_.readUnsigned();
#endif

    framesRead_++;
    slotsRead_ = 0;
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

#ifdef JS_NUNBOX32
static const uint32_t NUNBOX32_STACK_STACK = 0;
static const uint32_t NUNBOX32_STACK_REG   = 1;
static const uint32_t NUNBOX32_REG_STACK   = 2;
static const uint32_t NUNBOX32_REG_REG     = 3;
#endif

static const uint32_t MAX_TYPE_FIELD_VALUE = 7;

static const uint32_t MAX_REG_FIELD_VALUE  = 31;
static const uint32_t ESC_REG_FIELD_INDEX  = 31;
static const uint32_t ESC_REG_FIELD_CONST  = 30;
static const uint32_t MIN_REG_FIELD_ESC    = 30;

SnapshotReader::Slot
SnapshotReader::readSlot()
{
    JS_ASSERT(slotsRead_ < slotCount_);
    IonSpew(IonSpew_Snapshots, "Reading slot %u", slotsRead_);
    slotsRead_++;

    uint8_t b = reader_.readByte();

    JSValueType type = JSValueType(b & 0x7);
    uint32_t code = b >> 3;

    switch (type) {
      case JSVAL_TYPE_DOUBLE:
        if (code < MIN_REG_FIELD_ESC)
            return Slot(FloatRegister::FromCode(code));
        JS_ASSERT(code == ESC_REG_FIELD_INDEX);
        return Slot(TYPED_STACK, type, Location::From(reader_.readSigned()));

      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_STRING:
      case JSVAL_TYPE_OBJECT:
      case JSVAL_TYPE_BOOLEAN:
        if (code < MIN_REG_FIELD_ESC)
            return Slot(TYPED_REG, type, Location::From(Register::FromCode(code)));
        JS_ASSERT(code == ESC_REG_FIELD_INDEX);
        return Slot(TYPED_STACK, type, Location::From(reader_.readSigned()));

      case JSVAL_TYPE_NULL:
        if (code == ESC_REG_FIELD_CONST)
            return Slot(JS_NULL);
        if (code == ESC_REG_FIELD_INDEX)
            return Slot(JS_INT32, reader_.readSigned());
        return Slot(JS_INT32, code);

      case JSVAL_TYPE_UNDEFINED:
        if (code == ESC_REG_FIELD_CONST)
            return Slot(JS_UNDEFINED);
        if (code == ESC_REG_FIELD_INDEX)
            return Slot(CONSTANT, reader_.readUnsigned());
        return Slot(CONSTANT, code);

      default:
      {
        JS_ASSERT(type == JSVAL_TYPE_MAGIC);

        if (code == ESC_REG_FIELD_CONST) {
            uint8_t reg2 = reader_.readUnsigned();
            Location loc;
            if (reg2 != ESC_REG_FIELD_INDEX)
                loc = Location::From(Register::FromCode(reg2));
            else
                loc = Location::From(reader_.readSigned());
            return Slot(TYPED_REG, type, loc);
        }

        Slot slot(UNTYPED);
#ifdef JS_NUNBOX32
        switch (code) {
          case NUNBOX32_STACK_STACK:
            slot.unknown_type_.type = Location::From(reader_.readSigned());
            slot.unknown_type_.payload = Location::From(reader_.readSigned());
            return slot;
          case NUNBOX32_STACK_REG:
            slot.unknown_type_.type = Location::From(reader_.readSigned());
            slot.unknown_type_.payload = Location::From(Register::FromCode(reader_.readByte()));
            return slot;
          case NUNBOX32_REG_STACK:
            slot.unknown_type_.type = Location::From(Register::FromCode(reader_.readByte()));
            slot.unknown_type_.payload = Location::From(reader_.readSigned());
            return slot;
          default:
            JS_ASSERT(code == NUNBOX32_REG_REG);
            slot.unknown_type_.type = Location::From(Register::FromCode(reader_.readByte()));
            slot.unknown_type_.payload = Location::From(Register::FromCode(reader_.readByte()));
            return slot;
        }
#elif JS_PUNBOX64
        if (code < MIN_REG_FIELD_ESC) {
            slot.unknown_type_.value = Location::From(Register::FromCode(code));
        } else {
            JS_ASSERT(code == ESC_REG_FIELD_INDEX);
            slot.unknown_type_.value = Location::From(reader_.readSigned());
        }
        return slot;
#endif
      }
    }

    JS_NOT_REACHED("huh?");
    return Slot(JS_UNDEFINED);
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
SnapshotWriter::startFrame(JSFunction *fun, RawScript script, jsbytecode *pc, uint32_t exprStack)
{
    JS_ASSERT(CountArgSlots(script, fun) < SNAPSHOT_MAX_NARGS);

    uint32_t implicit = StartArgSlot(script, fun);
    uint32_t formalArgs = CountArgSlots(script, fun);

    nslots_ = formalArgs + script->nfixed + exprStack;
    slotsWritten_ = 0;

    IonSpew(IonSpew_Snapshots, "Starting frame; implicit %u, formals %u, fixed %u, exprs %u",
            implicit, formalArgs - implicit, script->nfixed, exprStack);

    JS_ASSERT(script->code <= pc && pc <= script->code + script->length);

    uint32_t pcoff = uint32_t(pc - script->code);
    IonSpew(IonSpew_Snapshots, "Writing pc offset %u, nslots %u", pcoff, nslots_);
    writer_.writeUnsigned(pcoff);
    writer_.writeUnsigned(nslots_);
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

void
SnapshotWriter::endFrame()
{
    // Check that the last write succeeded.
    JS_ASSERT(nslots_ == slotsWritten_);
    nslots_ = slotsWritten_ = 0;
    framesWritten_++;
}

void
SnapshotWriter::writeSlotHeader(JSValueType type, uint32_t regCode)
{
    JS_ASSERT(uint32_t(type) <= MAX_TYPE_FIELD_VALUE);
    JS_ASSERT(uint32_t(regCode) <= MAX_REG_FIELD_VALUE);
    JS_STATIC_ASSERT(Registers::Total < MIN_REG_FIELD_ESC);

    uint8_t byte = uint32_t(type) | (regCode << 3);
    writer_.writeByte(byte);

    slotsWritten_++;
    JS_ASSERT(slotsWritten_ <= nslots_);
}

void
SnapshotWriter::addSlot(const FloatRegister &reg)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: double (reg %s)", slotsWritten_, reg.name());

    writeSlotHeader(JSVAL_TYPE_DOUBLE, reg.code());
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
        JS_NOT_REACHED("no payload");
        return "";
    }
}

void
SnapshotWriter::addSlot(JSValueType type, const Register &reg)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: %s (%s)",
            slotsWritten_, ValTypeToString(type), reg.name());

    JS_ASSERT(type != JSVAL_TYPE_DOUBLE);
    writeSlotHeader(type, reg.code());
}

void
SnapshotWriter::addSlot(JSValueType type, int32_t stackIndex)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: %s (stack %d)",
            slotsWritten_, ValTypeToString(type), stackIndex);

    writeSlotHeader(type, ESC_REG_FIELD_INDEX);
    writer_.writeSigned(stackIndex);
}

#if defined(JS_NUNBOX32)
void
SnapshotWriter::addSlot(const Register &type, const Register &payload)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (t=%s, d=%s)",
            slotsWritten_, type.name(), payload.name());

    writeSlotHeader(JSVAL_TYPE_MAGIC, NUNBOX32_REG_REG);
    writer_.writeByte(type.code());
    writer_.writeByte(payload.code());
}

void
SnapshotWriter::addSlot(const Register &type, int32_t payloadStackIndex)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (t=%s, d=%d)",
            slotsWritten_, type.name(), payloadStackIndex);

    writeSlotHeader(JSVAL_TYPE_MAGIC, NUNBOX32_REG_STACK);
    writer_.writeByte(type.code());
    writer_.writeSigned(payloadStackIndex);
}

void
SnapshotWriter::addSlot(int32_t typeStackIndex, const Register &payload)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (t=%d, d=%s)",
            slotsWritten_, typeStackIndex, payload.name());

    writeSlotHeader(JSVAL_TYPE_MAGIC, NUNBOX32_STACK_REG);
    writer_.writeSigned(typeStackIndex);
    writer_.writeByte(payload.code());
}

void
SnapshotWriter::addSlot(int32_t typeStackIndex, int32_t payloadStackIndex)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (t=%d, d=%d)",
            slotsWritten_, typeStackIndex, payloadStackIndex);

    writeSlotHeader(JSVAL_TYPE_MAGIC, NUNBOX32_STACK_STACK);
    writer_.writeSigned(typeStackIndex);
    writer_.writeSigned(payloadStackIndex);
}

#elif defined(JS_PUNBOX64)
void
SnapshotWriter::addSlot(const Register &value)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (reg %s)", slotsWritten_, value.name());

    writeSlotHeader(JSVAL_TYPE_MAGIC, value.code());
}

void
SnapshotWriter::addSlot(int32_t valueStackSlot)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (stack %d)", slotsWritten_, valueStackSlot);

    writeSlotHeader(JSVAL_TYPE_MAGIC, ESC_REG_FIELD_INDEX);
    writer_.writeSigned(valueStackSlot);
}
#endif

void
SnapshotWriter::addUndefinedSlot()
{
    IonSpew(IonSpew_Snapshots, "    slot %u: undefined", slotsWritten_);

    writeSlotHeader(JSVAL_TYPE_UNDEFINED, ESC_REG_FIELD_CONST);
}

void
SnapshotWriter::addNullSlot()
{
    IonSpew(IonSpew_Snapshots, "    slot %u: null", slotsWritten_);

    writeSlotHeader(JSVAL_TYPE_NULL, ESC_REG_FIELD_CONST);
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

void
SnapshotWriter::addInt32Slot(int32_t value)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: int32_t %d", slotsWritten_, value);

    if (value >= 0 && uint32_t(value) < MIN_REG_FIELD_ESC) {
        writeSlotHeader(JSVAL_TYPE_NULL, value);
    } else {
        writeSlotHeader(JSVAL_TYPE_NULL, ESC_REG_FIELD_INDEX);
        writer_.writeSigned(value);
    }
}

void
SnapshotWriter::addConstantPoolSlot(uint32_t index)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: constant pool index %u", slotsWritten_, index);

    if (index < MIN_REG_FIELD_ESC) {
        writeSlotHeader(JSVAL_TYPE_UNDEFINED, index);
    } else {
        writeSlotHeader(JSVAL_TYPE_UNDEFINED, ESC_REG_FIELD_INDEX);
        writer_.writeUnsigned(index);
    }
}

