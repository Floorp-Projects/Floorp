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
 *  Marty Rosenberg <mrosenberg@mozilla.com>
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
#include "ion/arm/MacroAssembler-arm.h"
#include "ion/MoveEmitter.h"

using namespace js;
using namespace ion;
bool
isValueDTRDCandidate(ValueOperand &val)
{
    // In order to be used for a DTRD memory function, the two target registers
    // need to be a) Adjacent, with the tag larger than the payload, and
    // b) Aligned to a multiple of two.
    if ((val.typeReg().code() != (val.payloadReg().code() + 1)))
        return false;
    else if ((val.payloadReg().code() & 1) != 0)
        return false;
    return true;
}

void
MacroAssemblerARM::convertInt32ToDouble(const Register &src, const FloatRegister &dest_)
{
    // direct conversions aren't possible.
    VFPRegister dest = VFPRegister(dest_);
    as_vxfer(src, InvalidReg, dest.sintOverlay(),
             CoreToFloat);
    as_vcvt(dest, dest.sintOverlay());
}

bool
MacroAssemblerARM::alu_dbl(Register src1, Imm32 imm, Register dest, ALUOp op,
                           SetCond_ sc, Condition c)
{
    if ((sc == SetCond && ! condsAreSafe(op)) || !can_dbl(op)) {
        return false;
    }
    ALUOp interop = getDestVariant(op);
    Imm8::TwoImm8mData both = Imm8::encodeTwoImms(imm.value);
    if (both.fst.invalid) {
        return false;
    }
    // for the most part, there is no good reason to set the condition
    // codes for the first instruction.
    // we can do better things if the second instruction doesn't
    // have a dest, such as check for overflow by doing first operation
    // don't do second operation if first operation overflowed.
    // this preserves the overflow condition code.
    // unfortunately, it is horribly brittle.
    as_alu(ScratchRegister, src1, both.fst, interop, NoSetCond, c);
    as_alu(ScratchRegister, ScratchRegister, both.snd, op, sc, c);
    // we succeeded!
    return true;
}


void
MacroAssemblerARM::ma_alu(Register src1, Imm32 imm, Register dest,
                          ALUOp op,
                          SetCond_ sc, Condition c)
{
    // As it turns out, if you ask for a compare-like instruction
    // you *probably* want it to set condition codes.
    if (dest == InvalidReg) {
        JS_ASSERT(sc == SetCond);
    }

    // The operator gives us the ability to determine how
    // this can be used.
    Imm8 imm8 = Imm8(imm.value);
    // ONE INSTRUCTION:
    // If we can encode it using an imm8m, then do so.
    if (!imm8.invalid) {
        as_alu(dest, src1, imm8, op, sc, c);
        return;
    }
    // ONE INSTRUCTION, NEGATED:
    Imm32 negImm = imm;
    ALUOp negOp = ALUNeg(op, &negImm);
    Imm8 negImm8 = Imm8(negImm.value);
    if (negOp != op_invalid && !negImm8.invalid) {
        as_alu(dest, src1, negImm8, negOp, sc, c);
        return;
    }
    if (hasMOVWT()) {
        // If the operation is a move-a-like then we can try to use movw to
        // move the bits into the destination.  Otherwise, we'll need to
        // fall back on a multi-instruction format :(
        // movw/movt don't set condition codes, so don't hold your breath.
        if (sc == NoSetCond && (op == op_mov || op == op_mvn)) {
            // ARMv7 supports movw/movt. movw zero-extends
            // its 16 bit argument, so we can set the register
            // this way.
            // movt leaves the bottom 16 bits in tact, so
            // it is unsuitable to move a constant that
            if (op == op_mov && ((imm.value & ~ 0xffff) == 0)) {
                JS_ASSERT(src1 == InvalidReg);
                as_movw(dest, (uint16)imm.value, c);
                return;
            }
            // If they asked for a mvn rfoo, imm, where ~imm fits into 16 bits
            // then do it.
            if (op == op_mvn && (((~imm.value) & ~ 0xffff) == 0)) {
                JS_ASSERT(src1 == InvalidReg);
                as_movw(dest, (uint16)~imm.value, c);
                return;
            }
            // TODO: constant dedup may enable us to add dest, r0, 23 *if*
            // we are attempting to load a constant that looks similar to one
            // that already exists
            // If it can't be done with a single movw
            // then we *need* to use two instructions
            // since this must be some sort of a move operation, we can just use
            // a movw/movt pair and get the whole thing done in two moves.  This
            // does not work for ops like add, sinc we'd need to do
            // movw tmp; movt tmp; add dest, tmp, src1
            if (op == op_mvn)
                imm.value = ~imm.value;
            as_movw(dest, imm.value & 0xffff, c);
            as_movt(dest, (imm.value >> 16) & 0xffff, c);
            return;
        }
        // If we weren't doing a movalike, a 16 bit immediate
        // will require 2 instructions.  With the same amount of
        // space and (less)time, we can do two 8 bit operations, reusing
        // the dest register.  e.g.
        // movw tmp, 0xffff; add dest, src, tmp ror 4
        // vs.
        // add dest, src, 0xff0; add dest, dest, 0xf000000f
        // it turns out that there are some immediates that we miss with the
        // second approach.  A sample value is: add dest, src, 0x1fffe
        // this can be done by movw tmp, 0xffff; add dest, src, tmp lsl 1
        // since imm8m's only get even offsets, we cannot encode this.
        // I'll try to encode as two imm8's first, since they are faster.
        // Both operations should take 1 cycle, where as add dest, tmp ror 4
        // takes two cycles to execute.
    }
    // Either a) this isn't ARMv7 b) this isn't a move
    // start by attempting to generate a two instruction form.
    // Some things cannot be made into two-inst forms correctly.
    // namely, adds dest, src, 0xffff.
    // Since we want the condition codes (and don't know which ones will
    // be checked), we need to assume that the overflow flag will be checked
    // and add{,s} dest, src, 0xff00; add{,s} dest, dest, 0xff is not
    // guaranteed to set the overflow flag the same as the (theoretical)
    // one instruction variant.
    if (alu_dbl(src1, imm, dest, op, sc, c))
        return;
    // And try with its negative.
    if (negOp != op_invalid &&
        alu_dbl(src1, negImm, dest, negOp, sc, c))
        return;
    // Well, damn. We can use two 16 bit mov's, then do the op
    // or we can do a single load from a pool then op.
    if (hasMOVWT()) {
        // Try to load the immediate into a scratch register
        // then use that
        as_movw(ScratchRegister, imm.value & 0xffff, c);
        as_movt(ScratchRegister, (imm.value >> 16) & 0xffff, c);
    } else {
        JS_NOT_REACHED("non-ARMv7 loading of immediates NYI.");
    }
    as_alu(dest, src1, O2Reg(ScratchRegister), op, sc, c);
    // done!
}

void
MacroAssemblerARM::ma_alu(Register src1, Operand op2, Register dest, ALUOp op,
            SetCond_ sc, Assembler::Condition c)
{
    JS_ASSERT(op2.getTag() == Operand::OP2);
    as_alu(dest, src1, op2.toOp2(), op, sc, c);
}

void
MacroAssemblerARM::ma_alu(Register src1, Operand2 op2, Register dest, ALUOp op, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, op2, op, sc, c);
}
void
MacroAssemblerARM::ma_mov(Register src, Register dest,
            SetCond_ sc, Assembler::Condition c)
{
    as_mov(dest, O2Reg(src), sc, c);
}

