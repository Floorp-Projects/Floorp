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

#ifndef jsion_macro_assembler_arm_h__
#define jsion_macro_assembler_arm_h__

#include "ion/arm/Assembler-arm.h"

namespace js {
namespace ion {

static Register CallReg = ip;

// MacroAssemblerARM is inheriting form Assembler defined in Assembler-arm.{h,cpp}
class MacroAssemblerARM : public Assembler
{
protected:
    // Extra bytes currently pushed onto the frame beyond frameDepth_. This is
    // needed to compute offsets to stack slots while temporary space has been
    // reserved for unexpected spills or C++ function calls. It is maintained
    // by functions which track stack alignment, which for clear distinction
    // use StudlyCaps (for example, Push, Pop).
    uint32 framePushed_;

public:
    MacroAssemblerARM()
      : framePushed_(0)
    { }


    void convertInt32ToDouble(const Register &src, const FloatRegister &dest) {
        // direct conversions aren't possible.
        as_vxfer(src, InvalidReg, VFPRegister(dest, VFPRegister::Single),
                 CoreToFloat);
        as_vcvt(VFPRegister(dest, VFPRegister::Double),
                VFPRegister(dest, VFPRegister::Single));
    }



    uint32 framePushed() const {
        return framePushed_;
    }

    // For maximal awesomeness, 8 should be sufficent.
    static const uint32 StackAlignment = 8;
    // somewhat direct wrappers for the low-level assembler funcitons
    // bitops
    // attempt to encode a virtual alu instruction using
    // two real instructions.

