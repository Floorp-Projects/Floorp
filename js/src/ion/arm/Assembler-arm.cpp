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

#include "Assembler-arm.h"
#include "jsgcmark.h"

using namespace js;
using namespace js::ion;
uint32
js::ion::RT(Register r)
{
    return r.code() << 12;
}

uint32
js::ion::RN(Register r)
{
    return r.code() << 16;
}

uint32
js::ion::RD(Register r)
{
    return r.code() << 12;
}
uint32
js::ion::VD(VFPRegister vr)
{
    if (vr.isMissing())
        return 0;
    //bits 15,14,13,12, 22
    VFPRegister::VFPRegIndexSplit s = vr.encode();
    return s.bit << 22 | s.block << 12;
}
uint32
js::ion::VN(VFPRegister vr)
{
    if (vr.isMissing())
        return 0;
    // bits 19,18,17,16, 7
    VFPRegister::VFPRegIndexSplit s = vr.encode();
    return s.bit << 7 | s.block << 16;
}
uint32
js::ion::VM(VFPRegister vr)
{
    if (vr.isMissing())
        return 0;
    // bits 5, 3,2,1,0
    VFPRegister::VFPRegIndexSplit s = vr.encode();
    return s.bit << 5 | s.block;
}


VFPRegister::VFPRegIndexSplit
ion::VFPRegister::encode()
{
    JS_ASSERT(!_isInvalid);

    switch (kind) {
      case Double:
        return VFPRegIndexSplit(_code &0xf , _code >> 4);
      case Single:
        return VFPRegIndexSplit(_code >> 1, _code & 1);
      default:
        // vfp register treated as an integer, NOT a gpr
        return VFPRegIndexSplit(_code >> 1, _code & 1);
    }
}

VFPRegister js::ion::NoVFPRegister(true);

void
Assembler::executableCopy(uint8 *buffer)
{
    executableCopy((void*)buffer);
    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch &rp = jumps_[i];
        JS_NOT_REACHED("Feature NYI");
        //JSC::ARMAssembler::setRel32(buffer + rp.offset, rp.target);
    }
    JSC::ExecutableAllocator::cacheFlush(buffer, m_buffer.size());
}

class RelocationIterator
{
    CompactBufferReader reader_;
    uint32 offset_;

  public:
    RelocationIterator(CompactBufferReader &reader)
      : reader_(reader)
    { }

    bool read() {
        if (!reader_.more())
            return false;
        offset_ = reader_.readUnsigned();
        return true;
    }

    uint32 offset() const {
        return offset_;
    }
};

static inline IonCode *
CodeFromJump(uint8 *jump)
{
    JS_NOT_REACHED("Feature NYI");
#if 0
    uint8 *target = (uint8 *)JSC::ARMAssembler::getRel32Target(jump);
    return IonCode::FromExecutable(target);
#endif
    return NULL;
}

void
Assembler::TraceJumpRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader)
{
    JS_NOT_REACHED("Feature NYI");
#if 0
    RelocationIterator iter(reader);
    while (iter.read()) {
        IonCode *child = CodeFromJump(code->raw() + iter.offset());
        MarkIonCode(trc, child, "rel32");
    };
#endif
}

void
Assembler::TraceDataRelocations(JSTracer *trc, IonCode *code, CompactBufferReader &reader)
{
    JS_NOT_REACHED("Feature NYI");
}

void
Assembler::copyJumpRelocationTable(uint8 *dest)
{
    if (jumpRelocations_.length())
        memcpy(dest, jumpRelocations_.buffer(), jumpRelocations_.length());
}

void
Assembler::copyDataRelocationTable(uint8 *dest)
{
    if (dataRelocations_.length())
        memcpy(dest, dataRelocations_.buffer(), dataRelocations_.length());
}

void
Assembler::trace(JSTracer *trc)
{
    JS_NOT_REACHED("Feature NYI - must trace jump and data");
#if 0
    for (size_t i = 0; i < jumps_.length(); i++) {
        RelativePatch &rp = jumps_[i];
        if (rp.kind == Relocation::CODE)
            MarkIonCode(trc, IonCode::FromExecutable((uint8 *)rp.target), "masmrel32");
    }
#endif
}