void
MacroAssemblerARM::ma_mov(Imm32 imm, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(InvalidReg, imm, dest, op_mov, sc, c);
}
void
MacroAssemblerARM::ma_mov(const ImmGCPtr &ptr, Register dest)
{
    writeDataRelocation(nextOffset());
    ma_mov(Imm32(ptr.value), dest);
}

    // Shifts (just a move with a shifting op2)
void
MacroAssemblerARM::ma_lsl(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, lsl(src, shift.value));
}
void
MacroAssemblerARM::ma_lsr(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, lsr(src, shift.value));
}
void
MacroAssemblerARM::ma_asr(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, asr(src, shift.value));
}
void
MacroAssemblerARM::ma_ror(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, ror(src, shift.value));
}
void
MacroAssemblerARM::ma_rol(Imm32 shift, Register src, Register dst)
{
    as_mov(dst, rol(src, shift.value));
}
    // Shifts (just a move with a shifting op2)
void
MacroAssemblerARM::ma_lsl(Register shift, Register src, Register dst)
{
    as_mov(dst, lsl(src, shift));
}
void
MacroAssemblerARM::ma_lsr(Register shift, Register src, Register dst)
{
    as_mov(dst, lsr(src, shift));
}
void
MacroAssemblerARM::ma_asr(Register shift, Register src, Register dst)
{
    as_mov(dst, asr(src, shift));
}
void
MacroAssemblerARM::ma_ror(Register shift, Register src, Register dst)
{
    as_mov(dst, ror(src, shift));
}
void
MacroAssemblerARM::ma_rol(Register shift, Register src, Register dst)
{
    ma_rsb(shift, Imm32(32), ScratchRegister);
    as_mov(dst, ror(src, ScratchRegister));
}

    // Move not (dest <- ~src)

void
MacroAssemblerARM::ma_mvn(Imm32 imm, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(InvalidReg, imm, dest, op_mvn, sc, c);
}

void
MacroAssemblerARM::ma_mvn(Register src1, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    as_alu(dest, InvalidReg, O2Reg(src1), op_mvn, sc, c);
}

    // and
void
MacroAssemblerARM::ma_and(Register src, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_and(dest, src, dest);
}
void
MacroAssemblerARM::ma_and(Register src1, Register src2, Register dest,
            SetCond_ sc, Assembler::Condition c)
{
    as_and(dest, src1, O2Reg(src2), sc, c);
}
void
MacroAssemblerARM::ma_and(Imm32 imm, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(dest, imm, dest, op_and, sc, c);
}
void
MacroAssemblerARM::ma_and(Imm32 imm, Register src1, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(src1, imm, dest, op_and, sc, c);
}


    // bit clear (dest <- dest & ~imm) or (dest <- src1 & ~src2)
void
MacroAssemblerARM::ma_bic(Imm32 imm, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(dest, imm, dest, op_bic, sc, c);
}

    // exclusive or
void
MacroAssemblerARM::ma_eor(Register src, Register dest,
            SetCond_ sc, Assembler::Condition c)
{
    ma_eor(dest, src, dest, sc, c);
}
void
MacroAssemblerARM::ma_eor(Register src1, Register src2, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    as_eor(dest, src1, O2Reg(src2), sc, c);
}
void
MacroAssemblerARM::ma_eor(Imm32 imm, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(dest, imm, dest, op_eor, sc, c);
}
void
MacroAssemblerARM::ma_eor(Imm32 imm, Register src1, Register dest,
       SetCond_ sc, Assembler::Condition c)
{
    ma_alu(src1, imm, dest, op_eor, sc, c);
}

    // or
void
MacroAssemblerARM::ma_orr(Register src, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_orr(dest, src, dest, sc, c);
}
void
MacroAssemblerARM::ma_orr(Register src1, Register src2, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    as_orr(dest, src1, O2Reg(src2), sc, c);
}
void
MacroAssemblerARM::ma_orr(Imm32 imm, Register dest,
                          SetCond_ sc, Assembler::Condition c)
{
    ma_alu(dest, imm, dest, op_orr, sc, c);
}
void
MacroAssemblerARM::ma_orr(Imm32 imm, Register src1, Register dest,
                SetCond_ sc, Assembler::Condition c)
{
    ma_alu(src1, imm, dest, op_orr, sc, c);
}

    // arithmetic based ops
    // add with carry
void
MacroAssemblerARM::ma_adc(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_adc, sc, c);
}
void
MacroAssemblerARM::ma_adc(Register src, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, dest, O2Reg(src), op_adc, sc, c);
}
void
MacroAssemblerARM::ma_adc(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_adc, sc, c);
}

    // add
void
MacroAssemblerARM::ma_add(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_add, sc, c);
}

void
MacroAssemblerARM::ma_add(Register src1, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, O2Reg(src1), dest, op_add, sc, c);
}
void
MacroAssemblerARM::ma_add(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_add, sc, c);
}
void
MacroAssemblerARM::ma_add(Register src1, Operand op, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op, dest, op_add, sc, c);
}
void
MacroAssemblerARM::ma_add(Register src1, Imm32 op, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op, dest, op_add, sc, c);
}

    // subtract with carry
void
MacroAssemblerARM::ma_sbc(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_sbc, sc, c);
}
void
MacroAssemblerARM::ma_sbc(Register src1, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, dest, O2Reg(src1), op_sbc, sc, c);
}
void
MacroAssemblerARM::ma_sbc(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_sbc, sc, c);
}

    // subtract
void
MacroAssemblerARM::ma_sub(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_sub, sc, c);
}
void
MacroAssemblerARM::ma_sub(Register src1, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, Operand(src1), dest, op_sub, sc, c);
}
void
MacroAssemblerARM::ma_sub(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, Operand(src2), dest, op_sub, sc, c);
}
void
MacroAssemblerARM::ma_sub(Register src1, Operand op, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op, dest, op_sub, sc, c);
}
void
MacroAssemblerARM::ma_sub(Register src1, Imm32 op, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op, dest, op_sub, sc, c);
}

    // reverse subtract
void
MacroAssemblerARM::ma_rsb(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_rsb, sc, c);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, dest, O2Reg(src1), op_add, sc, c);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_rsb, sc, c);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Imm32 op2, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(src1, op2, dest, op_rsb, sc, c);
}

    // reverse subtract with carry
void
MacroAssemblerARM::ma_rsc(Imm32 imm, Register dest, SetCond_ sc, Condition c)
{
    ma_alu(dest, imm, dest, op_rsc, sc, c);
}
void
MacroAssemblerARM::ma_rsc(Register src1, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, dest, O2Reg(src1), op_rsc, sc, c);
}
void
MacroAssemblerARM::ma_rsc(Register src1, Register src2, Register dest, SetCond_ sc, Condition c)
{
    as_alu(dest, src1, O2Reg(src2), op_rsc, sc, c);
}

    // compares/tests
    // compare negative (sets condition codes as src1 + src2 would)
void
MacroAssemblerARM::ma_cmn(Register src1, Imm32 imm, Condition c)
{
    ma_alu(src1, imm, InvalidReg, op_cmn);
}
void
MacroAssemblerARM::ma_cmn(Register src1, Register src2, Condition c)
{
    as_alu(InvalidReg, src2, O2Reg(src1), op_cmn);
}
void
MacroAssemblerARM::ma_cmn(Register src1, Operand op, Condition c)
{
    JS_NOT_REACHED("Feature NYI");
}

