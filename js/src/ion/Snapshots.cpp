/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "MIRGenerator.h"
#include "Snapshots.h"
#include "IonFrames.h"
#include "jsscript.h"
#include "IonLinker.h"
#include "IonSpewer.h"

#ifdef TRACK_SNAPSHOTS
#include "MIR.h"
#include "LIR.h"
#endif

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
//   [ptr] Debug only: JSScript *
//   [vwu] parent pc offset (inline frames only!)
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

SnapshotReader::SnapshotReader(const uint8 *buffer, const uint8 *end)
  : reader_(buffer, end)
{
    IonSpew(IonSpew_Snapshots, "Creating snapshot reader");
    readSnapshotHeader();
    readSnapshotBody();
}

void
SnapshotReader::readSnapshotHeader()
{
    uint32 bits = reader_.readUnsigned();
    frameCount_ = bits >> BAILOUT_KIND_BITS;
    JS_ASSERT(frameCount_ > 0);
    bailoutKind_ = (BailoutKind)(bits & ((1 << BAILOUT_KIND_BITS) - 1));

    IonSpew(IonSpew_Snapshots, "Read snapshot header with frameCount %u, bailout kind %u",
            frameCount_, bailoutKind_);
}

void
SnapshotReader::readSnapshotBody()
{
#ifdef DEBUG
    union {
        JSScript *script;
        uint8 bytes[sizeof(JSScript *)];
    } u;
    for (size_t i = 0; i < sizeof(JSScript *); i++)
        u.bytes[i] = reader_.readByte();
    script_ = u.script;
    slotsRead_ = 0;
#endif

    pcOffset_ = reader_.readUnsigned();
    slotCount_ = reader_.readUnsigned();
    IonSpew(IonSpew_Snapshots, "Read pc offset %u, nslots %u", pcOffset_, slotCount_);

#ifdef TRACK_SNAPSHOTS
    pcOpcode_ = reader_.readUnsigned();
    mirOpcode_ = reader_.readUnsigned();
    lirOpcode_ = reader_.readUnsigned();
    if (IonSpewEnabled(IonSpew_Snapshots)) {
        IonSpewHeader(IonSpew_Snapshots);
        fprintf(IonSpewFile, "Involved Opcodes, Bytecode: %s, MIR: ", js_CodeName[pcOpcode_]);
        MDefinition::PrintOpcodeName(IonSpewFile, MDefinition::Opcode(mirOpcode_));
        fprintf(IonSpewFile, ", LIR: ");
        LInstruction::printName(IonSpewFile, LInstruction::Opcode(lirOpcode_));
        fprintf(IonSpewFile, "\n");
    }
#endif
}

#ifdef JS_NUNBOX32
static const uint32 NUNBOX32_STACK_STACK = 0;
static const uint32 NUNBOX32_STACK_REG   = 1;
static const uint32 NUNBOX32_REG_STACK   = 2;
static const uint32 NUNBOX32_REG_REG     = 3;
#endif

static const uint32 MAX_TYPE_FIELD_VALUE = 7;
static const uint32 MAX_REG_FIELD_VALUE  = 31;

// Indicates null or undefined.
static const uint32 SINGLETON_VALUE      = 30;

SnapshotReader::Slot
SnapshotReader::readSlot()
{
    JS_ASSERT(slotsRead_ < slotCount_);
#ifdef DEBUG
    IonSpew(IonSpew_Snapshots, "Reading slot %u", slotsRead_);
    slotsRead_++;
#endif

    uint8 b = reader_.readByte();

    JSValueType type = JSValueType(b & 0x7);
    uint32 code = b >> 3;

    switch (type) {
      case JSVAL_TYPE_DOUBLE:
        if (code != FloatRegisters::Invalid)
            return Slot(FloatRegister::FromCode(code));
        return Slot(TYPED_STACK, type, Location::From(reader_.readSigned()));

      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_STRING:
      case JSVAL_TYPE_OBJECT:
      case JSVAL_TYPE_BOOLEAN:
        if (code != Registers::Invalid)
            return Slot(TYPED_REG, type, Location::From(Register::FromCode(code)));
        return Slot(TYPED_STACK, type, Location::From(reader_.readSigned()));

      case JSVAL_TYPE_NULL:
        if (code == SINGLETON_VALUE)
            return Slot(JS_NULL);
        if (code == MAX_REG_FIELD_VALUE)
            return Slot(JS_INT32, reader_.readSigned());
        return Slot(JS_INT32, code);

      case JSVAL_TYPE_UNDEFINED:
        if (code == SINGLETON_VALUE)
            return Slot(JS_UNDEFINED);
        if (code == MAX_REG_FIELD_VALUE)
            return Slot(CONSTANT, reader_.readUnsigned());
        return Slot(CONSTANT, code);

      default:
      {
        JS_ASSERT(type == JSVAL_TYPE_MAGIC);
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
        if (code != Registers::Invalid)
            slot.unknown_type_.value = Location::From(Register::FromCode(code));
        else
            slot.unknown_type_.value = Location::From(reader_.readSigned());
        return slot;
#endif
      }
    }

    JS_NOT_REACHED("huh?");
    return Slot(JS_UNDEFINED);
}