    bool alu_dbl(Register src1, Imm32 imm, Register dest, ALUOp op,
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

    void ma_alu(Register src1, Imm32 imm, Register dest,
                ALUOp op,
                SetCond_ sc =  NoSetCond, Condition c = Always)
    {
        // the operator gives us the ability to determine how
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
            //  move the bits into the destination.  Otherwise, we'll need to
            // fall back on a multi-instruction format :(
            // movw/movt don't set condition codes, so don't hold your breath.
            if (sc == NoSetCond && (op == op_mov || op == op_mvn)) {
                // ARMv7 supports movw/movt. movw zero-extends
                // its 16 bit argument, so we can set the register
                // this way.
                // movt leaves the bottom 16 bits in tact, so
                // it is unsuitable to move a constant that
                if (op == op_mov && imm.value <= 0xffff) {
                    JS_ASSERT(src1 == InvalidReg);
                    as_movw(dest, (uint16)imm.value, c);
                    return;
                }
                // if they asked for a mvn rfoo, imm, where ~imm fits into 16 bits
                // then do it.
                if (op == op_mvn && ~imm.value <= 0xffff) {
                    JS_ASSERT(src1 == InvalidReg);
                    as_movw(dest, (uint16)-imm.value, c);
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
            // both operations should take 1 cycle, where as add dest, tmp ror 4
            // takes two cycles to execute.
        }
        // either a) this isn't ARMv7 b) this isn't a move
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
        // and try with its negative.
        if (negOp != op_invalid &&
            alu_dbl(src1, negImm, dest, negOp, sc, c))
            return;
        // well, damxn. We can use two 16 bit mov's, then do the op
        // or we can do a single load from a pool then op.
        if (hasMOVWT()) {
            // try to load the immediate into a scratch register
            // then use that
            as_movw(ScratchRegister, imm.value & 0xffff, c);
            as_movt(ScratchRegister, (imm.value >> 16) & 0xffff, c);
        } else {
            JS_NOT_REACHED("non-ARMv7 loading of immediates NYI.");
        }
        as_alu(dest, src1, O2Reg(ScratchRegister), op, sc, c);
        // done!
    }

    void ma_alu(Register src1, Operand op2, Register dest, ALUOp op,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        JS_ASSERT(op2.getTag() == Operand::OP2);
        as_alu(dest, src1, op2.toOp2(), op, sc, c);
    }

    // These should likely be wrapped up as a set of macros
    // or something like that.  I cannot think of a good reason
    // to explicitly have all of this code.
    // ALU based ops
    // mov
    void ma_mov(Register src, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        as_mov(dest, O2Reg(src), sc, c);
    }
    void ma_mov(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(InvalidReg, imm, dest, op_mov, sc, c);
    }
    void ma_mov(const ImmGCPtr &ptr, Register dest) {
        ma_mov(Imm32(ptr.value), dest);
        JS_NOT_REACHED("todo:make gc more sane.");
    }

    // shifts (just a move with a shifting op2)
    void ma_lsl(Imm32 shift, Register src, Register dst) {
        as_mov(dst, lsl(src, shift.value));
    }
    void ma_lsr(Imm32 shift, Register src, Register dst) {
        as_mov(dst, lsr(src, shift.value));
    }
    void ma_asr(Imm32 shift, Register src, Register dst) {
        as_mov(dst, asr(src, shift.value));
    }
    void ma_ror(Imm32 shift, Register src, Register dst) {
        as_mov(dst, ror(src, shift.value));
    }
    void ma_rol(Imm32 shift, Register src, Register dst) {
        as_mov(dst, rol(src, shift.value));
    }

    // move not (dest <- ~src)
    void ma_mvn(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(InvalidReg, imm, dest, op_mvn, sc, c);
    }

    void ma_mvn(Register src1, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        as_alu(dest, InvalidReg, O2Reg(src1), op_mvn, sc, c);
    }

    // and
    void ma_and(Register src, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_and(dest, src, dest);
    }
    void ma_and(Register src1, Register src2, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        as_and(dest, src1, O2Reg(src2), sc, c);
    }
    void ma_and(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(dest, imm, dest, op_and, sc, c);
    }
    void ma_and(Imm32 imm, Register src1, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(src1, imm, dest, op_and, sc, c);
    }


    // bit clear (dest <- dest & ~imm) or (dest <- src1 & ~src2)
    void ma_bic(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(dest, imm, dest, op_bic, sc, c);
    }

    // exclusive or
    void ma_eor(Register src, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_eor(dest, src, dest, sc, c);
    }
    void ma_eor(Register src1, Register src2, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        as_eor(dest, src1, O2Reg(src2), sc, c);
    }
    void ma_eor(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(dest, imm, dest, op_eor, sc, c);
    }
    void ma_eor(Imm32 imm, Register src1, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(src1, imm, dest, op_eor, sc, c);
    }

    // or
    void ma_orr(Register src, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_orr(dest, src, dest, sc, c);
    }
    void ma_orr(Register src1, Register src2, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        as_orr(dest, src1, O2Reg(src2), sc, c);
    }
    void ma_orr(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(dest, imm, dest, op_orr, sc, c);
    }
    void ma_orr(Imm32 imm, Register src1, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always)
    {
        ma_alu(src1, imm, dest, op_orr, sc, c);
    }

    // arithmetic based ops
    // add with carry
    void ma_adc(Imm32 imm, Register dest) {
        ma_alu(dest, imm, dest, op_adc);
    }
    void ma_adc(Register src, Register dest) {
        as_alu(dest, dest, O2Reg(src), op_adc);
    }
    void ma_adc(Register src1, Register src2, Register dest) {
        as_alu(dest, src1, O2Reg(src2), op_adc);
    }

    // add
    void ma_add(Imm32 imm, Register dest) {
        ma_alu(dest, imm, dest, op_add);
    }
    void ma_add(Register src1, Register dest) {
        as_alu(dest, dest, O2Reg(src1), op_add);
    }
    void ma_add(Register src1, Register src2, Register dest) {
        as_alu(dest, src1, O2Reg(dest), op_add);
    }
    void ma_add(Register src1, Operand op, Register dest) {
        ma_alu(src1, op, dest, op_add);
    }
    void ma_add(Register src1, Imm32 op, Register dest) {
        ma_alu(src1, op, dest, op_add);
    }

    // subtract with carry
    void ma_sbc(Imm32 imm, Register dest) {
        ma_alu(dest, imm, dest, op_sbc);
    }
    void ma_sbc(Register src1, Register dest) {
        as_alu(dest, dest, O2Reg(src1), op_sbc);
    }
    void ma_sbc(Register src1, Register src2, Register dest) {
        as_alu(dest, src1, O2Reg(dest), op_sbc);
    }

    // subtract
    void ma_sub(Imm32 imm, Register dest) {
        ma_alu(dest, imm, dest, op_sub);
    }
    void ma_sub(Register src1, Register dest) {
        ma_alu(dest, Operand(O2Reg(src1)), dest, op_sub);
    }
    void ma_sub(Register src1, Register src2, Register dest) {
        ma_alu(src1, Operand(O2Reg(src2)), dest, op_sub);
    }

    // reverse subtract
    void ma_rsb(Imm32 imm, Register dest) {
        ma_alu(dest, imm, dest, op_rsb);
    }
    void ma_rsb(Register src1, Register dest) {
        as_alu(dest, dest, O2Reg(src1), op_add);
    }
    void ma_rsb(Register src1, Register src2, Register dest) {
        as_alu(dest, src1, O2Reg(dest), op_rsc);
    }
    void ma_rsb(Register src1, Imm32 src2, Register dest) {
        JS_NOT_REACHED("Feature NYI");
    }

    // reverse subtract with carry
    void ma_rsc(Imm32 imm, Register dest) {
        ma_alu(dest, imm, dest, op_rsc);
    }
    void ma_rsc(Register src1, Register dest) {
        as_alu(dest, dest, O2Reg(src1), op_rsc);
    }
    void ma_rsc(Register src1, Register src2, Register dest) {
        as_alu(dest, src1, O2Reg(dest), op_rsc);
    }

    // compares/tests
    // compare negative (sets condition codes as src1 + src2 would)
    void ma_cmn(Imm32 imm, Register src1) {
        ma_alu(src1, imm, InvalidReg, op_cmn);
    }
    void ma_cmn(Register src1, Register src2) {
        as_alu(InvalidReg, src2, O2Reg(src1), op_cmn);
    }
    void ma_cmn(Register src1, Operand op) {
        JS_NOT_REACHED("Feature NYI");
    }

    // compare (src - src2)
    void ma_cmp(Imm32 imm, Register src1) {
        ma_alu(src1, imm, InvalidReg, op_cmp);
    }
    void ma_cmp(Register src1, Operand op) {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_cmp(Register src1, Register src2) {
        as_cmp(src2, O2Reg(src1));
        JS_NOT_REACHED("Feature NYI");
    }

    // test for equality, (src1^src2)
    void ma_teq(Imm32 imm, Register src1) {
        ma_alu(src1, imm, InvalidReg, op_teq);
    }
    void ma_teq(Register src2, Register src1) {
        as_tst(src2, O2Reg(src1));
    }
    void ma_teq(Register src1, Operand op) {
        JS_NOT_REACHED("Feature NYI");
    }


    // test (src1 & src2)
    void ma_tst(Imm32 imm, Register src1) {
        ma_alu(src1, imm, InvalidReg, op_tst, SetCond);
    }
    void ma_tst(Register src1, Register src2) {
        as_tst(src1, O2Reg(src2));
    }
    void ma_tst(Register src1, Operand op) {
        as_tst(src1, op.toOp2());
    }


    // memory
    // shortcut for when we know we're transferring 32 bits of data
    void ma_dtr(LoadStore ls, Register rn, Imm32 offset, Register rt,
                Index mode = Offset, Condition cc = Always)
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
        }
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_dtr(LoadStore ls, Register rn, Register rm, Register rt,
                Index mode = Offset, Condition cc = Always)
    {
        JS_NOT_REACHED("Feature NYI");
    }

    void ma_str(Register rt, DTRAddr addr, Index mode = Offset, Condition cc = Always)
    {
        as_dtr(IsStore, 32, mode, rt, addr, cc);
    }
    void ma_ldr(DTRAddr addr, Register rt, Index mode = Offset, Condition cc = Always)
    {
        as_dtr(IsLoad, 32, mode, rt, addr, cc);
    }
    // specialty for moving N bits of data, where n == 8,16,32,64
    void ma_dataTransferN(LoadStore ls, int size,
                          Register rn, Register rm, Register rt,
                          Index mode = Offset, Condition cc = Always) {
        JS_NOT_REACHED("Feature NYI");
    }

    void ma_dataTransferN(LoadStore ls, int size,
                          Register rn, Imm32 offset, Register rt,
                          Index mode = Offset, Condition cc = Always) {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_pop(Register r) {
        ma_dtr(IsLoad, sp, Imm32(4), r, PostIndex);
    }
    void ma_push(Register r) {
        ma_dtr(IsStore, sp ,Imm32(4), r, PreIndex);
    }

    // branches when done from within arm-specific code
    void ma_b(Label *dest, Condition c = Always)
    {
        as_b(dest, c);
    }
    void ma_b(void *target, Relocation::Kind reloc)
    {
            JS_NOT_REACHED("Feature NYI");
    }
    void ma_b(void *target, Condition c, Relocation::Kind reloc)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    // this is almost NEVER necessary, we'll basically never be calling a label
    // except, possibly in the crazy bailout-table case.
    void ma_bl(Label *dest, Condition c = Always)
    {
        as_bl(dest, c);
    }

    //VFP/ALU
    void ma_vadd(FloatRegister src1, FloatRegister src2, FloatRegister dst)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_vmul(FloatRegister src1, FloatRegister src2, FloatRegister dst)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_vcmp_F64(FloatRegister src1, FloatRegister src2)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_vcvt_F64_I32(FloatRegister src, FloatRegister dest)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_vcvt_I32_F64(FloatRegister src, FloatRegister dest)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_vmov(FloatRegister src, Register dest)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_vldr(VFPAddr addr, FloatRegister dest)
    {
        JS_NOT_REACHED("Feature NYI");
    }
    void ma_vstr(FloatRegister src, VFPAddr addr)
    {
        JS_NOT_REACHED("Feature NYI");
    }
  protected:
    uint32 alignStackForCall(uint32 stackForArgs) {
        // framePushed_ is accurate, so precisely adjust the stack requirement.
        uint32 displacement = stackForArgs + framePushed_;
        return stackForArgs + ComputeByteAlignment(displacement, StackAlignment);
    }

    uint32 dynamicallyAlignStackForCall(uint32 stackForArgs, const Register &scratch) {
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

    void restoreStackFromDynamicAlignment() {
        // x86 supports pop esp.  on arm, that isn't well defined, so just
        //  do it manually
        as_dtr(IsLoad, 32, Offset, sp, DTRAddr(sp, DtrOffImm(0)));
    }

  public:
    void reserveStack(uint32 amount) {
        if (amount)
            ma_sub(Imm32(amount), sp);
        framePushed_ += amount;
    }
    void freeStack(uint32 amount) {
        JS_ASSERT(amount <= framePushed_);
        if (amount)
            ma_add(Imm32(amount), sp);
        framePushed_ -= amount;
    }

    void branchTest32(Condition cond, const Address &address, Imm32 imm, Label *label) {
        JS_NOT_REACHED("NYI");
    }
    void branchPtr(Condition cond, Register lhs, ImmGCPtr ptr, Label *label) {
        JS_NOT_REACHED("NYI");
    }

    void movePtr(ImmWord imm, Register dest) {
        ma_mov(Imm32(imm.value), dest);
    }
    void movePtr(ImmGCPtr imm, Register dest) {
        ma_mov(imm, dest);
    }
    void loadPtr(const Address &address, Register dest) {
        JS_NOT_REACHED("NYI");
    }
    void setStackArg(const Register &reg, uint32 arg) {
        ma_dataTransferN(IsStore, 32, sp, Imm32(arg * STACK_SLOT_SIZE), reg);

    }
#ifdef DEBUG
    void checkCallAlignment() {
        Label good;
        ma_tst(Imm32(StackAlignment - 1), sp);
        ma_b(&good, Equal);
        breakpoint();
        bind(&good);
    }
#endif

    // Returns the register containing the type tag.
    Register splitTagForTest(const ValueOperand &value) {
        return value.typeReg();
    }

    // higher level tag testing code
    Condition testInt32(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        ma_cmp(ImmType(JSVAL_TYPE_INT32), value.typeReg());
        return cond;
    }

    Condition testBoolean(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        ma_cmp(ImmType(JSVAL_TYPE_BOOLEAN), value.typeReg());
        return cond;
    }
    Condition testDouble(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        JS_NOT_REACHED("Codegen for testDouble NYI");
        Condition actual = (cond == Equal)
                           ? Below
                           : AboveOrEqual;
        ma_cmp(ImmTag(JSVAL_TAG_CLEAR), value.typeReg());
        return actual;
    }
    Condition testNull(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        ma_cmp(ImmType(JSVAL_TYPE_NULL), value.typeReg());
        return cond;
    }
    Condition testUndefined(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        ma_cmp(ImmType(JSVAL_TYPE_UNDEFINED), value.typeReg());
        return cond;
    }
    Condition testString(Condition cond, const ValueOperand &value) {
        return testString(cond, value.typeReg());
    }
    Condition testObject(Condition cond, const ValueOperand &value) {
        return testObject(cond, value.typeReg());
    }

    // register-based tests
    Condition testInt32(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_INT32), tag);
        return cond;
    }
    Condition testBoolean(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_BOOLEAN), tag);
        return cond;
    }
    Condition testNull(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_NULL), tag);
        return cond;
    }
    Condition testUndefined(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_UNDEFINED), tag);
        return cond;
    }
    Condition testString(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_STRING), tag);
        return cond;
    }
    Condition testObject(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        ma_cmp(ImmTag(JSVAL_TAG_OBJECT), tag);
        return cond;
    }

    // unboxing code
    void unboxInt32(const ValueOperand &operand, const Register &dest) {
        ma_mov(operand.payloadReg(), dest);
    }
    void unboxBoolean(const ValueOperand &operand, const Register &dest) {
        ma_mov(operand.payloadReg(), dest);
    }
    void unboxDouble(const ValueOperand &operand, const FloatRegister &dest) {
        JS_ASSERT(dest != ScratchFloatReg);
        as_vxfer(operand.payloadReg(), operand.typeReg(),
                VFPRegister(dest), FloatToCore);
    }

    // Extended unboxing API. If the payload is already in a register, returns
    // that register. Otherwise, provides a move to the given scratch register,
    // and returns that.
    Register extractObject(const Address &address, Register scratch) {
        JS_NOT_REACHED("NYI");
        return scratch;
    }
    Register extractObject(const ValueOperand &value, Register scratch) {
        return value.payloadReg();
    }
    Register extractTag(const Address &address, Register scratch) {
        JS_NOT_REACHED("NYI");
        return scratch;
    }
    Register extractTag(const ValueOperand &value, Register scratch) {
        return value.typeReg();
    }

    void boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        JS_NOT_REACHED("Codegen for boolValueToDouble NYI");