// compare (src - src2)
void
MacroAssemblerARM::ma_cmp(Register src1, Imm32 imm, Condition c)
{
    ma_alu(src1, imm, InvalidReg, op_cmp, SetCond, c);
}

void
MacroAssemblerARM::ma_cmp(Register src1, ImmGCPtr ptr, Condition c)
{
    writeDataRelocation(nextOffset());
    ma_alu(src1, Imm32(ptr.value), InvalidReg, op_cmp, SetCond, c);
}
void
MacroAssemblerARM::ma_cmp(Register src1, Operand op, Condition c)
{
    switch (op.getTag()) {
      case Operand::OP2:
        as_cmp(src1, op.toOp2(), c);
        break;
      case Operand::MEM:
        ma_ldr(op, ScratchRegister);
        as_cmp(src1, O2Reg(ScratchRegister), c);
        break;
      default:
        JS_NOT_REACHED("trying to compare FP and integer registers");
        break;
    }
}
void
MacroAssemblerARM::ma_cmp(Register src1, Register src2, Condition c)
{
    as_cmp(src2, O2Reg(src1), c);
}

    // test for equality, (src1^src2)
void
MacroAssemblerARM::ma_teq(Imm32 imm, Register src1, Condition c)
{
    ma_alu(src1, imm, InvalidReg, op_teq, SetCond, c);
}
void
MacroAssemblerARM::ma_teq(Register src2, Register src1, Condition c)
{
    as_tst(src2, O2Reg(src1), c);
}
void
MacroAssemblerARM::ma_teq(Register src1, Operand op, Condition c)
{
        as_teq(src1, op.toOp2(), c);
}


// test (src1 & src2)
void
MacroAssemblerARM::ma_tst(Imm32 imm, Register src1, Condition c)
{
    ma_alu(src1, imm, InvalidReg, op_tst, SetCond, c);
}
void
MacroAssemblerARM::ma_tst(Register src1, Register src2, Condition c)
{
    as_tst(src1, O2Reg(src2), c);
}
void
MacroAssemblerARM::ma_tst(Register src1, Operand op, Condition c)
{
    as_tst(src1, op.toOp2(), c);
}

void
MacroAssemblerARM::ma_mul(Register src1, Register src2, Register dest)
{
    as_mul(dest, src1, src2);
}
void
MacroAssemblerARM::ma_mul(Register src1, Imm32 imm, Register dest)
{
    // TODO: be smarter!
    ma_mov(imm, ScratchRegister);
    as_mul( dest, src1, ScratchRegister);
}

Assembler::Condition
MacroAssemblerARM::ma_check_mul(Register src1, Register src2, Register dest, Condition cond)
{
    // TODO: this operation is illegal on armv6 and earlier if src2 == ScratchRegister
    //       or src2 == dest.
    if (cond == Equal || cond == NotEqual) {
        as_smull(ScratchRegister, dest, src1, src2, SetCond);
        return cond;
    } else if (cond == Overflow) {
        as_smull(ScratchRegister, dest, src1, src2);
        as_cmp(ScratchRegister, asr(dest, 31));
        return NotEqual;
    }
    JS_NOT_REACHED("Condition NYI");
    return Always;

}

Assembler::Condition
MacroAssemblerARM::ma_check_mul(Register src1, Imm32 imm, Register dest, Condition cond)
{
    ma_mov(imm, ScratchRegister);
    if (cond == Equal || cond == NotEqual) {
        as_smull(ScratchRegister, dest, ScratchRegister, src1, SetCond);
        return cond;
    } else if (cond == Overflow) {
        as_smull(ScratchRegister, dest, ScratchRegister, src1);
        as_cmp(ScratchRegister, asr(dest, 31));
        return NotEqual;
    }
    JS_NOT_REACHED("Condition NYI");
    return Always;
}


// memory
// shortcut for when we know we're transferring 32 bits of data
void
MacroAssemblerARM::ma_dtr(LoadStore ls, Register rn, Imm32 offset, Register rt,
                          Index mode, Assembler::Condition cc)
{
    int off = offset.value;
    if (off < 4096 && off > -4096) {
        // simplest offset, just use an immediate
        as_dtr(ls, 32, mode, rt, DTRAddr(rn, DtrOffImm(off)), cc);
        return;
    }
    // see if we can attempt to encode it as a standard imm8m offset
    datastore::Imm8mData imm = Imm8::encodeImm(off & (~0xfff));
    if (!imm.invalid) {
        as_add(ScratchRegister, rn, imm);
        as_dtr(ls, 32, mode, rt, DTRAddr(ScratchRegister, DtrOffImm(off & 0xfff)), cc);
    } else {
        ma_mov(offset, ScratchRegister);
        as_dtr(ls, 32, mode, rt, DTRAddr(rn, DtrRegImmShift(ScratchRegister, LSL, 0)));
    }
}

void
MacroAssemblerARM::ma_dtr(LoadStore ls, Register rn, Register rm, Register rt,
                          Index mode, Assembler::Condition cc)
{
    JS_NOT_REACHED("Feature NYI");
}

