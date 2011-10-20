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

#ifndef jsion_macro_assembler_x86_h__
#define jsion_macro_assembler_x86_h__

#include "ion/shared/MacroAssembler-x86-shared.h"

namespace js {
namespace ion {

class MacroAssemblerX86 : public MacroAssemblerX86Shared
{
    static const uint32 StackAlignment = 16;

  private:
    Operand payloadOf(const Address &address) {
        return Operand(address.base, address.offset);
    }
    Operand tagOf(const Address &address) {
        return Operand(address.base, address.offset + 4);
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
        movl(esp, scratch);
        andl(Imm32(~(StackAlignment - 1)), esp);
        push(scratch);
        uint32 displacement = stackForArgs + STACK_SLOT_SIZE;
        return stackForArgs + ComputeByteAlignment(displacement, StackAlignment);
    }

    void restoreStackFromDynamicAlignment() {
        pop(esp);
    }

  public:
    /////////////////////////////////////////////////////////////////
    // X86-specific interface.
    /////////////////////////////////////////////////////////////////
    Operand ToPayload(Operand base) {
        return base;
    }
    Operand ToType(Operand base) {
        return Operand(Register::FromCode(base.base()),
                       base.disp() + sizeof(void *));
    }
    void moveValue(const Value &val, Register type, Register data) {
        jsval_layout jv = JSVAL_TO_IMPL(val);
        movl(Imm32(jv.s.tag), type);
        movl(Imm32(jv.s.payload.i32), data);
    }

    /////////////////////////////////////////////////////////////////
    // X86/X64-common interface.
    /////////////////////////////////////////////////////////////////
    void storeValue(ValueOperand val, Operand dest) {
        movl(val.payloadReg(), ToPayload(dest));
        movl(val.typeReg(), ToType(dest));
    }
    void loadValue(Operand src, ValueOperand val) {
        Operand payload = ToPayload(src);
        Operand type = ToType(src);

        // Ensure that loading the payload does not erase the pointer to the
        // Value in memory.
        if (Register::FromCode(type.base()) != val.payloadReg()) {
            movl(payload, val.payloadReg());
            movl(type, val.typeReg());
        } else {
            movl(type, val.typeReg());
            movl(payload, val.payloadReg());
        }
    }
    void pushValue(ValueOperand val) {
        push(val.typeReg());
        push(val.payloadReg());
    }
    void popValue(ValueOperand val) {
        pop(val.payloadReg());
        pop(val.typeReg());
    }

    void movePtr(Operand op, const Register &dest) {
        movl(op, dest);
    }

    // Returns the register containing the type tag.
    Register splitTagForTest(const ValueOperand &value) {
        return value.typeReg();
    }