void
SnapshotReader::finishReadingFrame()
{
    JS_ASSERT(slotsRead_ == slotCount_);
    JS_ASSERT_IF(!remainingFrameCount(), reader_.readSigned() == -1);

    JS_ASSERT(frameCount_ > 0);
    frameCount_ -= 1;

    JS_ASSERT_IF(!frameCount_, reader_.readSigned() == -1);

    IonSpew(IonSpew_Snapshots, "Finished reading frame, %u remaining", frameCount_);

    if (frameCount_)
        readSnapshotBody();
}

bool
SnapshotReader::Slot::liveInReg() const
{
    switch (mode()) {
      case SnapshotReader::DOUBLE_REG:
      case SnapshotReader::TYPED_REG:
        return true;

      case SnapshotReader::TYPED_STACK:
      case SnapshotReader::JS_UNDEFINED:
      case SnapshotReader::JS_NULL:
      case SnapshotReader::JS_INT32:
      case SnapshotReader::CONSTANT:
        return false;

      case SnapshotReader::UNTYPED:
#if defined(JS_NUNBOX32)
        return !(type().isStackSlot() || payload().isStackSlot());
#elif defined(JS_PUNBOX64)
        return !value().isStackSlot();
#endif

      default:
        JS_NOT_REACHED("huh?");
        return false;
    }
}

SnapshotIterator::SnapshotIterator(const FrameRecovery &in)
  : in_(in),
    reader_(),
    unreadSlots_(0)
{
    JS_ASSERT(in.snapshotOffset() < in.ionScript()->snapshotsSize());
    const uint8 *start = in.ionScript()->snapshots() + in.snapshotOffset();
    const uint8 *end = in.ionScript()->snapshots() + in.ionScript()->snapshotsSize();
    reader_.construct(start, end);
    readFrame();
}

uintptr_t
SnapshotIterator::fromLocation(const SnapshotReader::Location &loc)
{
    if (loc.isStackSlot())
        return in_.readSlot(loc.stackSlot());
    return in_.machine().readReg(loc.reg());
}

void
SnapshotIterator::skipLocation(const SnapshotReader::Location &loc)
{
    if (loc.isStackSlot())
        in_.readSlot(loc.stackSlot());
}

Value
SnapshotIterator::FromTypedPayload(JSValueType type, uintptr_t payload)
{
    switch (type) {
      case JSVAL_TYPE_INT32:
        return Int32Value(payload);
      case JSVAL_TYPE_BOOLEAN:
        return BooleanValue(!!payload);
      case JSVAL_TYPE_STRING:
        return StringValue(reinterpret_cast<JSString *>(payload));
      case JSVAL_TYPE_OBJECT:
        return ObjectValue(*reinterpret_cast<JSObject *>(payload));
      default:
        JS_NOT_REACHED("unexpected type - needs payload");
        return UndefinedValue();
    }
}

SnapshotIterator::Slot
SnapshotIterator::readSlot()
{
    unreadSlots_--;
    return reader_.ref().readSlot();
}