void
MacroAssemblerARM::ma_str(Register rt, DTRAddr addr, Index mode, Condition cc)
{
    as_dtr(IsStore, 32, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_dtr(LoadStore ls, Register rt, const Operand &addr, Index mode, Condition cc)
{
    int off = addr.disp();
    Register base = Register::FromCode(addr.base());
    if (off > -4096 && off < 4096) {
        as_dtr(ls, 32, mode, rt, addr.toDTRAddr(), cc);
        return;
    }
    // We cannot encode this offset in a a single ldr.  Try to encode it as
    // an add scratch, base, imm; ldr dest, [scratch, +offset].
    int bottom = off & 0xfff;
    int neg_bottom = 0x1000 - bottom;
    // at this point, both off - bottom and off + neg_bottom will be reasonable-ish
    // quantities.
    if (off < 0) {
        Operand2 sub_off = Imm8(-(off-bottom)); // sub_off = bottom - off
        if (!sub_off.invalid) {
            as_sub(ScratchRegister, base, sub_off, NoSetCond, cc); // - sub_off = off - bottom
            as_dtr(ls, 32, Offset, rt, DTRAddr(ScratchRegister, DtrOffImm(bottom)), cc);
            return;
        }
        sub_off = Imm8(-(off+neg_bottom));// sub_off = -neg_bottom - off
        if (!sub_off.invalid) {
            as_sub(ScratchRegister, base, sub_off, NoSetCond, cc); // - sub_off = neg_bottom + off
            as_dtr(ls, 32, Offset, rt, DTRAddr(ScratchRegister, DtrOffImm(-neg_bottom)), cc);
            return;
        }
    } else {
        Operand2 sub_off = Imm8(off-bottom); // sub_off = off - bottom
        if (!sub_off.invalid) {
            as_add(ScratchRegister, base, sub_off, NoSetCond, cc); //  sub_off = off - bottom
            as_dtr(ls, 32, Offset, rt, DTRAddr(ScratchRegister, DtrOffImm(bottom)), cc);
            return;
        }
        sub_off = Imm8(off+neg_bottom);// sub_off = neg_bottom + off
        if (!sub_off.invalid) {
            as_add(ScratchRegister, base, sub_off, NoSetCond,  cc); // sub_off = neg_bottom + off
            as_dtr(ls, 32, Offset, rt, DTRAddr(ScratchRegister, DtrOffImm(-neg_bottom)), cc);
            return;
        }
    }
    JS_NOT_REACHED("TODO: implement bigger offsets :(");
}
void
MacroAssemblerARM::ma_str(Register rt, const Operand &addr, Index mode, Condition cc)
{
    ma_dtr(IsStore, rt, addr, mode, cc);
}
void
MacroAssemblerARM::ma_strd(Register rt, DebugOnly<Register> rt2, EDtrAddr addr, Index mode, Condition cc)
{
    JS_ASSERT((rt.code() & 1) == 0);
    JS_ASSERT(rt2.value.code() == rt.code() + 1);
    as_extdtr(IsStore, 64, true, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_ldr(DTRAddr addr, Register rt, Index mode, Condition cc)
{
    as_dtr(IsLoad, 32, mode, rt, addr, cc);
}
void
MacroAssemblerARM::ma_ldr(const Operand &addr, Register rt, Index mode, Condition cc)
{
    ma_dtr(IsLoad, rt, addr, mode, cc);
}

void
MacroAssemblerARM::ma_ldrb(DTRAddr addr, Register rt, Index mode, Condition cc)
{
    as_dtr(IsLoad, 8, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_ldrsh(EDtrAddr addr, Register rt, Index mode, Condition cc)
{
    as_extdtr(IsLoad, 16, true, mode, rt, addr, cc);
}

void
MacroAssemblerARM::ma_ldrh(EDtrAddr addr, Register rt, Index mode, Condition cc)
{
    as_extdtr(IsLoad, 16, false, mode, rt, addr, cc);
}
void
MacroAssemblerARM::ma_ldrsb(EDtrAddr addr, Register rt, Index mode, Condition cc)
{
    as_extdtr(IsLoad, 8, true, mode, rt, addr, cc);
}
void
MacroAssemblerARM::ma_ldrd(EDtrAddr addr, Register rt, DebugOnly<Register> rt2, Index mode, Condition cc)
{
    JS_ASSERT((rt.code() & 1) == 0);
    JS_ASSERT(rt2.value.code() == rt.code() + 1);
    as_extdtr(IsLoad, 64, true, mode, rt, addr, cc);
}

// specialty for moving N bits of data, where n == 8,16,32,64
void
MacroAssemblerARM::ma_dataTransferN(LoadStore ls, int size, bool IsSigned,
                          Register rn, Register rm, Register rt,
                          Index mode, Assembler::Condition cc)
{
    JS_NOT_REACHED("Feature NYI");
}

void
MacroAssemblerARM::ma_dataTransferN(LoadStore ls, int size, bool IsSigned,
                          Register rn, Imm32 offset, Register rt,
                          Index mode, Assembler::Condition cc)
{
    int x = offset.value;
    // we can encode this as a standard ldr... MAKE IT SO
    if (size == 32 || (size == 8 && !IsSigned) ) {
        if (x < 4096 && x > -4096) {
            as_dtr(ls, size, mode, rt, DTRAddr(rn, DtrOffImm(x)), cc);
        } else {
            JS_NOT_REACHED("Feature NYI");
        }
    } else {
        // should attempt to use the extended load/store instructions
        if (x < 256 && x > -256) {
            as_extdtr(ls, size, IsSigned, mode, rt, EDtrAddr(rn, EDtrOffImm(x)), cc);
        } else {
            JS_NOT_REACHED("Feature NYI");
        }
    }
}
void
MacroAssemblerARM::ma_pop(Register r)
{
    ma_dtr(IsLoad, sp, Imm32(4), r, PostIndex);
    if (r == pc)
        m_buffer.markGuard();
}
void
MacroAssemblerARM::ma_push(Register r)
{
    ma_dtr(IsStore, sp,Imm32(-4), r, PreIndex);
}

// branches when done from within arm-specific code
void
MacroAssemblerARM::ma_b(Label *dest, Assembler::Condition c)
{
    as_b(dest, c);
}

enum RelocBranchStyle {
    MOVWT,
    LDR_BX,
    LDR,
    MOVW_ADD
};

RelocBranchStyle
b_type()
{
    return MOVWT;
}
void
MacroAssemblerARM::ma_b(void *target, Relocation::Kind reloc, Assembler::Condition c)
{
    // we know the absolute address of the target, but not our final
    // location (with relocating GC, we *can't* know our final location)
    // for now, I'm going to be conservative, and load this with an
    // absolute address
    uint32 trg = (uint32)target;
    switch (b_type()) {
      case MOVWT:
        as_movw(ScratchRegister, Imm16(trg & 0xffff), c);
        as_movt(ScratchRegister, Imm16(trg >> 16), c);
        // this is going to get the branch predictor pissed off.
        as_bx(ScratchRegister, c);
        break;
      default:
        JS_NOT_REACHED("Other methods of generating tracable jumps NYI");
    }
}

// this is almost NEVER necessary, we'll basically never be calling a label
// except, possibly in the crazy bailout-table case.
void
MacroAssemblerARM::ma_bl(Label *dest, Assembler::Condition c)
{
    as_bl(dest, c);
}

//VFP/ALU
void
MacroAssemblerARM::ma_vadd(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vadd(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}

void
MacroAssemblerARM::ma_vsub(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vsub(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}

void
MacroAssemblerARM::ma_vmul(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vmul(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}

void
MacroAssemblerARM::ma_vdiv(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vdiv(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}

void
MacroAssemblerARM::ma_vmov(FloatRegister src, FloatRegister dest)
{
    as_vmov(dest, src);
}

void
MacroAssemblerARM::ma_vimm(double value, FloatRegister dest)
{
    jsdpun dpun;
    dpun.d = value;
    if ((dpun.s.lo) == 0) {
        if (dpun.s.hi == 0) {
            // to zero a register, load 1.0, then execute dN <- dN - dN
            VFPImm dblEnc(0x3FF00000);
            as_vimm(dest, dblEnc);
            as_vsub(dest, dest, dest);
            return;
        }
        VFPImm dblEnc(dpun.s.hi);
        if (dblEnc.isValid()) {
            as_vimm(dest, dblEnc);
            return;
        }

    }
    // fall back to putting the value in a pool.
    as_FImm64Pool(dest, value);
}

void
MacroAssemblerARM::ma_vcmp(FloatRegister src1, FloatRegister src2)
{
    as_vcmp(VFPRegister(src1), VFPRegister(src2));
}
void
MacroAssemblerARM::ma_vcvt_F64_I32(FloatRegister src, FloatRegister dest)
{
    as_vcvt(VFPRegister(dest).sintOverlay(), VFPRegister(src));
}
void
MacroAssemblerARM::ma_vcvt_I32_F64(FloatRegister dest, FloatRegister src)
{
    as_vcvt(VFPRegister(dest), VFPRegister(src).sintOverlay());
}
void
MacroAssemblerARM::ma_vxfer(FloatRegister src, Register dest)
{
    as_vxfer(dest, InvalidReg, VFPRegister(src).singleOverlay(), FloatToCore);
}
void
MacroAssemblerARM::ma_vdtr(LoadStore ls, const Operand &addr, FloatRegister rt, Condition cc)
{
    int off = addr.disp();
    JS_ASSERT((off & 3) == 0);
    Register base = Register::FromCode(addr.base());
    if (off > -1024 && off < 1024) {
        as_vdtr(ls, rt, addr.toVFPAddr(), cc);
        return;
    }
    // We cannot encode this offset in a a single ldr.  Try to encode it as
    // an add scratch, base, imm; ldr dest, [scratch, +offset].
    int bottom = off & (0xff << 2);
    int neg_bottom = (0x100 << 2) - bottom;
    // at this point, both off - bottom and off + neg_bottom will be reasonable-ish
    // quantities.
    if (off < 0) {
        Operand2 sub_off = Imm8(-(off-bottom)); // sub_off = bottom - off
        if (!sub_off.invalid) {
            as_sub(ScratchRegister, base, sub_off, NoSetCond, cc); // - sub_off = off - bottom
            as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(bottom)), cc);
            return;
        }
        sub_off = Imm8(-(off+neg_bottom));// sub_off = -neg_bottom - off
        if (!sub_off.invalid) {
            as_sub(ScratchRegister, base, sub_off, NoSetCond, cc); // - sub_off = neg_bottom + off
            as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(-neg_bottom)), cc);
            return;
        }
    } else {
        Operand2 sub_off = Imm8(off-bottom); // sub_off = off - bottom
        if (!sub_off.invalid) {
            as_add(ScratchRegister, base, sub_off, NoSetCond, cc); //  sub_off = off - bottom
            as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(bottom)), cc);
            return;
        }
        sub_off = Imm8(off+neg_bottom);// sub_off = neg_bottom + off
        if (!sub_off.invalid) {
            as_add(ScratchRegister, base, sub_off, NoSetCond,  cc); // sub_off = neg_bottom + off
            as_vdtr(ls, rt, VFPAddr(ScratchRegister, VFPOffImm(-neg_bottom)), cc);
            return;
        }
    }
    JS_NOT_REACHED("TODO: implement bigger offsets :(");

}

void
MacroAssemblerARM::ma_vldr(VFPAddr addr, FloatRegister dest)
{
    as_vdtr(IsLoad, dest, addr);
}
void
MacroAssemblerARM::ma_vldr(const Operand &addr, FloatRegister dest)
{
    ma_vdtr(IsLoad, addr, dest);
}

void
MacroAssemblerARM::ma_vstr(FloatRegister src, VFPAddr addr)
{
    as_vdtr(IsStore, src, addr);
}

void
MacroAssemblerARM::ma_vstr(FloatRegister src, const Operand &addr)
{
    ma_vdtr(IsStore, addr, src);
}
void
MacroAssemblerARM::ma_vstr(FloatRegister src, Register base, Register index, int32 shift)
{
    as_add(ScratchRegister, base, lsl(index, shift));
    ma_vstr(src, Operand(ScratchRegister, 0));
}

void
MacroAssemblerARMCompat::callWithExitFrame(IonCode *target)
{
    uint32 descriptor = MakeFrameDescriptor(framePushed(), IonFrame_JS);
    Push(ScratchRegister); // padding
    Push(Imm32(descriptor)); // descriptor
    // TODO: Use relocation here.
    ma_mov(Imm32((int) target->raw()), ScratchRegister);
    callIon(ScratchRegister);
}

void
MacroAssemblerARMCompat::callIon(const Register &callee)
{
    JS_ASSERT((framePushed() & 3) == 0);
    if ((framePushed() & 7) == 4) {
        ma_callIonHalfPush(callee);
    } else {
        adjustFrame(sizeof(void*));
        ma_callIon(callee);
    }
}

void
MacroAssemblerARMCompat::reserveStack(uint32 amount)
{
    if (amount)
        ma_sub(Imm32(amount), sp);
    adjustFrame(amount);
}
void
MacroAssemblerARMCompat::freeStack(uint32 amount)
{
    JS_ASSERT(amount <= framePushed_);
    if (amount)
        ma_add(Imm32(amount), sp);
    adjustFrame(-amount);
}
void
MacroAssemblerARMCompat::move32(const Imm32 &imm, const Register &dest)
{
    ma_mov(imm, dest);
}
void
MacroAssemblerARMCompat::move32(const Address &src, const Register &dest)
{
    movePtr(src, dest);
}
void
MacroAssemblerARMCompat::movePtr(const ImmWord &imm, const Register &dest)
{
    ma_mov(Imm32(imm.value), dest);
}
void
MacroAssemblerARMCompat::movePtr(const ImmGCPtr &imm, const Register &dest)
{
    writeDataRelocation(nextOffset());
    ma_mov(imm, dest);
}
void
MacroAssemblerARMCompat::movePtr(const Address &src, const Register &dest)
{
    loadPtr(src, dest);
}

void
MacroAssemblerARMCompat::load32(const Address &address, const Register &dest)
{
    loadPtr(address, dest);
}

void
MacroAssemblerARMCompat::load32(const ImmWord &imm, const Register &dest)
{
    loadPtr(imm, dest);
}

void
MacroAssemblerARMCompat::loadPtr(const Address &address, const Register &dest)
{
    ma_ldr(Operand(address), dest);
}
void
MacroAssemblerARMCompat::loadPtr(const ImmWord &imm, const Register &dest)
{
    movePtr(imm, ScratchRegister);
    loadPtr(Address(ScratchRegister, 0x0), dest);
}

void
MacroAssemblerARMCompat::store32(Register src, const ImmWord &imm)
{
    storePtr(src, imm);
}

void
MacroAssemblerARMCompat::storePtr(Register src, const Address &address)
{
    ma_str(src, Operand(address));
}

void
MacroAssemblerARMCompat::storePtr(Register src, const ImmWord &imm)
{
    movePtr(imm, ScratchRegister);
    storePtr(src, Address(ScratchRegister, 0x0));
}

void
MacroAssemblerARMCompat::cmp32(const Register &lhs, const Imm32 &rhs)
{
    ma_cmp(lhs, rhs);
}
void
MacroAssemblerARMCompat::cmpPtr(const Register &lhs, const ImmWord &rhs)
{
    ma_cmp(lhs, Imm32(rhs.value));
}

void
MacroAssemblerARMCompat::setStackArg(const Register &reg, uint32 arg)
{
    ma_dataTransferN(IsStore, 32, true, sp, Imm32(arg * STACK_SLOT_SIZE), reg);

}

void
MacroAssemblerARMCompat::subPtr(Imm32 imm, const Register dest)
{
    ma_sub(imm, dest);
}

void
MacroAssemblerARMCompat::addPtr(Imm32 imm, const Register dest)
{
    ma_add(imm, dest);
}

// higher level tag testing code
Assembler::Condition
MacroAssemblerARMCompat::compareDoubles(JSOp compare, FloatRegister lhs, FloatRegister rhs)
{
    ma_vcmp(lhs, rhs);
    as_vmrs(pc);
    switch (compare) {
      case JSOP_STRICTNE:
      case JSOP_NE:
        return Assembler::VFP_NotEqualOrUnordered;
      case JSOP_STRICTEQ:
      case JSOP_EQ:
        return Assembler::VFP_Equal;
      case JSOP_LT:
        return Assembler::VFP_LessThan;
      case JSOP_LE:
        return Assembler::VFP_LessThanOrEqual;
      case JSOP_GT:
        return Assembler::VFP_GreaterThan;
      case JSOP_GE:
        return Assembler::VFP_GreaterThanOrEqual;
      default:
        JS_NOT_REACHED("Unrecognized comparison operation");
        return Assembler::VFP_Unordered;
    }
}

Operand ToPayload(Operand base) {
    return Operand(Register::FromCode(base.base()),
                   base.disp());
}
Operand ToType(Operand base) {
    return Operand(Register::FromCode(base.base()),
                   base.disp() + sizeof(void *));
}

Operand payloadOf(const Address &address) {
    return Operand(address.base, address.offset);
}
Operand tagOf(const Address &address) {
    return Operand(address.base, address.offset + 4);
}

Assembler::Condition
MacroAssemblerARMCompat::testInt32(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(value.typeReg(), ImmType(JSVAL_TYPE_INT32));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testBoolean(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(value.typeReg(), ImmType(JSVAL_TYPE_BOOLEAN));
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testDouble(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    Assembler::Condition actual = (cond == Equal)
        ? Below
        : AboveOrEqual;
    ma_cmp(value.typeReg(), ImmTag(JSVAL_TAG_CLEAR));
    return actual;
}
Assembler::Condition
MacroAssemblerARMCompat::testNull(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(value.typeReg(), ImmType(JSVAL_TYPE_NULL));
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testUndefined(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(value.typeReg(), ImmType(JSVAL_TYPE_UNDEFINED));
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testString(Assembler::Condition cond, const ValueOperand &value)
{
    return testString(cond, value.typeReg());
}
Assembler::Condition
MacroAssemblerARMCompat::testObject(Assembler::Condition cond, const ValueOperand &value)
{
    return testObject(cond, value.typeReg());
}

    // register-based tests
Assembler::Condition
MacroAssemblerARMCompat::testInt32(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_INT32));
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testBoolean(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_BOOLEAN));
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testNull(Assembler::Condition cond, const Register &tag) {
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_NULL));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testUndefined(Assembler::Condition cond, const Register &tag) {
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_UNDEFINED));
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testString(Assembler::Condition cond, const Register &tag) {
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_STRING));
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testObject(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_TAG_OBJECT));
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testNumber(Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(tag, ImmTag(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET));
    return cond == Equal ? BelowOrEqual : Above;
}

// unboxing code
void
MacroAssemblerARMCompat::unboxInt32(const ValueOperand &operand, const Register &dest)
{
    ma_mov(operand.payloadReg(), dest);
}

void
MacroAssemblerARMCompat::unboxBoolean(const ValueOperand &operand, const Register &dest)
{
    ma_mov(operand.payloadReg(), dest);
}

void
MacroAssemblerARMCompat::unboxDouble(const ValueOperand &operand, const FloatRegister &dest)
{
    JS_ASSERT(dest != ScratchFloatReg);
    as_vxfer(operand.payloadReg(), operand.typeReg(),
             VFPRegister(dest), CoreToFloat);
}

void
MacroAssemblerARMCompat::unboxValue(const ValueOperand &src, AnyRegister dest)
{
    if (dest.isFloat()) {
        Label notInt32, end;
        branchTestInt32(Assembler::NotEqual, src, &notInt32);
        convertInt32ToDouble(src.payloadReg(), dest.fpu());
        ma_b(&end);
        bind(&notInt32);
        unboxDouble(src, dest.fpu());
        bind(&end);
    } else {
        if (src.payloadReg() != dest.gpr())
            as_mov(dest.gpr(), O2Reg(src.payloadReg()));
    }
}

void
MacroAssemblerARMCompat::boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest)
{
    VFPRegister d = VFPRegister(dest);
    ma_vimm(1.0, dest);
    ma_cmp(operand.payloadReg(), Imm32(0));
    as_vsub(d, d, d, NotEqual);
}

void
MacroAssemblerARMCompat::int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest)
{
    // transfer the integral value to a floating point register
    VFPRegister vfpdest = VFPRegister(dest);
    as_vxfer(operand.payloadReg(), InvalidReg,
             vfpdest.sintOverlay(), CoreToFloat);
    // convert the value to a double.
    as_vcvt(vfpdest, vfpdest.sintOverlay());
}