    Condition testUndefined(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_UNDEFINED));
        return cond;
    }
    Condition testBoolean(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_BOOLEAN));
        return cond;
    }
    Condition testInt32(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_INT32));
        return cond;
    }
    Condition testDouble(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        Condition actual = (cond == Equal) ? Below : AboveOrEqual;
        cmpl(tag, ImmTag(JSVAL_TAG_CLEAR));
        return actual;
    }
    Condition testNull(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_NULL));
        return cond;
    }
    Condition testString(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_STRING));
        return cond;
    }
    Condition testObject(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_TAG_OBJECT));
        return cond;
    }
    Condition testNumber(Condition cond, const Register &tag) {
        JS_ASSERT(cond == Equal || cond == NotEqual);
        cmpl(tag, ImmTag(JSVAL_UPPER_INCL_TAG_OF_NUMBER_SET));
        return cond == Equal ? BelowOrEqual : Above;
    }
    Condition testUndefined(Condition cond, const ValueOperand &value) {
        return testUndefined(cond, value.typeReg());
    }
    Condition testBoolean(Condition cond, const ValueOperand &value) {
        return testBoolean(cond, value.typeReg());
    }
    Condition testInt32(Condition cond, const ValueOperand &value) {
        return testInt32(cond, value.typeReg());
    }
    Condition testDouble(Condition cond, const ValueOperand &value) {
        return testDouble(cond, value.typeReg());
    }
    Condition testNull(Condition cond, const ValueOperand &value) {
        return testNull(cond, value.typeReg());
    }
    Condition testString(Condition cond, const ValueOperand &value) {
        return testString(cond, value.typeReg());
    }
    Condition testObject(Condition cond, const ValueOperand &value) {
        return testObject(cond, value.typeReg());
    }
    Condition testNumber(Condition cond, const ValueOperand &value) {
        return testNumber(cond, value.typeReg());
    }

    void cmpPtr(const Register &lhs, const ImmWord rhs) {
        cmpl(lhs, Imm32(rhs.value));
    }
    void testPtr(const Register &lhs, const Register &rhs) {
        return testl(lhs, rhs);
    }

    /////////////////////////////////////////////////////////////////
    // Common interface.
    /////////////////////////////////////////////////////////////////
    void reserveStack(uint32 amount) {
        if (amount)
            subl(Imm32(amount), StackPointer);
        framePushed_ += amount;
    }
    void freeStack(uint32 amount) {
        JS_ASSERT(amount <= framePushed_);
        if (amount)
            addl(Imm32(amount), StackPointer);
        framePushed_ -= amount;
    }

    void addPtr(Imm32 imm, const Register &dest) {
        addl(imm, dest);
    }
    void subPtr(Imm32 imm, const Register &dest) {
        subl(imm, dest);
    }

    void branchPtr(Condition cond, Register lhs, ImmGCPtr ptr, Label *label) {
        cmpl(lhs, ptr);
        j(cond, label);
    }

    void movePtr(ImmWord imm, Register dest) {
        movl(Imm32(imm.value), dest);
    }
    void movePtr(ImmGCPtr imm, Register dest) {
        movl(imm, dest);
    }
    void loadPtr(const Address &address, Register dest) {
        movl(Operand(address), dest);
    }
    void setStackArg(const Register &reg, uint32 arg) {
        movl(reg, Operand(esp, arg * STACK_SLOT_SIZE));
    }
    void checkCallAlignment() {
#ifdef DEBUG
        Label good;
        movl(esp, eax);
        testl(eax, Imm32(StackAlignment - 1));
        j(Equal, &good);
        breakpoint();
        bind(&good);
#endif
    }

    // Type testing instructions can take a tag in a register or a
    // ValueOperand.
    template <typename T>
    void branchTestUndefined(Condition cond, const T &t, Label *label) {
        cond = testUndefined(cond, t);
        j(cond, label);
    }
    template <typename T>
    void branchTestInt32(Condition cond, const T &t, Label *label) {
        cond = testInt32(cond, t);
        j(cond, label);
    }
    template <typename T>
    void branchTestBoolean(Condition cond, const T &t, Label *label) {
        cond = testBoolean(cond, t);
        j(cond, label);
    }
    template <typename T>
    void branchTestDouble(Condition cond, const T &t, Label *label) {
        cond = testDouble(cond, t);
        j(cond, label);
    }
    template <typename T>
    void branchTestNull(Condition cond, const T &t, Label *label) {
        cond = testNull(cond, t);
        j(cond, label);
    }
    template <typename T>
    void branchTestString(Condition cond, const T &t, Label *label) {
        cond = testString(cond, t);
        j(cond, label);
    }
    template <typename T>
    void branchTestObject(Condition cond, const T &t, Label *label) {
        cond = testObject(cond, t);
        j(cond, label);
    }
    template <typename T>
    void branchTestNumber(Condition cond, const T &t, Label *label) {
        cond = testNumber(cond, t);
        j(cond, label);
    }

    void unboxInt32(const ValueOperand &operand, const Register &dest) {
        movl(operand.payloadReg(), dest);
    }
    void unboxBoolean(const ValueOperand &operand, const Register &dest) {
        movl(operand.payloadReg(), dest);
    }
    void unboxDouble(const ValueOperand &operand, const FloatRegister &dest) {
        JS_ASSERT(dest != ScratchFloatReg);
        if (Assembler::HasSSE41()) {
            movd(operand.payloadReg(), dest);
            pinsrd(operand.typeReg(), dest);
        } else {
            movd(operand.payloadReg(), dest);
            movd(operand.typeReg(), ScratchFloatReg);
            unpcklps(ScratchFloatReg, dest);
        }
    }

    // Extended unboxing API. If the payload is already in a register, returns
    // that register. Otherwise, provides a move to the given scratch register,
    // and returns that.
    Register extractObject(const Address &address, Register scratch) {
        movl(payloadOf(address), scratch);
        return scratch;
    }
    Register extractObject(const ValueOperand &value, Register scratch) {
        return value.payloadReg();
    }
    Register extractTag(const Address &address, Register scratch) {
        movl(tagOf(address), scratch);
        return scratch;
    }
    Register extractTag(const ValueOperand &value, Register scratch) {
        return value.typeReg();
    }

    void boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        cvtsi2sd(operand.payloadReg(), dest);
    }
    void int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        cvtsi2sd(operand.payloadReg(), dest);
    }

    void loadStaticDouble(const double *dp, const FloatRegister &dest) {
        movsd(dp, dest);
    }

    Condition testInt32Truthy(bool truthy, const ValueOperand &operand) {
        testl(operand.payloadReg(), operand.payloadReg());
        return truthy ? NonZero : Zero;
    }
    void branchTestBooleanTruthy(bool truthy, const ValueOperand &operand, Label *label) {
        testl(operand.payloadReg(), operand.payloadReg());
        j(truthy ? NonZero : Zero, label);
    }
};

typedef MacroAssemblerX86 MacroAssemblerSpecific;

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_x86_h__

