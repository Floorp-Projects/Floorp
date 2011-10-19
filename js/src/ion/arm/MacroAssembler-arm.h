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
                SetCond_ sc = NoSetCond, Condition c = Always);

    // These should likely be wrapped up as a set of macros
    // or something like that.  I cannot think of a good reason
    // to explicitly have all of this code.
    // ALU based ops
    // mov
    void ma_mov(Register src, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_mov(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_mov(const ImmGCPtr &ptr, Register dest);

    // Shifts (just a move with a shifting op2)
    void ma_lsl(Imm32 shift, Register src, Register dst);
    void ma_lsr(Imm32 shift, Register src, Register dst);
    void ma_asr(Imm32 shift, Register src, Register dst);
    void ma_ror(Imm32 shift, Register src, Register dst);
    void ma_rol(Imm32 shift, Register src, Register dst);
    // Shifts (just a move with a shifting op2)
    void ma_lsl(Register shift, Register src, Register dst);
    void ma_lsr(Register shift, Register src, Register dst);
    void ma_asr(Register shift, Register src, Register dst);
    void ma_ror(Register shift, Register src, Register dst);
    void ma_rol(Register shift, Register src, Register dst);

    // Move not (dest <- ~src)
    void ma_mvn(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);


    void ma_mvn(Register src1, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);


    // and
    void ma_and(Register src, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_and(Register src1, Register src2, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_and(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_and(Imm32 imm, Register src1, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);



    // bit clear (dest <- dest & ~imm) or (dest <- src1 & ~src2)
    void ma_bic(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);


    // exclusive or
    void ma_eor(Register src, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_eor(Register src1, Register src2, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_eor(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_eor(Imm32 imm, Register src1, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);


    // or
    void ma_orr(Register src, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_orr(Register src1, Register src2, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_orr(Imm32 imm, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);

    void ma_orr(Imm32 imm, Register src1, Register dest,
                SetCond_ sc = NoSetCond, Condition c = Always);


    // arithmetic based ops
    // add with carry
    void ma_adc(Imm32 imm, Register dest, SetCond_ sc = NoSetCond);
    void ma_adc(Register src, Register dest, SetCond_ sc = NoSetCond);
    void ma_adc(Register src1, Register src2, Register dest, SetCond_ sc = NoSetCond);

    // add
    void ma_add(Imm32 imm, Register dest, SetCond_ sc = NoSetCond);
    void ma_add(Register src1, Register dest, SetCond_ sc = NoSetCond);
    void ma_add(Register src1, Register src2, Register dest, SetCond_ sc = NoSetCond);
    void ma_add(Register src1, Operand op, Register dest, SetCond_ sc = NoSetCond);
    void ma_add(Register src1, Imm32 op, Register dest, SetCond_ sc = NoSetCond);

    // subtract with carry
    void ma_sbc(Imm32 imm, Register dest, SetCond_ sc = NoSetCond);
    void ma_sbc(Register src1, Register dest, SetCond_ sc = NoSetCond);
    void ma_sbc(Register src1, Register src2, Register dest, SetCond_ sc = NoSetCond);

    // subtract
    void ma_sub(Imm32 imm, Register dest, SetCond_ sc = NoSetCond);
    void ma_sub(Register src1, Register dest, SetCond_ sc = NoSetCond);
    void ma_sub(Register src1, Register src2, Register dest, SetCond_ sc = NoSetCond);
    void ma_sub(Register src1, Operand op, Register dest, SetCond_ sc = NoSetCond);
    void ma_sub(Register src1, Imm32 op, Register dest, SetCond_ sc = NoSetCond);

    // reverse subtract
    void ma_rsb(Imm32 imm, Register dest, SetCond_ sc = NoSetCond);
    void ma_rsb(Register src1, Register dest, SetCond_ sc = NoSetCond);
    void ma_rsb(Register src1, Register src2, Register dest, SetCond_ sc = NoSetCond);
    void ma_rsb(Register src1, Imm32 op2, Register dest, SetCond_ sc = NoSetCond);

    // reverse subtract with carry
    void ma_rsc(Imm32 imm, Register dest, SetCond_ sc = NoSetCond);
    void ma_rsc(Register src1, Register dest, SetCond_ sc = NoSetCond);
    void ma_rsc(Register src1, Register src2, Register dest, SetCond_ sc = NoSetCond);

    // compares/tests
    // compare negative (sets condition codes as src1 + src2 would)
    void ma_cmn(Imm32 imm, Register src1);
    void ma_cmn(Register src1, Register src2);
    void ma_cmn(Register src1, Operand op);

    // compare (src - src2)
    void ma_cmp(Imm32 imm, Register src1);
    void ma_cmp(Register src1, Operand op);
    void ma_cmp(Register src1, Register src2);

    // test for equality, (src1^src2)
    void ma_teq(Imm32 imm, Register src1);
    void ma_teq(Register src2, Register src1);
    void ma_teq(Register src1, Operand op);


    // test (src1 & src2)
    void ma_tst(Imm32 imm, Register src1);
    void ma_tst(Register src1, Register src2);
    void ma_tst(Register src1, Operand op);


    // memory
    // shortcut for when we know we're transferring 32 bits of data
    void ma_dtr(LoadStore ls, Register rn, Imm32 offset, Register rt,
                Index mode = Offset, Condition cc = Always);

    void ma_dtr(LoadStore ls, Register rn, Register rm, Register rt,
                Index mode = Offset, Condition cc = Always);


    void ma_str(Register rt, DTRAddr addr, Index mode = Offset, Condition cc = Always);

    void ma_ldr(DTRAddr addr, Register rt, Index mode = Offset, Condition cc = Always);
    void ma_ldrb(DTRAddr addr, Register rt, Index mode = Offset, Condition cc = Always);
    void ma_ldrh(EDtrAddr addr, Register rt, Index mode = Offset, Condition cc = Always);
    void ma_ldrsh(EDtrAddr addr, Register rt, Index mode = Offset, Condition cc = Always);
    void ma_ldrsb(EDtrAddr addr, Register rt, Index mode = Offset, Condition cc = Always);

    // specialty for moving N bits of data, where n == 8,16,32,64
    void ma_dataTransferN(LoadStore ls, int size,
                          Register rn, Register rm, Register rt,
                          Index mode = Offset, Condition cc = Always);

    void ma_dataTransferN(LoadStore ls, int size,
                          Register rn, Imm32 offset, Register rt,
                          Index mode = Offset, Condition cc = Always);
    void ma_pop(Register r);
    void ma_push(Register r);

    // branches when done from within arm-specific code
    void ma_b(Label *dest, Condition c = Always);

    void ma_b(void *target, Relocation::Kind reloc);

    void ma_b(void *target, Condition c, Relocation::Kind reloc);

    // this is almost NEVER necessary, we'll basically never be calling a label
    // except, possibly in the crazy bailout-table case.
    void ma_bl(Label *dest, Condition c = Always);


    //VFP/ALU
    void ma_vadd(FloatRegister src1, FloatRegister src2, FloatRegister dst);

    void ma_vmul(FloatRegister src1, FloatRegister src2, FloatRegister dst);

    void ma_vcmp_F64(FloatRegister src1, FloatRegister src2);

    // source is F64, dest is I32
    void ma_vcvt_F64_I32(FloatRegister src, FloatRegister dest);

    // source is I32, dest is F64
    void ma_vcvt_I32_F64(FloatRegister src, FloatRegister dest);

    void ma_vxfer(FloatRegister src, Register dest);

    void ma_vldr(VFPAddr addr, FloatRegister dest);

    void ma_vstr(FloatRegister src, VFPAddr addr);

  protected:
    uint32 alignStackForCall(uint32 stackForArgs);

    uint32 dynamicallyAlignStackForCall(uint32 stackForArgs, const Register &scratch);

    void restoreStackFromDynamicAlignment();

  public:
    void reserveStack(uint32 amount);
    void freeStack(uint32 amount);

    void movePtr(ImmWord imm, Register dest);
    void movePtr(ImmGCPtr imm, Register dest);
    void loadPtr(const Address &address, Register dest);
    void setStackArg(const Register &reg, uint32 arg);
#ifdef DEBUG
    void checkCallAlignment();
#endif

    // calls an Ion function, assumes that the stack is untouched (8 byte alinged)
    void ma_callIon(const Register reg);
    // callso an Ion function, assuming that sp has already been decremented
    void ma_callIonNoPush(const Register reg);
    // calls an ion function, assuming that the stack is currently not 8 byte aligned
    void ma_callIonHalfPush(const Register reg);
    void breakpoint();

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
#if 0
    void Push(const Register &reg) {
        as_dtr(IsStore, STACK_SLOT_SIZE*8, PreIndex,
               reg,DTRAddr( sp, DtrOffImm(-STACK_SLOT_SIZE)));
        framePushed_ += STACK_SLOT_SIZE;
    }
#endif
    void push(Imm32 imm) {
        ma_mov(imm, ScratchRegister);
        ma_push(ScratchRegister);
    }

    void jump(Label *label) {
        as_b(label);
    }


    // Returns the register containing the type tag.
    Register splitTagForTest(const ValueOperand &value) {
        return value.typeReg();
    }

    // higher level tag testing code
    Condition testInt32(Condition cond, const ValueOperand &value);

    Condition testBoolean(Condition cond, const ValueOperand &value);
    Condition testDouble(Condition cond, const ValueOperand &value);
    Condition testNull(Condition cond, const ValueOperand &value);
    Condition testUndefined(Condition cond, const ValueOperand &value);
    Condition testString(Condition cond, const ValueOperand &value);
    Condition testObject(Condition cond, const ValueOperand &value);

    // register-based tests
    Condition testInt32(Condition cond, const Register &tag);
    Condition testBoolean(Condition cond, const Register &tag);
    Condition testNull(Condition cond, const Register &tag);
    Condition testUndefined(Condition cond, const Register &tag);
    Condition testString(Condition cond, const Register &tag);
    Condition testObject(Condition cond, const Register &tag);

    // unboxing code
    void unboxInt32(const ValueOperand &operand, const Register &dest);
    void unboxBoolean(const ValueOperand &operand, const Register &dest);
    void unboxDouble(const ValueOperand &operand, const FloatRegister &dest);

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

    void boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest);
    void int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest);

    void loadStaticDouble(const double *dp, const FloatRegister &dest);
    // treat the value as a boolean, and set condition codes accordingly
    Condition testInt32Truthy(bool truthy, const ValueOperand &operand);
    Condition testBooleanTruthy(bool truthy, const ValueOperand &operand);
    Condition testDoubleTruthy(bool truthy, const FloatRegister &reg);

    template<typename T>
    void branchTestInt32(Condition cond, const T & t, Label *label) {
        Condition c = testInt32(cond, t);
        ma_b(label, c);
    }
    template<typename T>
    void branchTestBoolean(Condition cond, const T & t, Label *label) {
        Condition c = testBoolean(cond, t);
        ma_b(label, c);
    }
    template<typename T>
    void branchTestDouble(Condition cond, const T & t, Label *label) {
        Condition c = testDouble(cond, t);
        ma_b(label, c);
    }
    template<typename T>
    void branchTestNull(Condition cond, const T & t, Label *label) {
        Condition c = testNull(cond, t);
        ma_b(label, c);
    }
    template<typename T>
    void branchTestObject(Condition cond, const T & t, Label *label) {
        Condition c = testObject(cond, t);
        ma_b(label, c);
    }
    template<typename T>
    void branchTestString(Condition cond, const T & t, Label *label) {
        Condition c = testString(cond, t);
        ma_b(label, c);
    }
    template<typename T>
    void branchTestUndefined(Condition cond, const T & t, Label *label) {
        Condition c = testUndefined(cond, t);
        ma_b(label, c);
    }

    template <typename T>
    void branchTestNumber(Condition cond, const T &t, Label *label) {
        JS_NOT_REACHED("feature NYI");
    }

    template<typename T>
    void branchTestBooleanTruthy(bool b, const T & t, Label *label) {
        Condition c = testBooleanTruthy(b, t);
        ma_b(label, c);
    }
    void branchTest32(Condition cond, const Address &address, Imm32 imm, Label *label) {
        JS_NOT_REACHED("NYI");
    }
    void branchPtr(Condition cond, Register lhs, ImmGCPtr ptr, Label *label) {
        JS_NOT_REACHED("NYI");
    }

};

typedef MacroAssemblerARMCompat MacroAssemblerSpecific;

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_arm_h__