Value
SnapshotIterator::slotValue(const Slot &slot)
{
    switch (slot.mode()) {
      case SnapshotReader::DOUBLE_REG:
        return DoubleValue(in_.machine().readFloatReg(slot.floatReg()));

      case SnapshotReader::TYPED_REG:
        return FromTypedPayload(slot.knownType(), in_.machine().readReg(slot.reg()));

      case SnapshotReader::TYPED_STACK:
      {
        JSValueType type = slot.knownType();
        if (type == JSVAL_TYPE_DOUBLE)
            return DoubleValue(in_.readDoubleSlot(slot.stackSlot()));
        return FromTypedPayload(type, in_.readSlot(slot.stackSlot()));
      }

      case SnapshotReader::UNTYPED:
      {
          jsval_layout layout;
#if defined(JS_NUNBOX32)
          layout.s.tag = (JSValueTag)fromLocation(slot.type());
          layout.s.payload.word = fromLocation(slot.payload());
#elif defined(JS_PUNBOX64)
          layout.asBits = fromLocation(slot.value());
#endif
          return IMPL_TO_JSVAL(layout);
      }

      case SnapshotReader::JS_UNDEFINED:
        return UndefinedValue();

      case SnapshotReader::JS_NULL:
        return NullValue();

      case SnapshotReader::JS_INT32:
        return Int32Value(slot.int32Value());

      case SnapshotReader::CONSTANT:
        return in_.ionScript()->getConstant(slot.constantIndex());

      default:
        JS_NOT_REACHED("huh?");
        return UndefinedValue();
    }
}

void
SnapshotIterator::skip(const Slot &slot)
{
    switch (slot.mode()) {
      case SnapshotReader::DOUBLE_REG:
      case SnapshotReader::TYPED_REG:
      case SnapshotReader::JS_UNDEFINED:
      case SnapshotReader::JS_NULL:
      case SnapshotReader::JS_INT32:
      case SnapshotReader::CONSTANT:
        return;

      case SnapshotReader::TYPED_STACK:
      {
        JSValueType type = slot.knownType();
        if (type == JSVAL_TYPE_DOUBLE)
            in_.readDoubleSlot(slot.stackSlot());
        in_.readSlot(slot.stackSlot());
        return;
      }

      case SnapshotReader::UNTYPED:
      {
#if defined(JS_NUNBOX32)
        skipLocation(slot.type());
        skipLocation(slot.payload());
#elif defined(JS_PUNBOX64)
        skipLocation(slot.value());
#endif
        return;
      }

      default:
        JS_NOT_REACHED("huh?");
        return;
    }
}

SnapshotOffset
SnapshotWriter::startSnapshot(uint32 frameCount, BailoutKind kind)
{
    nframes_ = frameCount;
    framesWritten_ = 0;

    lastStart_ = writer_.length();

    IonSpew(IonSpew_Snapshots, "starting snapshot with frameCount %u, bailout kind %u",
            frameCount, kind);
    JS_ASSERT(frameCount > 0);
    JS_ASSERT(((frameCount << BAILOUT_KIND_BITS) >> BAILOUT_KIND_BITS) == frameCount);
    JS_ASSERT((1 << BAILOUT_KIND_BITS) > uint32(kind));
    writer_.writeUnsigned((frameCount << BAILOUT_KIND_BITS) | uint32(kind));

    return lastStart_;
}

void
SnapshotWriter::startFrame(JSFunction *fun, JSScript *script, jsbytecode *pc, uint32 exprStack)
{
    JS_ASSERT(CountArgSlots(fun) < SNAPSHOT_MAX_NARGS);
    JS_ASSERT(exprStack < SNAPSHOT_MAX_STACK);

    uint32 formalArgs = CountArgSlots(fun);

    nslots_ = formalArgs + script->nfixed + exprStack;
    slotsWritten_ = 0;

    IonSpew(IonSpew_Snapshots, "Starting frame; formals %u, fixed %u, exprs %u",
            formalArgs, script->nfixed, exprStack);

#ifdef DEBUG
    union {
        JSScript *script;
        uint8 bytes[sizeof(JSScript *)];
    } u;
    u.script = script;
    for (size_t i = 0; i < sizeof(JSScript *); i++)
        writer_.writeByte(u.bytes[i]);
#endif

    JS_ASSERT(script->code <= pc && pc <= script->code + script->length);

    uint32 pcoff = uint32(pc - script->code);
    IonSpew(IonSpew_Snapshots, "Writing pc offset %u, nslots %u", pcoff, nslots_);
    writer_.writeUnsigned(pcoff);
    writer_.writeUnsigned(nslots_);
}