void
MacroAssemblerARMCompat::loadInt32OrDouble(const Operand &src, const FloatRegister &dest)
{
    Label notInt32, end;
    // If it's an int, convert it to double.
    ma_ldr(ToType(src), ScratchRegister);
    branchTestInt32(Assembler::NotEqual, ScratchRegister, &notInt32);
    ma_ldr(ToPayload(src), ScratchRegister);
    convertInt32ToDouble(ScratchRegister, dest);
    ma_b(&end);

    // Not an int, just load as double.
    bind(&notInt32);
    ma_vldr(src, dest);
    bind(&end);
}

void
MacroAssemblerARMCompat::loadInt32OrDouble(Register base, Register index, const FloatRegister &dest, int32 shift)
{
    Label notInt32, end;

    JS_STATIC_ASSERT(NUNBOX32_PAYLOAD_OFFSET == 0);

    // If it's an int, convert it to double.
    ma_alu(base, lsl(index, shift), ScratchRegister, op_add);

    // Since we only have one scratch register, we need to stomp over it with the tag
    ma_ldr(Address(ScratchRegister, NUNBOX32_TYPE_OFFSET), ScratchRegister);
    branchTestInt32(Assembler::NotEqual, ScratchRegister, &notInt32);

    // Implicitly requires NUNBOX32_PAYLOAD_OFFSET == 0: no offset provided
    ma_ldr(DTRAddr(base, DtrRegImmShift(index, LSL, shift)), ScratchRegister);
    convertInt32ToDouble(ScratchRegister, dest);
    ma_b(&end);

    // Not an int, just load as double.
    bind(&notInt32);
    // First, recompute the offset that had been stored in the scratch register
    // since the scratch register was overwritten loading in the type.
    ma_alu(base, lsl(index, shift), ScratchRegister, op_add);
    ma_vldr(Address(ScratchRegister, 0), dest);
    bind(&end);
}

