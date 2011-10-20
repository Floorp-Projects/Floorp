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

#ifndef jsion_macro_assembler_x64_h__
#define jsion_macro_assembler_x64_h__

#include "ion/shared/MacroAssembler-x86-shared.h"
#include "jsnum.h"

namespace js {
namespace ion {

struct ImmShiftedTag : public ImmWord
{
    ImmShiftedTag(JSValueShiftedTag shtag)
      : ImmWord((uintptr_t)shtag)
    { }

    ImmShiftedTag(JSValueType type)
      : ImmWord(uintptr_t(JSValueShiftedTag(JSVAL_TYPE_TO_SHIFTED_TAG(type))))
    { }
};

struct ImmTag : public Imm32
{
    ImmTag(JSValueTag tag)
      : Imm32(tag)
    { }
};

class MacroAssemblerX64 : public MacroAssemblerX86Shared
{
    static const uint32 StackAlignment = 16;

  protected:
    uint32 alignStackForCall(uint32 stackForArgs) {
        uint32 total = stackForArgs + ShadowStackSpace;
        uint32 displacement = total + framePushed_;
        return total + ComputeByteAlignment(displacement, StackAlignment);
    }

    uint32 dynamicallyAlignStackForCall(uint32 stackForArgs, const Register &scratch) {
        // framePushed_ is bogus or we don't know it for sure, so instead, save
        // the original value of esp and then chop off its low bits. Then, we
        // push the original value of esp.
        movq(rsp, scratch);
        andq(Imm32(~(StackAlignment - 1)), rsp);
        push(scratch);
        uint32 total = stackForArgs + ShadowStackSpace;
        uint32 displacement = total + STACK_SLOT_SIZE;
        return total + ComputeByteAlignment(displacement, StackAlignment);
    }

    void restoreStackFromDynamicAlignment() {
        pop(rsp);
    }

  public:
    /////////////////////////////////////////////////////////////////
    // X86/X64-common interface.
    /////////////////////////////////////////////////////////////////
    void storeValue(ValueOperand val, Operand dest) {
        movq(val.valueReg(), dest);
    }
    void loadValue(Operand src, ValueOperand val) {
        movq(src, val.valueReg());
    }
    void pushValue(ValueOperand val) {
        push(val.valueReg());
    }
    void popValue(ValueOperand val) {
        pop(val.valueReg());
    }

    void movePtr(Operand op, const Register &dest) {
        movq(op, dest);
    }
    void moveValue(const Value &val, const Register &dest) {
        movq(ImmWord((void *)val.asRawBits()), dest);
    }