void
Assembler::executableCopy(void *buffer)
{
    ASSERT(m_buffer.sizeOfConstantPool() == 0);
    memcpy(buffer, m_buffer.data(), m_buffer.size());
    // whoops, can't call this; it is ARMAssembler-specific.
    // Presently, we don't generate anything that needs to be fixed up.
    // In the future, things that *do* need to be fixed up will
    // likely look radically different from things that needed to be fixed
    // up in JM.
    // TODO: implement this!
#if 0
    fixUpOffsets(buffer);
#endif
}

void
Assembler::processDeferredData(IonCode *code, uint8 *data)
{
    for (size_t i = 0; i < data_.length(); i++) {
        DeferredData *deferred = data_[i];
        Bind(code, deferred->label(), data + deferred->offset());
        deferred->copy(code, data + deferred->offset());
    }
}

void
Assembler::processCodeLabels(IonCode *code)
{
    for (size_t i = 0; i < codeLabels_.length(); i++) {
        CodeLabel *label = codeLabels_[i];
        Bind(code, label->dest(), code->raw() + label->src()->offset());
    }
}

Assembler::Condition
Assembler::InvertCondition(Condition cond)
{
    switch (cond) {
      case Equal:
        //case Zero:
        return NotEqual;
      case Above:
        return BelowOrEqual;
      case AboveOrEqual:
        return Below;
      case Below:
        return AboveOrEqual;
      case BelowOrEqual:
        return Above;
      case LessThan:
        return GreaterThanOrEqual;
        //    case LessThanOrEqual:
        //        return GreaterThan;
      case GreaterThan:
        return LessThanOrEqual;
      case GreaterThanOrEqual:
        return LessThan;
        // still missing overflow, signed.
      default:
        JS_NOT_REACHED("Comparisons other than LT, LE, GT, GE not yet supported");
        return Equal;
    }

}

Imm8::TwoImm8mData
Imm8::encodeTwoImms(uint32 imm)
{
    // In the ideal case, we are looking for a number that (in binary) looks like:
    // 0b((00)*)n_1((00)*)n_2((00)*)
    //    left  n1   mid  n2
    // where both n_1 and n_2 fit into 8 bits.
    // since this is being done with rotates, we also need to handle the case
    // that one of these numbers is in fact split between the left and right
    // sides, in which case the constant will look like:
    // 0bn_1a((00)*)n_2((00)*)n_1b
    //   n1a  mid  n2   rgh    n1b
    // also remember, values are rotated by multiples of two, and left,
    // mid or right can have length zero
    uint32 imm1, imm2;
    int left = (js_bitscan_clz32(imm)) & 30;
    uint32 no_n1 = imm & ~(0xff << (24 - left));
    // not technically needed: this case only happens if we can encode
    // as a single imm8m.  There is a perfectly reasonable encoding in this
    // case, but we shouldn't encourage people to do things like this.
    if (no_n1 == 0)
        return TwoImm8mData();
    int mid = ((js_bitscan_clz32(no_n1)) & 30);
    uint32 no_n2 =
        no_n1  & ~((0xff << ((24 - mid)&31)) | 0xff >> ((8 + mid)&31));
    if (no_n2 == 0) {
        // we hit the easy case, no wraparound.
        // note: a single constant *may* look like this.
        int imm1shift = left + 8;
        int imm2shift = mid + 8;
        imm1 = (imm >> (32 - imm1shift)) & 0xff;
        if (imm2shift >= 32) {
            imm2shift = 0;
            // this assert does not always hold
            //assert((imm & 0xff) == no_n1);
            // in fact, this would lead to some incredibly subtle bugs.
            imm2 = no_n1;
        } else {
            imm2 = ((imm >> (32 - imm2shift)) | (imm << imm2shift)) & 0xff;
            JS_ASSERT( ((no_n1 >> (32 - imm2shift)) | (no_n1 << imm2shift)) ==
                       imm2);
        }
        return TwoImm8mData(datastore::Imm8mData(imm1,imm1shift),
                            datastore::Imm8mData(imm2, imm2shift));
    } else {
        // either it wraps, or it does not fit.
        // if we initially chopped off more than 8 bits, then it won't fit.
        if (left >= 8)
            return TwoImm8mData();
        int right = 32 - (js_bitscan_clz32(no_n2) & 30);
        // all remaining set bits *must* fit into the lower 8 bits
        // the right == 8 case should be handled by the previous case.
        if (right > 8) {
            return TwoImm8mData();
        }
        // make sure the initial bits that we removed for no_n1
        // fit into the 8-(32-right) leftmost bits
        if (((imm & (0xff << (24 - left))) << (8-right)) != 0) {
            // BUT we may have removed more bits than we needed to for no_n1
            // 0x04104001 e.g. we can encode 0x104 with a single op, then
            // 0x04000001 with a second, but we try to encode 0x0410000
            // and find that we need a second op for 0x4000, and 0x1 cannot
            // be included in the encoding of 0x04100000
            no_n1 = imm & ~((0xff >> (8-right)) | (0xff << (24 + right)));
            mid = (js_bitscan_clz32(no_n1)) & 30;
            no_n2 =
                no_n1  & ~((0xff << ((24 - mid)&31)) | 0xff >> ((8 + mid)&31));
            if (no_n2 != 0) {
                return TwoImm8mData();
            }
        }
        // now assemble all of this information into a two coherent constants
        // it is a rotate right from the lower 8 bits.
        int imm1shift = 8 - right;
        imm1 = 0xff & ((imm << imm1shift) | (imm >> (32 - imm1shift)));
        JS_ASSERT ((imm1shift&~0x1e) == 0);
        // left + 8 + mid is the position of the leftmost bit of n_2.
        // we needed to rotate 0x000000ab right by 8 in order to get
        // 0xab000000, then shift again by the leftmost bit in order to
        // get the constant that we care about.
        int imm2shift =  mid + 8;
        imm2 = ((imm >> (32 - imm2shift)) | (imm << imm2shift)) & 0xff;
        return TwoImm8mData(datastore::Imm8mData(imm1,imm1shift),
                            datastore::Imm8mData(imm2, imm2shift));
    }
}