#if 0
        cvtsi2sd(operand.payloadReg(), dest);
#endif
    }
    void int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        JS_NOT_REACHED("Codegen for int32ValueToDouble NYI");
        // transfer the integral value to a floating point register
        VFPRegister vfpdest = VFPRegister(dest);
        as_vxfer(operand.payloadReg(), InvalidReg,
                 vfpdest.intOverlay(), CoreToFloat);
        // convert the value to a double.
        as_vcvt(dest, dest);
    }

    void loadStaticDouble(const double *dp, const FloatRegister &dest) {
        JS_NOT_REACHED("Codegen for loadStaticDouble NYI");
#if 0
        _vldr()
        movsd(dp, dest);
#endif
    }
    // treat the value as a boolean, and set condition codes accordingly
    Condition testInt32Truthy(bool truthy, const ValueOperand &operand) {
        ma_tst(operand.payloadReg(), operand.payloadReg());
        return truthy ? NonZero : Zero;
    }
    Condition testBooleanTruthy(bool truthy, const ValueOperand &operand) {
        ma_tst(operand.payloadReg(), operand.payloadReg());
        return truthy ? NonZero : Zero;
    }
    Condition testDoubleTruthy(bool truthy, const FloatRegister &reg) {
        JS_NOT_REACHED("codegen for testDoubleTruthy NYI");
        // need to do vfp code here.
#if 0
        xorpd(ScratchFloatReg, ScratchFloatReg);
        ucomisd(ScratchFloatReg, reg);
#endif
        return truthy ? NonZero : Zero;
    }
