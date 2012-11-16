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

    // Compare payload regs of R0 and R1.
    masm.unboxNonDouble(R0, rdx);
    masm.unboxNonDouble(R1, ScratchReg);
    masm.cmpq(rdx, ScratchReg);
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

    // Add R0 and R1.  Don't need to explicitly unbox, just use the TailCallReg which
    // should be available.
    masm.unboxNonDouble(R0, rdx);
    masm.unboxNonDouble(R1, ScratchReg);

    switch(op) {
      case JSOP_ADD:
        masm.addl(rdx, ScratchReg);
        break;
      default:
        JS_ASSERT(!"Unhandled op for BinaryArith_Int32!");
        return false;
    }

    // Just jump to failure on overflow.  R0 and R1 are preserved, so we can just jump to
    // the next stub.
    masm.j(Assembler::Overflow, &failure);

    // Box the result and return
    masm.boxValue(JSVAL_TYPE_INT32, ScratchReg, R0.valueReg());
    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}


} // namespace ion
} // namespace js