ALUOp
ion::ALUNeg(ALUOp op, Imm32 *imm)
{
    // find an alternate ALUOp to get the job done, and use a different imm.
    switch (op) {
      case op_mov:
        *imm = Imm32(~imm->value);
        return op_mvn;
      case op_mvn:
        *imm = Imm32(~imm->value);
        return op_mov;
      case op_and:
        *imm = Imm32(~imm->value);
        return op_bic;
      case op_bic:
        *imm = Imm32(~imm->value);
        return op_and;
      case op_add:
        *imm = Imm32(-imm->value);
        return op_sub;
      case op_sub:
        *imm = Imm32(-imm->value);
        return op_add;
      case op_cmp:
        *imm = Imm32(-imm->value);
        return op_cmn;
      case op_cmn:
        *imm = Imm32(-imm->value);
        return op_cmp;
        // orr has orn on thumb2 only.
      default:
        break;
    }
    return op_invalid;
}

bool
ion::can_dbl(ALUOp op)
{
    // some instructions can't be processed as two separate instructions
    // such as and, and possibly add (when we're setting ccodes).
    return false;
}

bool
ion::condsAreSafe(ALUOp op) {
    // Even when we are setting condition codes, sometimes we can
    // get away with splitting an operation into two.
    // for example, if our immediate is 0x00ff00ff, and the operation is eors
    // we can split this in half, since x ^ 0x00ff0000 ^ 0x000000ff should
    // set all of its condition codes exactly the same as x ^ 0x00ff00ff.
    // However, if the operation were adds,
    // we cannot split this in half.  If the source on the add is
    // 0xfff00ff0, the result sholud be 0xef10ef, but do we set the overflow bit
    // or not?  Depending on which half is performed first (0x00ff0000
    // or 0x000000ff) the V bit will be set differently, and *not* updating
    // the V bit would be wrong.  Theoretically, the following should work
    // adds r0, r1, 0x00ff0000;
    // addsvs r0, r1, 0x000000ff;
    // addvc r0, r1, 0x000000ff;
    // but this is 3 instructions, and at that point, we might as well use
    // something else.
    return false;
}

ALUOp
ion::getDestVariant(ALUOp op)
{
    // all of the compare operations are dest-less variants of a standard
    // operation.  Given the dest-less variant, return the dest-ful variant.
    switch (op) {
      case op_cmp:
        return op_sub;
      case op_cmn:
        return op_add;
      case op_tst:
        return op_and;
      case op_teq:
        return op_eor;
    }
    return op;
}

O2RegImmShift
ion::O2Reg(Register r) {
    return O2RegImmShift(r, LSL, 0);
}

O2RegImmShift
ion::lsl(Register r, int amt)
{
    return O2RegImmShift(r, LSL, amt);
}

O2RegImmShift
ion::lsr(Register r, int amt)
{
    return O2RegImmShift(r, LSR, amt);
}

