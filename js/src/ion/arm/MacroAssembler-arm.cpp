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
using namespace js;
using namespace ion;
void
MacroAssemblerARM::convertInt32ToDouble(const Register &src, const FloatRegister &dest)
{
    // direct conversions aren't possible.
    as_vxfer(src, InvalidReg, VFPRegister(dest, VFPRegister::Single),
             CoreToFloat);
    as_vcvt(VFPRegister(dest, VFPRegister::Double),
            VFPRegister(dest, VFPRegister::Single));
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
    ma_mov(Imm32(ptr.value), dest);
    //JS_NOT_REACHED("todo:make gc more sane.");
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
MacroAssemblerARM::ma_adc(Imm32 imm, Register dest, SetCond_ sc)
{
    ma_alu(dest, imm, dest, op_adc, sc);
}
void
MacroAssemblerARM::ma_adc(Register src, Register dest, SetCond_ sc)
{
    as_alu(dest, dest, O2Reg(src), op_adc, sc);
}
void
MacroAssemblerARM::ma_adc(Register src1, Register src2, Register dest, SetCond_ sc)
{
    as_alu(dest, src1, O2Reg(src2), op_adc, sc);
}

    // add
void
MacroAssemblerARM::ma_add(Imm32 imm, Register dest, SetCond_ sc)
{
    ma_alu(dest, imm, dest, op_add, sc);
}
void
MacroAssemblerARM::ma_add(Register src1, Register dest, SetCond_ sc)
{
    as_alu(dest, dest, O2Reg(src1), op_add, sc);
}
void
MacroAssemblerARM::ma_add(Register src1, Register src2, Register dest, SetCond_ sc)
{
    as_alu(dest, src1, O2Reg(dest), op_add, sc);
}
void
MacroAssemblerARM::ma_add(Register src1, Operand op, Register dest, SetCond_ sc)
{
    ma_alu(src1, op, dest, op_add, sc);
}
void
MacroAssemblerARM::ma_add(Register src1, Imm32 op, Register dest, SetCond_ sc)
{
    ma_alu(src1, op, dest, op_add, sc);
}

    // subtract with carry
void
MacroAssemblerARM::ma_sbc(Imm32 imm, Register dest, SetCond_ sc)
{
    ma_alu(dest, imm, dest, op_sbc, sc);
}
void
MacroAssemblerARM::ma_sbc(Register src1, Register dest, SetCond_ sc)
{
    as_alu(dest, dest, O2Reg(src1), op_sbc, sc);
}
void
MacroAssemblerARM::ma_sbc(Register src1, Register src2, Register dest, SetCond_ sc)
{
    as_alu(dest, src1, O2Reg(dest), op_sbc, sc);
}

    // subtract
void
MacroAssemblerARM::ma_sub(Imm32 imm, Register dest, SetCond_ sc)
{
    ma_alu(dest, imm, dest, op_sub, sc);
}
void
MacroAssemblerARM::ma_sub(Register src1, Register dest, SetCond_ sc)
{
    ma_alu(dest, Operand(O2Reg(src1)), dest, op_sub, sc);
}
void
MacroAssemblerARM::ma_sub(Register src1, Register src2, Register dest, SetCond_ sc)
{
    ma_alu(src1, Operand(O2Reg(src2)), dest, op_sub, sc);
}
void
MacroAssemblerARM::ma_sub(Register src1, Operand op, Register dest, SetCond_ sc)
{
    ma_alu(src1, op, dest, op_sub, sc);
}
void
MacroAssemblerARM::ma_sub(Register src1, Imm32 op, Register dest, SetCond_ sc)
{
    ma_alu(src1, op, dest, op_sub, sc);
}

    // reverse subtract
void
MacroAssemblerARM::ma_rsb(Imm32 imm, Register dest, SetCond_ sc)
{
    ma_alu(dest, imm, dest, op_rsb, sc);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Register dest, SetCond_ sc)
{
    as_alu(dest, dest, O2Reg(src1), op_add, sc);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Register src2, Register dest, SetCond_ sc)
{
    as_alu(dest, src1, O2Reg(dest), op_rsc, sc);
}
void
MacroAssemblerARM::ma_rsb(Register src1, Imm32 op2, Register dest, SetCond_ sc)
{
    ma_alu(src1, op2, dest, op_rsb, sc);
}

    // reverse subtract with carry
void
MacroAssemblerARM::ma_rsc(Imm32 imm, Register dest, SetCond_ sc)
{
    ma_alu(dest, imm, dest, op_rsc, sc);
}
void
MacroAssemblerARM::ma_rsc(Register src1, Register dest, SetCond_ sc)
{
    as_alu(dest, dest, O2Reg(src1), op_rsc, sc);
}
void
MacroAssemblerARM::ma_rsc(Register src1, Register src2, Register dest, SetCond_ sc)
{
    as_alu(dest, src1, O2Reg(dest), op_rsc, sc);
}

    // compares/tests
    // compare negative (sets condition codes as src1 + src2 would)
void
MacroAssemblerARM::ma_cmn(Imm32 imm, Register src1)
{
    ma_alu(src1, imm, InvalidReg, op_cmn);
}
void
MacroAssemblerARM::ma_cmn(Register src1, Register src2)
{
    as_alu(InvalidReg, src2, O2Reg(src1), op_cmn);
}
void
MacroAssemblerARM::ma_cmn(Register src1, Operand op)
{
    JS_NOT_REACHED("Feature NYI");
}

// compare (src - src2)
void
MacroAssemblerARM::ma_cmp(Imm32 imm, Register src1)
{
    ma_alu(src1, imm, InvalidReg, op_cmp, SetCond);
}
void
MacroAssemblerARM::ma_cmp(Register src1, Operand op)
{
    as_cmp(src1, op.toOp2());
}
void
MacroAssemblerARM::ma_cmp(Register src1, Register src2)
{
    as_cmp(src2, O2Reg(src1));
}

    // test for equality, (src1^src2)
void
MacroAssemblerARM::ma_teq(Imm32 imm, Register src1)
{
    ma_alu(src1, imm, InvalidReg, op_teq, SetCond);
}
void
MacroAssemblerARM::ma_teq(Register src2, Register src1)
{
    as_tst(src2, O2Reg(src1));
}
void
MacroAssemblerARM::ma_teq(Register src1, Operand op)
{
    JS_NOT_REACHED("Feature NYI");
}


// test (src1 & src2)
void
MacroAssemblerARM::ma_tst(Imm32 imm, Register src1)
{
    ma_alu(src1, imm, InvalidReg, op_tst, SetCond);
}
void
MacroAssemblerARM::ma_tst(Register src1, Register src2)
{
    as_tst(src1, O2Reg(src2));
}
void
MacroAssemblerARM::ma_tst(Register src1, Operand op)
{
    as_tst(src1, op.toOp2());
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
MacroAssemblerARM::ma_ldr(DTRAddr addr, Register rt, Index mode, Condition cc)
{
    as_dtr(IsLoad, 32, mode, rt, addr, cc);
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

// specialty for moving N bits of data, where n == 8,16,32,64
void
MacroAssemblerARM::ma_dataTransferN(LoadStore ls, int size,
                          Register rn, Register rm, Register rt,
                          Index mode, Assembler::Condition cc)
{
    JS_NOT_REACHED("Feature NYI");
}

void
MacroAssemblerARM::ma_dataTransferN(LoadStore ls, int size,
                          Register rn, Imm32 offset, Register rt,
                          Index mode, Assembler::Condition cc)
{
    JS_NOT_REACHED("Feature NYI");
}
void
MacroAssemblerARM::ma_pop(Register r)
{
    ma_dtr(IsLoad, sp, Imm32(4), r, PostIndex);
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
void
MacroAssemblerARM::ma_b(void *target, Relocation::Kind reloc)
{
    // TODO: do the right thing with the reloc.
    ma_b(target, Always, reloc);
}
void
MacroAssemblerARM::ma_b(void *target, Assembler::Condition c, Relocation::Kind reloc)
{
    // we know the absolute address of the target, but not our final
    // location (with relocating GC, we *can't* know our final location)
    // for now, I'm going to be conservative, and load this with an
    // absolute address
    uint32 trg = (uint32)target;
    as_movw(ScratchRegister, Imm16(trg & 0xffff), c);
    as_movt(ScratchRegister, Imm16(trg >> 16), c);
    // this is going to get the branch predictor pissed off.
    as_bx(ScratchRegister, c);
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
MacroAssemblerARM::ma_vmul(FloatRegister src1, FloatRegister src2, FloatRegister dst)
{
    as_vmul(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
}
void
MacroAssemblerARM::ma_vcmp_F64(FloatRegister src1, FloatRegister src2)
{
    as_vcmp(VFPRegister(src1), VFPRegister(src2));
}
void
MacroAssemblerARM::ma_vcvt_F64_I32(FloatRegister src, FloatRegister dest)
{
    as_vcvt(VFPRegister(src), VFPRegister(dest).sintOverlay());
}
void
MacroAssemblerARM::ma_vcvt_I32_F64(FloatRegister src, FloatRegister dest)
{
    as_vcvt(VFPRegister(src).sintOverlay(), VFPRegister(dest));
}
void
MacroAssemblerARM::ma_vxfer(FloatRegister src, Register dest)
{
    as_vxfer(dest, InvalidReg, VFPRegister(src).singleOverlay(), FloatToCore);
}
void
MacroAssemblerARM::ma_vldr(VFPAddr addr, FloatRegister dest)
{
    as_vdtr(IsLoad, dest, addr);
}
void
MacroAssemblerARM::ma_vstr(FloatRegister src, VFPAddr addr)
{
    as_vdtr(IsStore, src, addr);
}

uint32
MacroAssemblerARM::alignStackForCall(uint32 stackForArgs)
{
    // framePushed_ is accurate, so precisely adjust the stack requirement.
    uint32 displacement = stackForArgs + framePushed_;
    return stackForArgs + ComputeByteAlignment(displacement, StackAlignment);
}

uint32
MacroAssemblerARM::dynamicallyAlignStackForCall(uint32 stackForArgs, const Register &scratch)
{
    // framePushed_ is bogus or we don't know it for sure, so instead, save
    // the original value of esp and then chop off its low bits. Then, we
    // push the original value of esp.

    JS_NOT_REACHED("Codegen for dynamicallyAlignedStackForCall NYI");
#if 0
    ma_mov(sp, scratch);
    ma_bic(Imm32(StackAlignment - 1), sp);
    Push(scratch);
#endif
    uint32 displacement = stackForArgs + STACK_SLOT_SIZE;
    return stackForArgs + ComputeByteAlignment(displacement, StackAlignment);
}

void
MacroAssemblerARM::restoreStackFromDynamicAlignment()
{
    // x86 supports pop esp.  on arm, that isn't well defined, so just
    //  do it manually
    as_dtr(IsLoad, 32, Offset, sp, DTRAddr(sp, DtrOffImm(0)));
}

void
MacroAssemblerARM::reserveStack(uint32 amount)
{
    if (amount)
        ma_sub(Imm32(amount), sp);
    framePushed_ += amount;
}
void
MacroAssemblerARM::freeStack(uint32 amount)
{
    JS_ASSERT(amount <= framePushed_);
    if (amount)
        ma_add(Imm32(amount), sp);
    framePushed_ -= amount;
}
void
MacroAssemblerARM::movePtr(ImmWord imm, const Register dest)
{
    ma_mov(Imm32(imm.value), dest);
}
void
MacroAssemblerARM::movePtr(ImmGCPtr imm, const Register dest)
{
    ma_mov(imm, dest);
}

void
MacroAssemblerARM::loadPtr(const Address &address, Register dest)
{
    ma_ldr(DTRAddr(address.base, DtrOffImm(address.offset)), dest);
}
void
MacroAssemblerARM::setStackArg(const Register &reg, uint32 arg)
{
    ma_dataTransferN(IsStore, 32, sp, Imm32(arg * STACK_SLOT_SIZE), reg);

}
#ifdef DEBUG
void
MacroAssemblerARM::checkCallAlignment()
{
    Label good;
    ma_tst(Imm32(StackAlignment - 1), sp);
    ma_b(&good, Equal);
    breakpoint();
    bind(&good);
}
#endif

    // higher level tag testing code
Assembler::Condition
MacroAssemblerARMCompat::testInt32(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(ImmType(JSVAL_TYPE_INT32), value.typeReg());
    return cond;
}

Assembler::Condition
MacroAssemblerARMCompat::testBoolean(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(ImmType(JSVAL_TYPE_BOOLEAN), value.typeReg());
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testDouble(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    Assembler::Condition actual = (cond == Equal)
        ? Below
        : AboveOrEqual;
    ma_cmp(ImmTag(JSVAL_TAG_CLEAR), value.typeReg());
    return actual;
}
Assembler::Condition
MacroAssemblerARMCompat::testNull(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(ImmType(JSVAL_TYPE_NULL), value.typeReg());
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testUndefined(Assembler::Condition cond, const ValueOperand &value)
{
    JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
    ma_cmp(ImmType(JSVAL_TYPE_UNDEFINED), value.typeReg());
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
    ma_cmp(ImmTag(JSVAL_TAG_INT32), tag);
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testBoolean(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(ImmTag(JSVAL_TAG_BOOLEAN), tag);
    return cond;
}
Assembler::Condition
MacroAssemblerARMCompat::testNull(Assembler::Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_NULL), tag);
        return cond;
    }

Assembler::Condition
MacroAssemblerARMCompat::testUndefined(Assembler::Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_UNDEFINED), tag);
        return cond;
    }
Assembler::Condition
MacroAssemblerARMCompat::testString(Assembler::Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_STRING), tag);
        return cond;
    }

Assembler::Condition
MacroAssemblerARMCompat::testObject(Assembler::Condition cond, const Register &tag)
{
    JS_ASSERT(cond == Equal || cond == NotEqual);
    ma_cmp(ImmTag(JSVAL_TAG_OBJECT), tag);
    return cond;
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
MacroAssemblerARMCompat::boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest)
{
    JS_NOT_REACHED("Codegen for boolValueToDouble NYI");
#if 0
    cvtsi2sd(operand.payloadReg(), dest);
#endif
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
MacroAssemblerARMCompat::loadStaticDouble(const double *dp, const FloatRegister &dest)
{
    JS_NOT_REACHED("Codegen for loadStaticDouble NYI");
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
    return truthy ? NonZero : Zero;
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
    // The stack is presently 8 byte aligned
    // We want to decrement sp by 8, and write pc+8 into the new sp

    as_dtr(IsStore, 32, PreIndex, pc, DTRAddr(sp, DtrOffImm(-8)));
    as_blx(r);
}
void
MacroAssemblerARM::ma_callIonNoPush(const Register r)
{
    as_dtr(IsStore, 32, Offset, pc, DTRAddr(sp, DtrOffImm(0)));
    as_blx(r);
}
void
MacroAssemblerARM::ma_callIonHalfPush(const Register r)
{
    ma_push(pc);
    as_blx(r);
}

void
MacroAssemblerARM::breakpoint() {
    as_bkpt();
}
