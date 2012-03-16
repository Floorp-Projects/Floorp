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

#include "MacroAssembler-x86.h"
#include "ion/MoveEmitter.h"
#include "ion/IonFrames.h"

using namespace js;
using namespace js::ion;

void
MacroAssemblerX86::setupABICall(uint32 args)
{
    JS_ASSERT(!inCall_);
    inCall_ = true;

    args_ = args;
    passedArgs_ = 0;
    stackForCall_ = 0;
}

void
MacroAssemblerX86::setupAlignedABICall(uint32 args)
{
    setupABICall(args);
    dynamicAlignment_ = false;
}

void
MacroAssemblerX86::setupUnalignedABICall(uint32 args, const Register &scratch)
{
    setupABICall(args);
    dynamicAlignment_ = true;

    movl(esp, scratch);
    andl(Imm32(~(StackAlignment - 1)), esp);
    push(scratch);
}

void
MacroAssemblerX86::passABIArg(const MoveOperand &from)
{
    ++passedArgs_;
    MoveOperand to = MoveOperand(StackPointer, stackForCall_);
    if (from.isDouble()) {
        stackForCall_ += sizeof(double);
        enoughMemory_ &= moveResolver_.addMove(from, to, Move::DOUBLE);
    } else {
        stackForCall_ += sizeof(int32_t);
        enoughMemory_ &= moveResolver_.addMove(from, to, Move::GENERAL);
    }
}

void
MacroAssemblerX86::passABIArg(const Register &reg)
{
    passABIArg(MoveOperand(reg));
}

void
MacroAssemblerX86::passABIArg(const FloatRegister &reg)
{
    passABIArg(MoveOperand(reg));
}

void
MacroAssemblerX86::callWithABI(void *fun, Result result)
{
    JS_ASSERT(inCall_);
    JS_ASSERT(args_ == passedArgs_);

    uint32 stackAdjust;
    if (dynamicAlignment_) {
        stackAdjust = stackForCall_
                    + ComputeByteAlignment(stackForCall_ + STACK_SLOT_SIZE,
                                           StackAlignment);
    } else {
        stackAdjust = stackForCall_
                    + ComputeByteAlignment(stackForCall_ + framePushed_,
                                           StackAlignment);
    }

    reserveStack(stackAdjust);

    // Position all arguments.
    {
        enoughMemory_ &= moveResolver_.resolve();
        if (!enoughMemory_)
            return;

        MoveEmitter emitter(*this);
        emitter.emit(moveResolver_);
        emitter.finish();
    }

#ifdef DEBUG
    {
        // Check call alignment.
        Label good;
        movl(esp, eax);
        testl(eax, Imm32(StackAlignment - 1));
        j(Equal, &good);
        breakpoint();
        bind(&good);
    }
#endif

    call(ImmWord(fun));

    freeStack(stackAdjust);
    if (result == DOUBLE) {
        reserveStack(sizeof(double));
        fstp(Operand(esp, 0));
        movsd(Operand(esp, 0), ReturnFloatReg);
        breakpoint();
        freeStack(sizeof(double));
    }
    if (dynamicAlignment_)
        pop(esp);

    JS_ASSERT(inCall_);
    inCall_ = false;
}

void
MacroAssemblerX86::handleException()
{
    // Reserve space for exception information.
    subl(Imm32(sizeof(ResumeFromException)), esp);
    movl(esp, eax);

    // Ask for an exception handler.
    setupUnalignedABICall(1, ecx);
    passABIArg(eax);
    callWithABI(JS_FUNC_TO_DATA_PTR(void *, ion::HandleException));
    
    // Load the error value, load the new stack pointer, and return.
    moveValue(MagicValue(JS_ION_ERROR), JSReturnOperand);
    movl(Operand(esp, offsetof(ResumeFromException, stackPointer)), esp);
    ret();
}

void
MacroAssemblerX86::branchTestValue(Condition cond, const ValueOperand &value, const Value &v, Label *label)
{
    jsval_layout jv = JSVAL_TO_IMPL(v);
    if (v.isMarkable())
        cmpl(value.payloadReg(), ImmGCPtr(reinterpret_cast<gc::Cell *>(v.toGCThing())));
    else
        cmpl(value.payloadReg(), Imm32(jv.s.payload.i32));

    if (cond == Equal) {
        Label done;
        j(NotEqual, &done);
        {
            cmpl(value.typeReg(), Imm32(jv.s.tag));
            j(Equal, label);
        }
        bind(&done);
    } else {
        JS_ASSERT(cond == NotEqual);
        j(NotEqual, label);

        cmpl(value.typeReg(), Imm32(jv.s.tag));
        j(NotEqual, label);
    }
}
