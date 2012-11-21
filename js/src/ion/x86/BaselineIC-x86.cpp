/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ion/BaselineJIT.h"
#include "ion/BaselineIC.h"
#include "ion/BaselineCompiler.h"
#include "ion/BaselineHelpers.h"
#include "ion/IonLinker.h"

using namespace js;
using namespace js::ion;

namespace js {
namespace ion {

// ICCompare_Int32

bool
ICCompare_Int32::Compiler::generateStubCode(MacroAssembler &masm)
{
    // The condition to test on depends on the opcode
    Assembler::Condition cond;
    switch(op) {
      case JSOP_LT: cond = Assembler::LessThan; break;
      case JSOP_GT: cond = Assembler::GreaterThan; break;
      default:
        JS_ASSERT(!"Unhandled op for ICCompare_Int32!");
        return false;
    }

    // Guard that R0 is an integer and R1 is an integer.
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    // Compare payload regs of R0 and R1.
    masm.cmpl(R0.payloadReg(), R1.payloadReg());
    masm.setCC(cond, R0.payloadReg());
    masm.movzxbl(R0.payloadReg(), R0.payloadReg());

    // Box the result and return
    masm.tagValue(JSVAL_TYPE_BOOLEAN, R0.payloadReg(), R0);
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}

// ICBinaryArith_Int32

bool
ICBinaryArith_Int32::Compiler::generateStubCode(MacroAssembler &masm)
{
    // Guard that R0 is an integer and R1 is an integer.
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    // Add R0 and R1.  Don't need to explicitly unbox, just use the TailCallReg which
    // should be available.
    Register scratchReg = BaselineTailCallReg;

    Label revertRegister;
    switch(op_) {
      case JSOP_ADD:
        // Add R0 and R1.  Don't need to explicitly unbox.
        masm.movl(R1.payloadReg(), scratchReg);
        masm.addl(R0.payloadReg(), scratchReg);

        // Just jump to failure on overflow.  R0 and R1 are preserved, so we can just jump to
        // the next stub.
        masm.j(Assembler::Overflow, &failure);

        // Just overwrite the payload, the tag is still fine.
        masm.movl(scratchReg, R0.payloadReg());
        break;
      case JSOP_BITOR:
        // We can overide R0, because the instruction is unfailable.
        // The R0.typeReg() is also still intact.
        masm.orl(R1.payloadReg(), R0.payloadReg());
        break;
      case JSOP_BITXOR:
        masm.xorl(R1.payloadReg(), R0.payloadReg());
        break;
      case JSOP_BITAND:
        masm.andl(R1.payloadReg(), R0.payloadReg());
        break;
      case JSOP_LSH:
        // RHS needs to be in ecx for shift operations.
        JS_ASSERT(R0.typeReg() == ecx);
        masm.movl(R1.payloadReg(), ecx);
        masm.shll_cl(R0.payloadReg());
        // We need to tag again, because we overwrote it.
        masm.tagValue(JSVAL_TYPE_INT32, R0.payloadReg(), R0);
        break;
      case JSOP_RSH:
        masm.movl(R1.payloadReg(), ecx);
        masm.sarl_cl(R0.payloadReg());
        masm.tagValue(JSVAL_TYPE_INT32, R0.payloadReg(), R0);
        break;
      case JSOP_URSH:
        if (!allowDouble_)
            masm.movl(R0.payloadReg(), scratchReg);

        masm.movl(R1.payloadReg(), ecx);
        masm.shrl_cl(R0.payloadReg());
        masm.testl(R0.payloadReg(), R0.payloadReg());
        if (allowDouble_) {
            Label toUint;
            masm.j(Assembler::Signed, &toUint);

            // Box and return.
            masm.tagValue(JSVAL_TYPE_INT32, R0.payloadReg(), R0);
            EmitReturnFromIC(masm);

            masm.bind(&toUint);
            masm.convertUInt32ToDouble(R0.payloadReg(), ScratchFloatReg);
            masm.boxDouble(ScratchFloatReg, R0);
        } else {
            masm.j(Assembler::Signed, &revertRegister);
            masm.tagValue(JSVAL_TYPE_INT32, R0.payloadReg(), R0);
        }
        break;
      default:
       JS_NOT_REACHED("Unhandled op for BinaryArith_Int32.  ");
       return NULL;
    }

    // Return.
    EmitReturnFromIC(masm);

    // Revert the content of R0 in the fallible >>> case.
    if (op_ == JSOP_URSH && !allowDouble_) {
        masm.bind(&revertRegister);
        masm.tagValue(JSVAL_TYPE_INT32, scratchReg, R0);
        // Fall through to failure.
    }
    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}


} // namespace ion
} // namespace js
