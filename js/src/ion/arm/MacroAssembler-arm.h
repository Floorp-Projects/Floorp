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

    void Push(const Register &reg) {
        push(reg);
        framePushed_ += STACK_SLOT_SIZE;
    }

    void convertInt32ToDouble(const Register &src, const FloatRegister &dest) {
        cvtsi2sd(Operand(src), dest);
    }
    void jump(Label *label) {
#if 0
        jmp(label);
#endif
    }

    uint32 framePushed() const {
        return framePushed_;
    }

    // For maximal awesomeness, 8 should be sufficent.
    static const uint32 StackAlignment = 8;

  protected:
    uint32 alignStackForCall(uint32 stackForArgs) {
        JS_NOT_REACHED("Codegen for alignStackForCall NYI");
        // framePushed_ is accurate, so precisely adjust the stack requirement.
        uint32 displacement = stackForArgs + framePushed_;
        return stackForArgs + ComputeByteAlignment(displacement, StackAlignment);
    }

    uint32 dynamicallyAlignStackForCall(uint32 stackForArgs, const Register &scratch) {
        // framePushed_ is bogus or we don't know it for sure, so instead, save
        // the original value of esp and then chop off its low bits. Then, we
        // push the original value of esp.
        JS_NOT_REACHED("Codegen for dynamicallyAlignStackForCall NYI");

#if 0
        movl(esp, scratch);
        andl(Imm32(~(StackAlignment - 1)), esp);
        push(scratch);
#endif
        uint32 displacement = stackForArgs + STACK_SLOT_SIZE;
        return stackForArgs + ComputeByteAlignment(displacement, StackAlignment);
    }

    void restoreStackFromDynamicAlignment() {
        JS_NOT_REACHED("Codegen for restoreStackFromDynamicAlignment NYI");
#if 0
        pop(esp);
#endif

    }

  public:
    void reserveStack(uint32 amount) {
        JS_NOT_REACHED("Codegen for reserveStack NYI");

#if 0
        if (amount)
            subl(Imm32(amount), esp);
        framePushed_ += amount;
#endif
    }
    void freeStack(uint32 amount) {
        JS_NOT_REACHED("Codegen for freeStack NYI");
#if 0
        JS_ASSERT(amount <= framePushed_);
        if (amount)
            addl(Imm32(amount), esp);
        framePushed_ -= amount;
#endif
    }
    void movePtr(ImmWord imm, const Register &dest) {
        JS_NOT_REACHED("Codegen for movePtr NYI");
#if 0
        movl(Imm32(imm.value), dest);
#endif
    }
    void setStackArg(const Register &reg, uint32 arg) {
        JS_NOT_REACHED("Codegen for setStackArg NYI");

#if 0
        movl(reg, Operand(esp, arg * STACK_SLOT_SIZE));
#endif
    }
    void checkCallAlignment() {
#ifdef DEBUG
        JS_NOT_REACHED("Codegen for checkCallAlignment NYI");
#if 0
        Label good;
        movl(esp, eax);
        testl(Imm32(StackAlignment - 1), eax);
        j(Equal, &good);
        breakpoint();
        bind(&good);
#endif
#endif
    }

    Condition testInt32(Condition cond, const ValueOperand &value) {
        JS_NOT_REACHED("Codegen for testInt32 NYI");
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
#if 0
        cmpl(ImmType(JSVAL_TYPE_INT32), value.typeReg());
#endif
        return cond;
    }
    Condition testBoolean(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        JS_NOT_REACHED("Codegen for testBoolean NYI");

#if 0
        cmpl(ImmType(JSVAL_TYPE_BOOLEAN), value.typeReg());
#endif
        return cond;
    }
    Condition testDouble(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        JS_NOT_REACHED("Codegen for testDouble NYI");
        Condition actual = (cond == Assembler::Equal)
                           ? Assembler::Below
                           : Assembler::AboveOrEqual;
#if 0
        cmpl(ImmTag(JSVAL_TAG_CLEAR), value.typeReg());
#endif
        return actual;
    }
    Condition testNull(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        JS_NOT_REACHED("Codegen for testNull NYI");

#if 0
        cmpl(ImmType(JSVAL_TYPE_NULL), value.typeReg());
#endif
        return cond;
    }
    Condition testUndefined(Condition cond, const ValueOperand &value) {
        JS_ASSERT(cond == Assembler::Equal || cond == Assembler::NotEqual);
        JS_NOT_REACHED("Codegen for testUndefined NYI");

#if 0
        cmpl(ImmType(JSVAL_TYPE_UNDEFINED), value.typeReg());
#endif
        return cond;
    }

    void unboxInt32(const ValueOperand &operand, const Register &dest) {
        JS_NOT_REACHED("Codegen for unboxInt32 NYI");
#if 0
        movl(operand.payloadReg(), dest);
#endif
    }
    void unboxBoolean(const ValueOperand &operand, const Register &dest) {
        JS_NOT_REACHED("Codegen for unboxBoolean NYI");

#if 0
        movl(operand.payloadReg(), dest);
#endif
    }
    void unboxDouble(const ValueOperand &operand, const FloatRegister &dest) {
        JS_NOT_REACHED("Codegen for unboxDouble NYI");
#if 0
        JS_ASSERT(dest != ScratchFloatReg);
        if (Assembler::HasSSE41()) {
            movd(operand.payloadReg(), dest);
            pinsrd(operand.typeReg(), dest);
        } else {
            movd(operand.payloadReg(), dest);
            movd(operand.typeReg(), ScratchFloatReg);
            unpcklps(ScratchFloatReg, dest);
        }
#endif
    }

    void boolValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        JS_NOT_REACHED("Codegen for boolValueToDouble NYI");
#if 0
        cvtsi2sd(operand.payloadReg(), dest);
#endif
    }
    void int32ValueToDouble(const ValueOperand &operand, const FloatRegister &dest) {
        JS_NOT_REACHED("Codegen for int32ValueToDouble NYI");
#if 0
        cvtsi2sd(operand.payloadReg(), dest);
#endif
    }

    void loadStaticDouble(const double *dp, const FloatRegister &dest) {
        JS_NOT_REACHED("Codegen for loadStaticDouble NYI");
#if 0
        movsd(dp, dest);
#endif
    }
};

typedef MacroAssemblerARM MacroAssemblerSpecific;

} // namespace ion
} // namespace js

#endif // jsion_macro_assembler_arm_h__

