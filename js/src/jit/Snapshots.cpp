/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsscript.h"

#include "jit/IonSpewer.h"
#ifdef TRACK_SNAPSHOTS
#include "jit/LIR.h"
#include "jit/MIR.h"
#endif
#include "jit/SnapshotReader.h"
#include "jit/SnapshotWriter.h"

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

Slot
SnapshotReader::readSlot()
{
    JS_ASSERT(slotsRead_ < slotCount_);
    IonSpew(IonSpew_Snapshots, "Reading slot %u", slotsRead_);
    slotsRead_++;
    return Slot::read(reader_);
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

    nslots_ = formalArgs + script->nfixed() + exprStack;
    slotsWritten_ = 0;

    IonSpew(IonSpew_Snapshots, "Starting frame; implicit %u, formals %u, fixed %u, exprs %u",
            implicit, formalArgs - implicit, script->nfixed(), exprStack);

    uint32_t pcoff = script->pcToOffset(pc);
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
SnapshotWriter::addSlot(const Slot &slot)
{
    if (IonSpewEnabled(IonSpew_Snapshots)) {
        IonSpewHeader(IonSpew_Snapshots);
        fprintf(IonSpewFile, "    slot %u: ", slotsWritten_);
        slot.dump(IonSpewFile);
        fprintf(IonSpewFile, "\n");
    }

    slotsWritten_++;
    JS_ASSERT(slotsWritten_ <= nslots_);
    slot.write(writer_);
}

void
SnapshotWriter::endFrame()
{
    // Check that the last write succeeded.
    JS_ASSERT(nslots_ == slotsWritten_);
    nslots_ = slotsWritten_ = 0;
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