#if 0
#endif
    void breakpoint() {
        as_bkpt();
    }
};

class MacroAssemblerARMCompat : public MacroAssemblerARM
{
public:
    // jumps + other functions that should be called from
    // non-arm specific code...
    // basically, an x86 front end on top of the ARM code.
    void j(Condition code , Label *dest)
    {
        as_b(dest, code);
    }
    void j(Label *dest)
    {
        as_b(dest, Always);
    }

    void mov(Imm32 imm, Register dest) {
        ma_mov(imm, dest);
    }
    void call(const Register reg) {
        as_blx(reg);
    }
    void call(Label *label) {
        JS_NOT_REACHED("Feature NYI");
        /* we can blx to it if it close by, otherwise, we need to
         * set up a branch + link node.
         */
    }
    void ret() {
        ma_pop(pc);
    }
    void Push(const Register &reg) {
        as_dtr(IsStore, STACK_SLOT_SIZE*8, PreIndex,
               reg,DTRAddr( sp, DtrOffImm(-STACK_SLOT_SIZE)));
        framePushed_ += STACK_SLOT_SIZE;
    }
    void jump(Label *label) {
        as_b(label);
    }
    template<typename T>
    void branchTestInt32(Condition cond, const T & t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }
    template<typename T>
    void branchTestBoolean(Condition cond, const T & t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }
    template<typename T>
    void branchTestDouble(Condition cond, const T & t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }
    template<typename T>
    void branchTestNull(Condition cond, const T & t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }
    template<typename T>
    void branchTestObject(Condition cond, const T & t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }
    template<typename T>
    void branchTestString(Condition cond, const T & t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }
    template<typename T>
    void branchTestUndefined(Condition cond, const T & t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }
    template <typename T>
    void branchTestNumber(Condition cond, const T &t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }

    template<typename T>
    void branchTestBooleanTruthy(bool b, const T & t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }
};

typedef MacroAssemblerARMCompat MacroAssemblerSpecific;

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_arm_h__