O2RegImmShift
ion::ror(Register r, int amt)
{
    return O2RegImmShift(r, ROR, amt);
}
O2RegImmShift
ion::rol(Register r, int amt)
{
    return O2RegImmShift(r, ROR, 32 - amt);
}

O2RegImmShift
ion::asr (Register r, int amt)
{
    return O2RegImmShift(r, ASR, amt);
}


O2RegRegShift
ion::lsl(Register r, Register amt)
{

    return O2RegRegShift(r, LSL, amt);
}

O2RegRegShift
ion::lsr(Register r, Register amt)
{
    return O2RegRegShift(r, LSR, amt);
}

O2RegRegShift
ion::ror(Register r, Register amt)
{
    return O2RegRegShift(r, ROR, amt);
}

O2RegRegShift
ion::asr (Register r, Register amt)
{
    return O2RegRegShift(r, ASR, amt);
}


js::ion::VFPImm::VFPImm(uint32 top)
{
    data = -1;
    datastore::Imm8VFPImmData tmp;
    if (DoubleEncoder::lookup(top, &tmp)) {
        data = tmp.encode();
    }
}

js::ion::DoubleEncoder js::ion::DoubleEncoder::_this;

//VFPRegister implementation
VFPRegister
VFPRegister::doubleOverlay()
{
    JS_ASSERT(!_isInvalid);
    if (kind != Double) {
        return VFPRegister(_code >> 1, Double);
    } else {
        return *this;
    }
}
VFPRegister
VFPRegister::singleOverlay()
{
    JS_ASSERT(!_isInvalid);
    if (kind == Double) {
        // There are no corresponding float registers for d16-d31
        ASSERT(_code < 16);
        return VFPRegister(_code << 1, Single);
    } else {
        return VFPRegister(_code, Single);
    }
}

VFPRegister
VFPRegister::sintOverlay()
{
    JS_ASSERT(!_isInvalid);
    if (kind == Double) {
        // There are no corresponding float registers for d16-d31
        ASSERT(_code < 16);
        return VFPRegister(_code << 1, Int);
    } else {
        return VFPRegister(_code, Int);
    }
}
VFPRegister
VFPRegister::uintOverlay()
{
    JS_ASSERT(!_isInvalid);
    if (kind == Double) {
        // There are no corresponding float registers for d16-d31
        ASSERT(_code < 16);
        return VFPRegister(_code << 1, UInt);
    } else {
        return VFPRegister(_code, UInt);
    }
}

bool
VFPRegister::isInvalid()
{
    return _isInvalid;
}

bool
VFPRegister::isMissing()
{
    JS_ASSERT(!_isInvalid);
    return _isMissing;
}


bool
Assembler::oom() const
{
    return m_buffer.oom() ||
        !enoughMemory_ ||
        jumpRelocations_.oom();
}

bool
Assembler::addDeferredData(DeferredData *data, size_t bytes)
{
    data->setOffset(dataBytesNeeded_);
    dataBytesNeeded_ += bytes;
    if (dataBytesNeeded_ >= MAX_BUFFER_SIZE)
        return false;
    return data_.append(data);
}

bool
Assembler::addCodeLabel(CodeLabel *label)
{
    return codeLabels_.append(label);
}

// Size of the instruction stream, in bytes.
size_t
Assembler::size() const
{
    return m_buffer.uncheckedSize();
}
// Size of the relocation table, in bytes.
size_t
Assembler::jumpRelocationTableBytes() const
{
    return jumpRelocations_.length();
}
size_t
Assembler::dataRelocationTableBytes() const
{
    return dataRelocations_.length();
}

// Size of the data table, in bytes.
size_t
Assembler::dataSize() const
{
    return dataBytesNeeded_;
}
size_t
Assembler::bytesNeeded() const
{
    return size() +
        dataSize() +
        jumpRelocationTableBytes() +
        dataRelocationTableBytes();
}
// write a blob of binary into the instruction stream
void
Assembler::writeInst(uint32 x)
{
    m_buffer.putInt(x);
}