    Condition testUndefined(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_UNDEFINED));
        return cond;
    }
    Condition testInt32(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_INT32));
        return cond;
    }
    Condition testBoolean(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_BOOLEAN));
        return cond;
    }
    Condition testNull(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_NULL));
        return cond;
    }
    Condition testString(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_STRING));
        return cond;
    }
    Condition testObject(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_OBJECT));
        return cond;
    }
    Condition testNumber(Condition cond, Register tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, Imm32(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET));
        return cond == Equal ? BelowOrEqual : Above;
    }
    Condition testUndefined(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testUndefined(cond, ScratchReg);
    }
    Condition testInt32(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testInt32(cond, ScratchReg);
    }
    Condition testBoolean(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testBoolean(cond, ScratchReg);
    }
    Condition testDouble(Condition cond, const ValueOperand &src) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        movq(ImmShiftedTag(JSVAL_SHIFTED_TAG_MAX_DOUBLE), ScratchReg);
        cmpq(src.value(), ScratchReg);
        return (cond == NotEqual) ? Above : BelowOrEqual;
    }
    Condition testNull(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testNull(cond, ScratchReg);
    }
    Condition testString(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testString(cond, ScratchReg);
    }
    Condition testObject(Condition cond, const ValueOperand &src) {
        splitTag(src, ScratchReg);
        return testObject(cond, ScratchReg);
    }

    void cmpPtr(const Register &lhs, const ImmWord rhs) {
        JS_ASSERT(lhs != ScratchReg);
        movq(rhs, ScratchReg);
        return cmpq(lhs, ScratchReg);
    }
    void cmpPtr(const Register &lhs, const ImmGCPtr rhs) {
        JS_ASSERT(lhs != ScratchReg);
        movq(rhs, ScratchReg);
        return cmpq(lhs, ScratchReg);
    }
    void testPtr(const Register &lhs, const Register &rhs) {
        testq(lhs, rhs);
    }

    /////////////////////////////////////////////////////////////////
    // Common interface.
    /////////////////////////////////////////////////////////////////
    void reserveStack(uint32 amount) {
        if (amount)
            subq(Imm32(amount), StackPointer);
        framePushed_ += amount;
    }
    void freeStack(uint32 amount) {
        JS_ASSERT(amount <= framePushed_);
        if (amount)
            addq(Imm32(amount), StackPointer);
        framePushed_ -= amount;
    }

    void addPtr(Imm32 imm, const Register &dest) {
        addq(imm, dest);
    }
    void subPtr(Imm32 imm, const Register &dest) {
        subq(imm, dest);
    }

    void branchPtr(Condition cond, Register lhs, ImmGCPtr ptr, Label *label) {
        cmpPtr(lhs, ptr);
        j(cond, label);
    }

    void movePtr(ImmWord imm, Register dest) {
        movq(imm, dest);
    }
    void movePtr(ImmGCPtr imm, Register dest) {
        movq(imm, dest);
    }
    void loadPtr(const Address &address, Register dest) {
        movq(Operand(address), dest);
    }
    void setStackArg(const Register &reg, uint32 arg) {
        movq(reg, Operand(rsp, (arg - NumArgRegs) * STACK_SLOT_SIZE + ShadowStackSpace));
    }
    void checkCallAlignment() {
#ifdef DEBUG
        Label good;
        movl(rsp, rax);
        testq(rax, Imm32(StackAlignment - 1));
        j(Equal, &good);
        breakpoint();
        bind(&good);
#endif
    }

    void splitTag(Register src, Register dest) {
        if (src != dest)
            movq(src, dest);
        shrq(Imm32(JSVAL_TAG_SHIFT), dest);
    }

    void splitTag(const ValueOperand &operand, const Register &dest) {
        JS_ASSERT(operand.valueReg() != dest);
        splitTag(operand.valueReg(), dest);
    }

    // Extracts the tag of a value and places it in ScratchReg.
    Register splitTagForTest(const ValueOperand &value) {
        splitTag(value, ScratchReg);
        return ScratchReg;
    }
    void cmpTag(const ValueOperand &operand, ImmTag tag) {
        Register reg = splitTagForTest(operand);
        cmpl(Operand(reg), tag);
    }

    void branchTestUndefined(Condition cond, Register tag, Label *label) {
        cond = testUndefined(cond, tag);
        j(cond, label);
    }
    void branchTestInt32(Condition cond, Register tag, Label *label) {
        cond = testInt32(cond, tag);
        j(cond, label);
    }
    void branchTestBoolean(Condition cond, Register tag, Label *label) {
        cond = testBoolean(cond, tag);
        j(cond, label);
    }
    void branchTestNull(Condition cond, Register tag, Label *label) {
        cond = testNull(cond, tag);
        j(cond, label);
    }
    void branchTestString(Condition cond, Register tag, Label *label) {
        cond = testString(cond, tag);
        j(cond, label);
    }
    void branchTestObject(Condition cond, Register tag, Label *label) {
        cond = testObject(cond, tag);
        j(cond, label);
    }
    void branchTestNumber(Condition cond, Register tag, Label *label) {
        cond = testNumber(cond, tag);
        j(cond, label);
    }

    // Type-testing instructions on x64 will clobber ScratchReg, when used on
    // ValueOperands.
    void branchTestUndefined(Condition cond, const ValueOperand &src, Label *label) {
        cond = testUndefined(cond, src);
        j(cond, label);
    }
    void branchTestInt32(Condition cond, const ValueOperand &src, Label *label) {
        splitTag(src, ScratchReg);
        branchTestInt32(cond, ScratchReg, label);
    }
    void branchTestBoolean(Condition cond, const ValueOperand &src, Label *label) {
        splitTag(src, ScratchReg);
        branchTestBoolean(cond, ScratchReg, label);
    }
    void branchTestDouble(Condition cond, const ValueOperand &src, Label *label) {
        cond = testDouble(cond, src);
        j(cond, label);
    }
    void branchTestNull(Condition cond, const ValueOperand &src, Label *label) {
        cond = testNull(cond, src);
        j(cond, label);
    }
    void branchTestString(Condition cond, const ValueOperand &src, Label *label) {
        cond = testString(cond, src);
        j(cond, label);
    }
    void branchTestObject(Condition cond, const ValueOperand &src, Label *label) {
        cond = testObject(cond, src);
        j(cond, label);
    }

    // Note that the |dest| register here may be ScratchReg, so we shouldn't
    // use it.
    void unboxInt32(const ValueOperand &src, const Register &dest) {
        movl(src.value(), dest);
    }
    void unboxBoolean(const ValueOperand &src, const Register &dest) {
        movl(src.value(), dest);
    }
    void unboxDouble(const ValueOperand &src, const FloatRegister &dest) {
        movqsd(src.valueReg(), dest);
    }
    void unboxString(const ValueOperand &src, const Register &dest) {
        movq(ImmWord(JSVAL_PAYLOAD_MASK), dest);
        andq(src.valueReg(), dest);
    }
    void unboxObject(const ValueOperand &src, const Register &dest) {
        // TODO: Can we unbox more efficiently? Bug 680294.
        movq(ImmWord(JSVAL_PAYLOAD_MASK), ScratchReg);
        if (src.valueReg() != dest)
            movq(src.valueReg(), dest);
        andq(ScratchReg, dest);
    }

    // Extended unboxing API. If the payload is already in a register, returns
    // that register. Otherwise, provides a move to the given scratch register,
    // and returns that.
    Register extractObject(const Address &address, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        loadPtr(address, scratch);
        unboxObject(ValueOperand(scratch), scratch);
        return scratch;
    }
    Register extractObject(const ValueOperand &value, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        unboxObject(value, scratch);
        return scratch;
    }
    Register extractTag(const Address &address, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        loadPtr(address, scratch);
        splitTag(scratch, scratch);
        return scratch;
    }
    Register extractTag(const ValueOperand &value, Register scratch) {
        JS_ASSERT(scratch != ScratchReg);
        splitTag(value, scratch);
        return scratch;
    }

    // These two functions use the low 32-bits of the full value register.
    void boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        cvtsi2sd(operand.value(), dest);
    }
    void int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        cvtsi2sd(operand.value(), dest);
    }

    void loadDouble(double d, const FloatRegister &dest) {
        jsdpun dpun;
        dpun.d = d;
        if (dpun.u64 == 0) {
            xorpd(dest, dest);
        } else {
            movq(ImmWord(dpun.u64), ScratchReg);
            movqsd(ScratchReg, dest);
        }
    }
    void loadStaticDouble(const double *dp, const FloatRegister &dest) {
        loadDouble(*dp, dest);
    }

    Condition testInt32Truthy(bool truthy, const ValueOperand &operand) {
        testl(operand.valueReg(), operand.valueReg());
        return truthy ? NonZero : Zero;
    }
    void branchTestBooleanTruthy(bool truthy, const ValueOperand &operand, Label *label) {
        testl(operand.valueReg(), operand.valueReg());
        j(truthy ? NonZero : Zero, label);
    }
};

typedef MacroAssemblerX64 MacroAssemblerSpecific;

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_x64_h__

