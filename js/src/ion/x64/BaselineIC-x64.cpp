/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ion/BaselineJIT.h"
#include "ion/BaselineIC.h"
#include "ion/BaselineHelpers.h"
#include "ion/BaselineCompiler.h"
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

    // Directly compare the int32 payload of R0 and R1.
    masm.cmpl(R0.valueReg(), R1.valueReg());
    masm.setCC(cond, ScratchReg);
    masm.movzxbl(ScratchReg, ScratchReg);

    // Box the result and return
    masm.boxValue(JSVAL_TYPE_BOOLEAN, ScratchReg, R0.valueReg());
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

    Label revertRegister;
    switch(op_) {
      case JSOP_ADD:
        masm.unboxInt32(R1, ExtractTemp0);
        // Just jump to failure on overflow. R0 and R1 are preserved, so we can just jump to
        // the next stub.
        masm.addl(R0.valueReg(), ExtractTemp0);
        masm.j(Assembler::Overflow, &failure);

        // Box the result
        masm.boxValue(JSVAL_TYPE_INT32, ExtractTemp0, R0.valueReg());
        break;
      case JSOP_BITOR:
        // We can overide R0, because the instruction is unfailable.
        // Because the tag bits are the same, we don't need to retag.
        masm.orq(R1.valueReg(), R0.valueReg());
        break;
      case JSOP_BITXOR:
        masm.xorl(R1.valueReg(), R0.valueReg());
        masm.tagValue(JSVAL_TYPE_INT32, R0.valueReg(), R0);
        break;
      case JSOP_BITAND:
        masm.andq(R1.valueReg(), R0.valueReg());
        break;
      case JSOP_LSH:
        masm.unboxInt32(R0, ExtractTemp0);
        masm.unboxInt32(R1, ecx); // Unboxing R1 to ecx, clobbers R0.
        masm.shll_cl(ExtractTemp0);
        masm.boxValue(JSVAL_TYPE_INT32, ExtractTemp0, R0.valueReg());
        break;
      case JSOP_RSH:
        masm.unboxInt32(R0, ExtractTemp0);
        masm.unboxInt32(R1, ecx);
        masm.sarl_cl(ExtractTemp0);
        masm.boxValue(JSVAL_TYPE_INT32, ExtractTemp0, R0.valueReg());
        break;
      case JSOP_URSH:
        if (!allowDouble_)
            masm.movq(R0.valueReg(), ScratchReg);

        masm.unboxInt32(R0, ExtractTemp0);
        masm.unboxInt32(R1, ecx); // This clobbers R0

        masm.shrl_cl(ExtractTemp0);
        masm.testl(ExtractTemp0, ExtractTemp0);
        if (allowDouble_) {
            Label toUint;
            masm.j(Assembler::Signed, &toUint);

            // Box and return.
            masm.boxValue(JSVAL_TYPE_INT32, ExtractTemp0, R0.valueReg());
            EmitReturnFromIC(masm);

            masm.bind(&toUint);
            masm.convertUInt32ToDouble(ExtractTemp0, ScratchFloatReg);
            masm.boxDouble(ScratchFloatReg, R0);
        } else {
            masm.j(Assembler::Signed, &revertRegister);
            masm.boxValue(JSVAL_TYPE_INT32, ExtractTemp0, R0.valueReg());
        }
        break;
      default:
        JS_NOT_REACHED("Unhandled op in BinaryArith_Int32");
        return false;
    }

    // Return from stub.
    EmitReturnFromIC(masm);

    // Revert the content of R0 in the fallible >>> case.
    if (op_ == JSOP_URSH && !allowDouble_) {
        masm.bind(&revertRegister);
        // Restore tag and payload.
        masm.movq(ScratchReg, R0.valueReg());
        // Fall through to failure.
    }
    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}


} // namespace ion
} // namespace js