void
Assembler::align(int alignment)
{
    while (!m_buffer.isAligned(alignment))
        as_mov(r0, O2Reg(r0));

}
void
Assembler::as_alu(Register dest, Register src1, Operand2 op2,
                ALUOp op, SetCond_ sc, Condition c)
{
    writeInst((int)op | (int)sc | (int) c | op2.encode() |
              ((dest == InvalidReg) ? 0 : RD(dest)) |
              ((src1 == InvalidReg) ? 0 : RN(src1)));
}
void
Assembler::as_mov(Register dest,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, InvalidReg, op2, op_mov, sc, c);
}
void
Assembler::as_mvn(Register dest, Operand2 op2,
                SetCond_ sc, Condition c)
{
    as_alu(dest, InvalidReg, op2, op_mvn, sc, c);
}
// logical operations
void
Assembler::as_and(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_and, sc, c);
}
void
Assembler::as_bic(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_bic, sc, c);
}
void
Assembler::as_eor(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_eor, sc, c);
}
void
Assembler::as_orr(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_orr, sc, c);
}
// mathematical operations
void
Assembler::as_adc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_adc, sc, c);
}
void
Assembler::as_add(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_add, sc, c);
}
void
Assembler::as_sbc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_sbc, sc, c);
}
void
Assembler::as_sub(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_sub, sc, c);
}
void
Assembler::as_rsb(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_rsb, sc, c);
}
void
Assembler::as_rsc(Register dest, Register src1,
                Operand2 op2, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op_rsc, sc, c);
}
// test operations
void
Assembler::as_cmn(Register src1, Operand2 op2,
                Condition c)
{
    as_alu(InvalidReg, src1, op2, op_cmn, SetCond, c);
}
void
Assembler::as_cmp(Register src1, Operand2 op2,
                Condition c)
{
    as_alu(InvalidReg, src1, op2, op_cmp, SetCond, c);
}
void
Assembler::as_teq(Register src1, Operand2 op2,
                Condition c)
{
    as_alu(InvalidReg, src1, op2, op_teq, SetCond, c);
}
void
Assembler::as_tst(Register src1, Operand2 op2,
                Condition c)
{
    as_alu(InvalidReg, src1, op2, op_tst, SetCond, c);
}

// Not quite ALU worthy, but useful none the less:
// These also have the isue of these being formatted
// completly differently from the standard ALU operations.
void
Assembler::as_movw(Register dest, Imm16 imm, Condition c)
{
    JS_ASSERT(hasMOVWT());
    writeInst(0x03000000 | c | imm.encode() | RD(dest));
}
void
Assembler::as_movt(Register dest, Imm16 imm, Condition c)
{
    JS_ASSERT(hasMOVWT());
    writeInst(0x03400000 | c | imm.encode() | RD(dest));
}
// Data transfer instructions: ldr, str, ldrb, strb.
// Using an int to differentiate between 8 bits and 32 bits is
// overkill, but meh
void
Assembler::as_dtr(LoadStore ls, int size, Index mode,
                Register rt, DTRAddr addr, Condition c)
{
    JS_ASSERT(size == 32 || size == 8);
    writeInst( 0x04000000 | ls | (size == 8 ? 0x00400000 : 0) | mode | c |
               RT(rt) | addr.encode());
    return;
}
// Handles all of the other integral data transferring functions:
// ldrsb, ldrsh, ldrd, etc.
// size is given in bits.
void
Assembler::as_extdtr(LoadStore ls, int size, bool IsSigned, Index mode,
                   Register rt, EDtrAddr addr, Condition c)
{
    int extra_bits2 = 0;
    int extra_bits1 = 0;
    switch(size) {
      case 8:
        JS_ASSERT(IsSigned);
        JS_ASSERT(ls!=IsStore);
        break;
      case 16:
        //case 32:
        // doesn't need to be handled-- it is handled by the default ldr/str
        extra_bits2 = 0x01;
        extra_bits1 = (ls == IsStore) ? 0 : 1;
        if (IsSigned) {
            JS_ASSERT(ls != IsStore);
            extra_bits2 |= 0x2;
        }
        break;
      case 64:
        if (ls == IsStore) {
            extra_bits2 = 0x3;
        } else {
            extra_bits2 = 0x2;
        }
        extra_bits1 = 0;
        break;
      default:
        JS_NOT_REACHED("SAY WHAT?");
    }
    writeInst(extra_bits2 << 5 | extra_bits1 << 20 | 0x90 |
              addr.encode() | RT(rt) | c);
    return;
}

void
Assembler::as_dtm(LoadStore ls, Register rn, uint32 mask,
                DTMMode mode, DTMWriteBack wb, Condition c)
{
    writeInst(0x08000000 | RN(rn) | ls |
              mode | mask | c | wb);

    return;
}

