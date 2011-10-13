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


    void convertInt32ToDouble(const Register &src, const FloatRegister &dest);


    uint32 framePushed() const {
        return framePushed_;
    }

    // For maximal awesomeness, 8 should be sufficent.
    static const uint32 StackAlignment = 8;
    // somewhat direct wrappers for the low-level assembler funcitons
    // bitops
    // attempt to encode a virtual alu instruction using
    // two real instructions.
  private:
    bool alu_dbl(Register src1, Imm32 imm, Register dest, ALUOp op,
                 SetCond_ sc, Condition c);
  public:
    void ma_alu(Register src1, Imm32 imm, Register dest,
                ALUOp op,
                SetCond_ sc =  NoSetCond, Condition c = Always);

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

    // Shifts (just a move with a shifting op2)
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
    // Shifts (just a move with a shifting op2)
    void ma_lsl(Register shift, Register src, Register dst) {
        as_mov(dst, lsl(src, shift));
    }
    void ma_lsr(Register shift, Register src, Register dst) {
        as_mov(dst, lsr(src, shift));
    }
    void ma_asr(Register shift, Register src, Register dst) {
        as_mov(dst, asr(src, shift));
    }
    void ma_ror(Register shift, Register src, Register dst) {
        as_mov(dst, ror(src, shift));
    }
    void ma_rol(Register shift, Register src, Register dst) {
        ma_rsb(shift, Imm32(32), ScratchRegister);
        as_mov(dst, ror(src, ScratchRegister));
    }

    // Move not (dest <- ~src)
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
    void ma_sub(Register src1, Operand op, Register dest) {
        ma_alu(src1, op, dest, op_sub);
    }
    void ma_sub(Register src1, Imm32 op, Register dest) {
        ma_alu(src1, op, dest, op_sub);
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
    void ma_rsb(Register src1, Imm32 op2, Register dest) {
        ma_alu(src1, op2, dest, op_rsb);
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
        ma_alu(src1, imm, InvalidReg, op_cmp, SetCond);
    }
    void ma_cmp(Register src1, Operand op) {
        as_cmp(src1, op.toOp2());
    }
    void ma_cmp(Register src1, Register src2) {
        as_cmp(src2, O2Reg(src1));
    }

    // test for equality, (src1^src2)
    void ma_teq(Imm32 imm, Register src1) {
        ma_alu(src1, imm, InvalidReg, op_teq, SetCond);
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
            as_dtr(ls, 32, mode, rt, DTRAddr(ScratchRegister, DtrOffImm(off & 0xfff)), cc);
        } else {
            ma_mov(offset, ScratchRegister);
            as_dtr(ls, 32, mode, rt, DTRAddr(rn, DtrRegImmShift(ScratchRegister, LSL, 0)));
        }
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
    void ma_bl(Label *dest, Condition c = Always)
    {
        as_bl(dest, c);
    }

    //VFP/ALU
    void ma_vadd(FloatRegister src1, FloatRegister src2, FloatRegister dst)
    {
        as_vadd(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
    }
    void ma_vmul(FloatRegister src1, FloatRegister src2, FloatRegister dst)
    {
        as_vmul(VFPRegister(dst), VFPRegister(src1), VFPRegister(src2));
    }
    void ma_vcmp_F64(FloatRegister src1, FloatRegister src2)
    {
        as_vcmp(VFPRegister(src1), VFPRegister(src2));
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
        //as_vmov(VFPRegister(dest), VFPRegister(src));
    }
    void ma_vldr(VFPAddr addr, FloatRegister dest)
    {
        as_vdtr(IsLoad, dest, addr);
    }
    void ma_vstr(FloatRegister src, VFPAddr addr)
    {
        as_vdtr(IsStore, src, addr);
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
                VFPRegister(dest), CoreToFloat);
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