void
MacroAssemblerARMCompat::loadStaticDouble(const double *dp, const FloatRegister &dest)
{
    ma_mov(Imm32((uint32)dp), ScratchRegister);
    as_vdtr(IsLoad, dest, VFPAddr(ScratchRegister, VFPOffImm(0)));
#if 0
    _vldr()
        movsd(dp, dest);
#endif
}
    // treat the value as a boolean, and set condition codes accordingly

Assembler::Condition
MacroAssemblerARMCompat::testInt32Truthy(bool truthy, const ValueOperand &operand)
{
    ma_tst(operand.payloadReg(), operand.payloadReg());
    return truthy ? NonZero : Zero;
}

Assembler::Condition
MacroAssemblerARMCompat::testBooleanTruthy(bool truthy, const ValueOperand &operand)
{
    ma_tst(operand.payloadReg(), operand.payloadReg());
    return truthy ? NonZero : Zero;
}

Assembler::Condition
MacroAssemblerARMCompat::testDoubleTruthy(bool truthy, const FloatRegister &reg)
{
    as_vcmpz(VFPRegister(reg));
    as_vmrs(pc);
    as_cmp(r0, O2Reg(r0), Overflow);
    return truthy ? NonZero : Zero;
}

Register
MacroAssemblerARMCompat::extractObject(const Address &address, Register scratch)
{
    ma_ldr(payloadOf(address), scratch);
    return scratch;
}
Register
MacroAssemblerARMCompat::extractTag(const Address &address, Register scratch)
{
    ma_ldr(tagOf(address), scratch);
    return scratch;
}
void
MacroAssemblerARMCompat::moveValue(const Value &val, Register type, Register data) {
    jsval_layout jv = JSVAL_TO_IMPL(val);
    ma_mov(Imm32(jv.s.tag), type);
    ma_mov(Imm32(jv.s.payload.i32), data);
}
void
MacroAssemblerARMCompat::moveValue(const Value &val, const ValueOperand &dest) {
    moveValue(val, dest.typeReg(), dest.payloadReg());
}