// Control flow stuff:

// bx can *only* branch to a register
// never to an immediate.
void
Assembler::as_bx(Register r, Condition c)
{
    writeInst(((int) c) | op_bx | r.code());
}

// Branch can branch to an immediate *or* to a register.
// Branches to immediates are pc relative, branches to registers
// are absolute
void
Assembler::as_b(BOffImm off, Condition c)
{
    writeInst(((int)c) | op_b | off.encode());
}

void
Assembler::as_b(Label *l, Condition c)
{
    BufferOffset next = nextOffset();
    if (l->bound()) {
        as_b(BufferOffset(l).diffB(next), c);
    } else {
        // Ugh.  int32 :(
        int32 old = l->use(next.getOffset());
        if (old == LabelBase::INVALID_OFFSET) {
            old = -4;
        }
        // This will currently throw an assertion if we couldn't actually
        // encode the offset of the branch.
        as_b(BOffImm(old), c);
    }
}
void
Assembler::as_b(BOffImm off, Condition c, BufferOffset inst)
{
    *editSrc(inst) = ((int)c) | op_b | off.encode();
}

// blx can go to either an immediate or a register.
// When blx'ing to a register, we change processor mode
// depending on the low bit of the register
// when blx'ing to an immediate, we *always* change processor state.
void
Assembler::as_blx(Label *l)
{
    JS_NOT_REACHED("Feature NYI");
}

void
Assembler::as_blx(Register r, Condition c)
{
    writeInst(((int) c) | op_blx | r.code());
}
void
Assembler::as_bl(BOffImm off, Condition c)
{
    writeInst(((int)c) | op_bl | off.encode());
}
// bl can only branch+link to an immediate, never to a register
// it never changes processor state
void
Assembler::as_bl()
{
    JS_NOT_REACHED("Feature NYI");
}
// bl #imm can have a condition code, blx #imm cannot.
// blx reg can be conditional.
void
Assembler::as_bl(Label *l, Condition c)
{
    BufferOffset next = nextOffset();
    if (l->bound()) {
        as_bl(BufferOffset(l).diffB(next), c);
    } else {
        int32 old = l->use(next.getOffset());
        // See if the list was empty :(
        if (old == -1) {
            old = -4;
        }
        // This will fail if we couldn't actually
        // encode the offset of the branch.
        as_bl(BOffImm(old), c);
    }
}
void
Assembler::as_bl(BOffImm off, Condition c, BufferOffset inst)
{
    *editSrc(inst) = ((int)c) | op_bl | off.encode();
}

// VFP instructions!
enum vfp_tags {
    vfp_tag   = 0x0C000A00,
    vfp_arith = 0x02000000
};
void
Assembler::writeVFPInst(vfp_size sz, uint32 blob)
{
    JS_ASSERT((sz & blob) == 0);
    JS_ASSERT((vfp_tag & blob) == 0);
    writeInst(vfp_tag | sz | blob);
}

// Unityped variants: all registers hold the same (ieee754 single/double)
// notably not included are vcvt; vmov vd, #imm; vmov rt, vn.
void
Assembler::as_vfp_float(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  VFPOp op, Condition c)
{
    // Make sure we believe that all of our operands are the same kind
    JS_ASSERT(vd.equiv(vn) && vd.equiv(vm));
    vfp_size sz = vd.isDouble() ? isDouble : isSingle;
    writeVFPInst(sz, VD(vd) | VN(vn) | VM(vm) | op | vfp_arith | c);
}

void
Assembler::as_vadd(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c)
{
    as_vfp_float(vd, vn, vm, opv_add, c);
}

void
Assembler::as_vdiv(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c)
{
    as_vfp_float(vd, vn, vm, opv_mul, c);
}

void
Assembler::as_vmul(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c)
{
    as_vfp_float(vd, vn, vm, opv_mul, c);
}

void
Assembler::as_vnmul(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c)
{
    as_vfp_float(vd, vn, vm, opv_mul, c);
    JS_NOT_REACHED("Feature NYI");
}

void
Assembler::as_vnmla(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c)
{
    JS_NOT_REACHED("Feature NYI");
}

void
Assembler::as_vnmls(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                  Condition c)
{
    JS_NOT_REACHED("Feature NYI");
}

void
Assembler::as_vneg(VFPRegister vd, VFPRegister vm, Condition c)
{
    as_vfp_float(vd, NoVFPRegister, vm, opv_neg, c);
}