#ifdef TRACK_SNAPSHOTS
void
SnapshotWriter::trackFrame(uint32 pcOpcode, uint32 mirOpcode, uint32 lirOpcode)
{
    writer_.writeUnsigned(pcOpcode);
    writer_.writeUnsigned(mirOpcode);
    writer_.writeUnsigned(lirOpcode);
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
SnapshotWriter::writeSlotHeader(JSValueType type, uint32 regCode)
{
    JS_ASSERT(uint32(type) <= MAX_TYPE_FIELD_VALUE);
    JS_ASSERT(uint32(regCode) <= MAX_REG_FIELD_VALUE);

    uint8 byte = uint32(type) | (regCode << 3);
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
        return "int32";
      case JSVAL_TYPE_DOUBLE:
        return "double";
      case JSVAL_TYPE_STRING:
        return "string";
      case JSVAL_TYPE_BOOLEAN:
        return "boolean";
      case JSVAL_TYPE_OBJECT:
        return "object";
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
SnapshotWriter::addSlot(JSValueType type, int32 stackIndex)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: %s (stack %d)",
            slotsWritten_, ValTypeToString(type), stackIndex);

    if (type == JSVAL_TYPE_DOUBLE)
        writeSlotHeader(type, FloatRegisters::Invalid);
    else
        writeSlotHeader(type, Registers::Invalid);
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
SnapshotWriter::addSlot(const Register &type, int32 payloadStackIndex)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (t=%s, d=%d)",
            slotsWritten_, type.name(), payloadStackIndex);

    writeSlotHeader(JSVAL_TYPE_MAGIC, NUNBOX32_REG_STACK);
    writer_.writeByte(type.code());
    writer_.writeSigned(payloadStackIndex);
}

void
SnapshotWriter::addSlot(int32 typeStackIndex, const Register &payload)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (t=%d, d=%s)",
            slotsWritten_, typeStackIndex, payload.name());

    writeSlotHeader(JSVAL_TYPE_MAGIC, NUNBOX32_STACK_REG);
    writer_.writeSigned(typeStackIndex);
    writer_.writeByte(payload.code());
}

void
SnapshotWriter::addSlot(int32 typeStackIndex, int32 payloadStackIndex)
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
SnapshotWriter::addSlot(int32 valueStackSlot)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: value (stack %d)", slotsWritten_, valueStackSlot);

    writeSlotHeader(JSVAL_TYPE_MAGIC, Registers::Invalid);
    writer_.writeSigned(valueStackSlot);
}
#endif

void
SnapshotWriter::addUndefinedSlot()
{
    IonSpew(IonSpew_Snapshots, "    slot %u: undefined", slotsWritten_);

    writeSlotHeader(JSVAL_TYPE_UNDEFINED, SINGLETON_VALUE);
}

void
SnapshotWriter::addNullSlot()
{
    IonSpew(IonSpew_Snapshots, "    slot %u: null", slotsWritten_);

    writeSlotHeader(JSVAL_TYPE_NULL, SINGLETON_VALUE);
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
            uint32(writer_.length() - lastStart_), lastStart_);
}

void
SnapshotWriter::addInt32Slot(int32 value)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: int32 %d", slotsWritten_, value);

    if (value >= 0 && uint32(value) < SINGLETON_VALUE) {
        writeSlotHeader(JSVAL_TYPE_NULL, value);
    } else {
        writeSlotHeader(JSVAL_TYPE_NULL, MAX_REG_FIELD_VALUE);
        writer_.writeSigned(value);
    }
}

void
SnapshotWriter::addConstantPoolSlot(uint32 index)
{
    IonSpew(IonSpew_Snapshots, "    slot %u: constant pool index %u", slotsWritten_, index);

    if (index < SINGLETON_VALUE) {
        writeSlotHeader(JSVAL_TYPE_UNDEFINED, index);
    } else {
        writeSlotHeader(JSVAL_TYPE_UNDEFINED, MAX_REG_FIELD_VALUE);
        writer_.writeUnsigned(index);
    }
}