/////////////////////////////////////////////////////////////////
// X86/X64-common (ARM too now) interface.
/////////////////////////////////////////////////////////////////
void
MacroAssemblerARMCompat::storeValue(ValueOperand val, Operand dst) {
    ma_str(val.payloadReg(), ToPayload(dst));
    ma_str(val.typeReg(), ToType(dst));
}

void
MacroAssemblerARMCompat::storeValue(ValueOperand val, Register base, Register index, int32 shift)
{
    if (isValueDTRDCandidate(val)) {
        Register tmpIdx;
        if (shift == 0) {
            tmpIdx = index;
        } else if (shift < 0) {
            ma_asr(Imm32(-shift), index, ScratchRegister);
            tmpIdx = ScratchRegister;
        } else {
            ma_lsl(Imm32(shift), index, ScratchRegister);
            tmpIdx = ScratchRegister;
        }
        ma_strd(val.payloadReg(), val.typeReg(), EDtrAddr(base, EDtrOffReg(tmpIdx)));
    } else {
        // The ideal case is the base is dead so the sequence:
        // str val.payloadReg(), [base, index LSL shift]!
        // str val.typeReg(), [base, #4]
        // only clobbers dead registers
        // Sadly, this information is not available, so the scratch register is necessary.
        ma_alu(base, lsl(index, shift), ScratchRegister, op_add);

        // This case is handled by a simpler function.
        storeValue(val, Address(ScratchRegister, 0));
    }
}

void
MacroAssemblerARMCompat::loadValue(Register base, Register index, ValueOperand val, int32 shift)
{
    if (isValueDTRDCandidate(val)) {
        Register tmpIdx;
        if (shift == 0) {
            tmpIdx = index;
        } else if (shift < 0) {
            ma_asr(Imm32(-shift), index, ScratchRegister);
            tmpIdx = ScratchRegister;
        } else {
            ma_lsl(Imm32(shift), index, ScratchRegister);
            tmpIdx = ScratchRegister;
        }
        ma_ldrd(EDtrAddr(base, EDtrOffReg(tmpIdx)), val.payloadReg(), val.typeReg());
    } else {
        // The ideal case is the base is dead so the sequence:
        // ldr val.payloadReg(), [base, index LSL shift]!
        // ldr val.typeReg(), [base, #4]
        // only clobbers dead registers
        // Sadly, this information is not available, so the scratch register is necessary.
        ma_alu(base, lsl(index, shift), ScratchRegister, op_add);

        // This case is handled by a simpler function.
        loadValue(Address(ScratchRegister, 0), val);
    }
}

void
MacroAssemblerARMCompat::loadValue(Address src, ValueOperand val)
{
    Operand srcOp = Operand(src);
    Operand payload = ToPayload(srcOp);
    Operand type = ToType(srcOp);
    // TODO: copy this code into a generic function that acts on all sequences of memory accesses
    if (isValueDTRDCandidate(val)) {
        // If the value we want is in two consecutive registers starting with an even register,
        // they can be combined as a single ldrd.
        int offset = srcOp.disp();
        if (offset < 256 && offset > -256) {
            ma_ldrd(EDtrAddr(Register::FromCode(srcOp.base()), EDtrOffImm(srcOp.disp())), val.payloadReg(), val.typeReg());
            return;
        }
    }
    // if the value is lower than the type, then we may be able to use an ldm instruction

    if (val.payloadReg().code() < val.typeReg().code()) {
        if (srcOp.disp() <= 4 && srcOp.disp() >= -8 && (srcOp.disp() & 3) == 0) {
            // turns out each of the 4 value -8, -4, 0, 4 corresponds exactly with one of
            // LDM{DB, DA, IA, IB}
            DTMMode mode;
            switch(srcOp.disp()) {
              case -8:
                mode = DB;
                break;
              case -4:
                mode = DA;
                break;
              case 0:
                mode = IA;
                break;
              case 4:
                mode = IB;
                break;
              default:
                JS_NOT_REACHED("Bogus Offset for LoadValue as DTM");
            }
            startDataTransferM(IsLoad, Register::FromCode(srcOp.base()), mode);
            transferReg(val.payloadReg());
            transferReg(val.typeReg());
            finishDataTransfer();
            return;
        }
    }
    // Ensure that loading the payload does not erase the pointer to the
    // Value in memory.
    if (Register::FromCode(type.base()) != val.payloadReg()) {
        ma_ldr(payload, val.payloadReg());
        ma_ldr(type, val.typeReg());
    } else {
        ma_ldr(type, val.typeReg());
        ma_ldr(payload, val.payloadReg());
    }
}
void
MacroAssemblerARMCompat::pushValue(ValueOperand val) {
    ma_push(val.typeReg());
    ma_push(val.payloadReg());
}
void
MacroAssemblerARMCompat::popValue(ValueOperand val) {
    ma_pop(val.payloadReg());
    ma_pop(val.typeReg());
}
void
MacroAssemblerARMCompat::storePayload(const Value &val, Operand dest)
{
    jsval_layout jv = JSVAL_TO_IMPL(val);
    if (val.isMarkable()) {
        ma_mov(ImmGCPtr((gc::Cell *)jv.s.payload.ptr), ScratchRegister);
    } else {
        ma_mov(Imm32(jv.s.payload.i32), ScratchRegister);
    }
    ma_str(ScratchRegister, ToPayload(dest));
}
void
MacroAssemblerARMCompat::storePayload(Register src, Operand dest)
{
    if (dest.getTag() == Operand::MEM) {
        ma_str(src, ToPayload(dest));
        return;
    }
    JS_NOT_REACHED("why do we do all of these things?");

}

void
MacroAssemblerARMCompat::storePayload(const Value &val, Register base, Register index, int32 shift)
{
    jsval_layout jv = JSVAL_TO_IMPL(val);
    if (val.isMarkable()) {
        ma_mov(ImmGCPtr((gc::Cell *)jv.s.payload.ptr), ScratchRegister);
    } else {
        ma_mov(Imm32(jv.s.payload.i32), ScratchRegister);
    }
    JS_STATIC_ASSERT(NUNBOX32_PAYLOAD_OFFSET == 0);
    // If NUNBOX32_PAYLOAD_OFFSET is not zero, the memory operand [base + index << shift + imm]
    // cannot be encoded into a single instruction, and cannot be integrated into the as_dtr call.
    as_dtr(IsStore, 32, Offset, ScratchRegister, DTRAddr(base, DtrRegImmShift(index, LSL, shift)));
}
void
MacroAssemblerARMCompat::storePayload(Register src, Register base, Register index, int32 shift)
{
    JS_ASSERT((shift < 32) && (shift >= 0));
    // If NUNBOX32_PAYLOAD_OFFSET is not zero, the memory operand [base + index << shift + imm]
    // cannot be encoded into a single instruction, and cannot be integrated into the as_dtr call.
    JS_STATIC_ASSERT(NUNBOX32_PAYLOAD_OFFSET == 0);
    // Technically, shift > -32 can be handle by changing LSL to ASR, but should never come up,
    // and this is one less code path to get wrong.
    as_dtr(IsStore, 32, Offset, src, DTRAddr(base, DtrRegImmShift(index, LSL, shift)));
}