void
Assembler::as_vsqrt(VFPRegister vd, VFPRegister vm, Condition c)
{
    as_vfp_float(vd, NoVFPRegister, vm, opv_sqrt, c);
}

void
Assembler::as_vabs(VFPRegister vd, VFPRegister vm, Condition c)
{
    as_vfp_float(vd, NoVFPRegister, vm, opv_abs, c);
}

void
Assembler::as_vsub(VFPRegister vd, VFPRegister vn, VFPRegister vm,
                 Condition c)
{
    as_vfp_float(vd, vn, vm, opv_sub, c);
}

void
Assembler::as_vcmp(VFPRegister vd, VFPRegister vm,
                 Condition c)
{
    as_vfp_float(vd, NoVFPRegister, vm, opv_cmp, c);
}
void
Assembler::as_vcmpz(VFPRegister vd, Condition c)
{
    as_vfp_float(vd, NoVFPRegister, NoVFPRegister, opv_cmpz, c);
}

// specifically, a move between two same sized-registers
void
Assembler::as_vmov(VFPRegister vd, VFPRegister vsrc, Condition c)
{
    as_vfp_float(vd, NoVFPRegister, vsrc, opv_mov, c);
}
//xfer between Core and VFP

// Unlike the next function, moving between the core registers and vfp
// registers can't be *that* properly typed.  Namely, since I don't want to
// munge the type VFPRegister to also include core registers.  Thus, the core
// and vfp registers are passed in based on their type, and src/dest is
// determined by the float2core.

void
Assembler::as_vxfer(Register vt1, Register vt2, VFPRegister vm, FloatToCore_ f2c,
                  Condition c)
{
    vfp_size sz = isSingle;
    if (vm.isDouble()) {
        // Technically, this can be done with a vmov à la ARM ARM under vmov
        // however, that requires at least an extra bit saying if the
        // operation should be performed on the lower or upper half of the
        // double.  Moving a single to/from 2N/2N+1 isn't equivalent,
        // since there are 32 single registers, and 32 double registers
        // so there is no way to encode the last 16 double registers.
        JS_ASSERT(vt2 != InvalidReg);
        sz = isDouble;
    }
    VFPXferSize xfersz = WordTransfer;
    if (vt2 != InvalidReg) {
        // We are doing a 64 bit transfer.
        xfersz = DoubleTransfer;
    }
    writeVFPInst(sz, xfersz | f2c | c |
              RT(vt1) | ((vt2 != InvalidReg) ? RN(vt2) : 0) | VM(vm));
}
enum vcvt_destFloatness {
    toInteger = 1 << 18,
    toFloat  = 0 << 18
};
enum vcvt_Signedness {
    toSigned   = 1 << 16,
    toUnsigned = 0 << 16,
    fromSigned   = 1 << 7,
    fromUnsigned = 0 << 7
};

// our encoding actually allows just the src and the dest (and their types)
// to uniquely specify the encoding that we are going to use.
void
Assembler::as_vcvt(VFPRegister vd, VFPRegister vm,
                 Condition c)
{
    // Unlike other cases, the source and dest types cannot be the same
    JS_ASSERT(!vd.equiv(vm));
    vfp_size sz = isDouble;
    if (vd.isFloat() && vm.isFloat()) {
        // Doing a float -> float conversion
        if (vm.isSingle()) {
            sz = isSingle;
        }
        writeVFPInst(sz, c | 0x04B700C0 |
                  VM(vm) | VD(vd));
    } else {
        // At least one of the registers should be a float.
        vcvt_destFloatness destFloat;
        vcvt_Signedness opSign;
        JS_ASSERT(vd.isFloat() || vm.isFloat());
        if (vd.isSingle() || vm.isSingle()) {
            sz = isSingle;
        }
        if (vd.isFloat()) {
            destFloat = toFloat;
            if (vm.isSInt()) {
                opSign = fromSigned;
            } else {
                opSign = fromUnsigned;
            }
        } else {
            destFloat = toInteger;
            if (vd.isSInt()) {
                opSign = toSigned;
            } else {
                opSign = toUnsigned;
            }
        }
        writeVFPInst(sz, 0x02B80040 | VD(vd) | VM(vm) | destFloat | opSign);
    }

}
// xfer between VFP and memory
void
Assembler::as_vdtr(LoadStore ls, VFPRegister vd, VFPAddr addr,
                 Condition c /* vfp doesn't have a wb option*/)
{
    vfp_size sz = vd.isDouble() ? isDouble : isSingle;
    writeVFPInst(sz, 0x01000000 | addr.encode() | VD(vd) | c);
}

// VFP's ldm/stm work differently from the standard arm ones.
// You can only transfer a range

void
Assembler::as_vdtm(LoadStore st, Register rn, VFPRegister vd, int length,
                 /*also has update conditions*/Condition c)
{
    JS_ASSERT(length <= 16 && length >= 0);
    vfp_size sz = vd.isDouble() ? isDouble : isSingle;

    if (vd.isDouble())
        length *= 2;

    writeVFPInst(sz, dtmLoadStore | RN(rn) | VD(vd) |
              length |
              dtmMode | dtmUpdate | dtmCond);
}

void
Assembler::as_vimm(VFPRegister vd, VFPImm imm, Condition c)
{
    vfp_size sz = vd.isDouble() ? isDouble : isSingle;

    if (!vd.isDouble()) {
        // totally do not know how to handle this right now
        JS_NOT_REACHED("non-double immediate");
    }
    writeVFPInst(sz,  c | imm.encode() | VD(vd) | 0x02B00000);

}
void
Assembler::as_vmrs(Register r, Condition c)
{
    writeInst(c | 0x0ef10a10 | RT(r));
}

bool
Assembler::nextLink(BufferOffset b, BufferOffset *next)
{
    uint32 branch = *editSrc(b);
    JS_ASSERT(((branch & op_b_mask) == op_b) ||
              ((branch & op_b_mask) == op_bl));
    uint32 dest = (branch & op_b_dest_mask);
    // turns out the end marker is the same as the mask.
    if (dest == op_b_dest_mask)
        return false;
    // add in the extra 2 bits of padding that we chopped off when we made the b
    dest = dest << 2;
    // and let everyone know about it.
    new (next) BufferOffset(dest);
    return true;
}

void
Assembler::bind(Label *label, BufferOffset boff)
{
    if (label->used()) {
        bool more;
        // If our caller didn't give us an explicit target to bind to
        // then we want to bind to the location of the next instruction
        BufferOffset dest = boff.assigned() ? boff : nextOffset();
        BufferOffset b(label);
        do {
            BufferOffset next;
            more = nextLink(b, &next);
            uint32 branch = *editSrc(b);
            Condition c = getCondition(branch);
            switch (branch & op_b_mask) {
              case op_b:
                as_b(dest.diffB(b), c, b);
                break;
              case op_bl:
                as_bl(dest.diffB(b), c, b);
                break;
              default:
                JS_NOT_REACHED("crazy fixup!");
            }
            b = next;
        } while (more);
    }
    label->bind(nextOffset().getOffset());
}

void
Assembler::retarget(Label *label, Label *target)
{
    if (label->used()) {
        if (target->bound()) {
            bind(label, BufferOffset(target));
        } else {
            // The target is unbound.  We can just take the head of the list
            // hanging off of label, and dump that into target.
            uint32 prev = target->use(label->offset());
            JS_ASSERT(prev == Label::INVALID_OFFSET);
        }
    }
    label->reset();

}


void
Assembler::Bind(IonCode *code, AbsoluteLabel *label, const void *address)
{
#if 0
    uint8 *raw = code->raw();
    if (label->used()) {
        intptr_t src = label->offset();
        do {
            intptr_t next = reinterpret_cast<intptr_t>(JSC::ARMAssembler::getPointer(raw + src));
            JSC::ARMAssembler::setPointer(raw + src, address);
            src = next;
        } while (src != AbsoluteLabel::INVALID_OFFSET);
    }
    JS_ASSERT(((uint8 *)address - raw) >= 0 && ((uint8 *)address - raw) < INT_MAX);
    label->bind();
#endif
    JS_NOT_REACHED("Feature NYI");
}

void
Assembler::call(Label *label)
{
#if 0
    if (label->bound()) {
        masm.linkJump(masm.call(), JmpDst(label->offset()));
    } else {
        JmpSrc j = masm.call();
        JmpSrc prev = JmpSrc(label->use(j.offset()));
        masm.setNextJump(j, prev);
    }
#endif
    JS_NOT_REACHED("Feature NYI");
}

void
Assembler::as_bkpt()
{
    writeInst(0xe1200070);
}