void
MacroAssemblerARMCompat::storeTypeTag(ImmTag tag, Operand dest) {
    if (dest.getTag() == Operand::MEM) {
        ma_mov(tag, ScratchRegister);
        ma_str(ScratchRegister, ToType(dest));
        return;
    }

    JS_NOT_REACHED("why do we do all of these things?");

}

void
MacroAssemblerARMCompat::storeTypeTag(ImmTag tag, Register base, Register index, int32 shift) {
    JS_ASSERT(base != ScratchRegister);
    JS_ASSERT(index != ScratchRegister);
    // A value needs to be store a value int base + index << shift + 4.
    // Arm cannot handle this in a single operand, so a temp register is required.
    // However, the scratch register is presently in use to hold the immediate that
    // is being stored into said memory location. Work around this by modifying
    // the base so the valid [base + index << shift] format can be used, then
    // restore it.
    ma_add(base, Imm32(NUNBOX32_TYPE_OFFSET), base);
    ma_mov(tag, ScratchRegister);
    ma_str(ScratchRegister, DTRAddr(base, DtrRegImmShift(index, LSL, shift)));
    ma_sub(base, Imm32(NUNBOX32_TYPE_OFFSET), base);
}

void
MacroAssemblerARMCompat::linkExitFrame() {
    movePtr(ImmWord(GetIonContext()->cx->runtime), ScratchRegister);
    ma_str(StackPointer, Operand(ScratchRegister, offsetof(JSRuntime, ionTop)));
}

// ARM says that all reads of pc will return 8 higher than the
// address of the currently executing instruction.  This means we are
// correctly storing the address of the instruction after the call
// in the register.
// Also ION is breaking the ARM EABI here (sort of). The ARM EABI
// says that a function call should move the pc into the link register,
// then branch to the function, and *sp is data that is owned by the caller,
// not the callee.  The ION ABI says *sp should be the address that
// we will return to when leaving this function
void
MacroAssemblerARM::ma_callIon(const Register r)
{
    // When the stack is 8 byte aligned,
    // we want to decrement sp by 8, and write pc+8 into the new sp.
    // when we return from this call, sp will be its present value minus 4.
    as_dtr(IsStore, 32, PreIndex, pc, DTRAddr(sp, DtrOffImm(-8)));
    as_blx(r);
}
void
MacroAssemblerARM::ma_callIonNoPush(const Register r)
{
    // Since we just write the return address into the stack, which is
    // popped on return, the net effect is removing 4 bytes from the stack
    as_dtr(IsStore, 32, Offset, pc, DTRAddr(sp, DtrOffImm(0)));
    as_blx(r);
}
void
MacroAssemblerARM::ma_callIonHalfPush(const Register r)
{
    // The stack is unaligned by 4 bytes.
    // We push the pc to the stack to align the stack before the call, when we
    // return the pc is poped and the stack is restored to its unaligned state.
    ma_push(pc);
    as_blx(r);
}

void
MacroAssemblerARM::ma_call(void *dest)
{
    ma_mov(Imm32((uint32)dest), CallReg);
    as_blx(CallReg);
}

void
MacroAssemblerARMCompat::breakpoint()
{
    as_bkpt();
}

uint32
MacroAssemblerARMCompat::setupABICall(uint32 args)
{
    JS_ASSERT(!inCall_);
    inCall_ = true;

    uint32 stackForArgs = args > NumArgRegs
                          ? (args - NumArgRegs) * sizeof(void *)
                          : 0;
    return stackForArgs;
}

void
MacroAssemblerARMCompat::setupAlignedABICall(uint32 args)
{
    // Find the total number of bytes the stack will have been adjusted by,
    // in order to compute alignment. Include a stack slot for saving the stack
    // pointer, which will be dynamically aligned.
    uint32 stackForCall = setupABICall(args);
    uint32 displacement = stackForCall + framePushed_;

    // framePushed_ is accurate, so precisely adjust the stack requirement.
    stackAdjust_ = stackForCall + ComputeByteAlignment(displacement, StackAlignment);
    dynamicAlignment_ = false;
    reserveStack(stackAdjust_);
}

void
MacroAssemblerARMCompat::setupUnalignedABICall(uint32 args, const Register &scratch)
{
    uint32 stackForCall = setupABICall(args);
    uint32 displacement = stackForCall + STACK_SLOT_SIZE;

    // Find the total number of bytes the stack will have been adjusted by,
    // in order to compute alignment. framePushed_ is bogus or we don't know
    // it for sure, so instead, save the original value of esp and then chop
    // off its low bits. Then, we push the original value of esp.
    ma_mov(sp, scratch);
    ma_and(Imm32(~(StackAlignment - 1)), sp, sp);
    ma_push(scratch);
    stackAdjust_ = stackForCall + ComputeByteAlignment(displacement, StackAlignment);
    dynamicAlignment_ = true;
    reserveStack(stackAdjust_);
}

void
MacroAssemblerARMCompat::setABIArg(uint32 arg, const MoveOperand &from)
{
    MoveOperand to;
    Register dest;
    if (GetArgReg(arg, &dest)) {
        to = MoveOperand(dest);
    } else {
        // There is no register for this argument, so just move it to its
        // stack slot immediately.
        uint32 disp = GetArgStackDisp(arg);
        to = MoveOperand(StackPointer, disp);
    }
    enoughMemory_ &= moveResolver_.addMove(from, to, Move::GENERAL);
}

void
MacroAssemblerARMCompat::setABIArg(uint32 arg, const Register &reg)
{
    setABIArg(arg, MoveOperand(reg));
}

void MacroAssemblerARMCompat::checkStackAlignment()
{
#ifdef DEBUG
        Label good;
        ma_tst(Imm32(StackAlignment - 1), sp);
        ma_b(&good, Equal);
        breakpoint();
        bind(&good);
#endif
}

void
MacroAssemblerARMCompat::callWithABI(void *fun)
{
    JS_ASSERT(inCall_);

    // Position all arguments.
    {
        enoughMemory_ &= moveResolver_.resolve();
        if (!enoughMemory_)
            return;

        MoveEmitter emitter(*this);
        emitter.emit(moveResolver_);
        emitter.finish();
    }

    checkStackAlignment();
    ma_call(fun);

    freeStack(stackAdjust_);
    if (dynamicAlignment_) {
        // x86 supports pop esp.  on arm, that isn't well defined, so just
        // do it manually
        as_dtr(IsLoad, 32, Offset, sp, DTRAddr(sp, DtrOffImm(0)));
    }

    JS_ASSERT(inCall_);
    inCall_ = false;
}

void
MacroAssemblerARMCompat::handleException()
{
    // Reserve space for exception information.
    int size = (sizeof(ResumeFromException) + 7) & ~7;
    ma_sub(Imm32(size), sp);
    ma_mov(sp, r0);

    // Ask for an exception handler.
    setupAlignedABICall(1);
    setABIArg(0, r0);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, ion::HandleException));
    // Load the error value, load the new stack pointer, and return.
    moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    ma_ldr(Operand(sp, offsetof(ResumeFromException, stackPointer)), sp);

    // We're going to be returning by the ion calling convention, which returns
    // by ??? (for now, I think ldr pc, [sp]!)
    as_dtr(IsLoad, 32, PostIndex, pc, DTRAddr(sp, DtrOffImm(4)));
    //ret();
}
