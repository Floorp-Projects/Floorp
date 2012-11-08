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

IonCode *
ICCompare_Int32::Compiler::generateStubCode()
{
    // The condition to test on depends on the opcode
    Assembler::Condition cond;
    switch(op) {
      case JSOP_LT: cond = Assembler::LessThan; break;
      case JSOP_GT: cond = Assembler::GreaterThan; break;
      default:
        JS_ASSERT(!"Unhandled op for ICCompare_Int32!");
        return NULL;
    }

    MacroAssembler masm;

    // Guard that R0 is an integer and R1 is an integer.
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    // Compare payload regs of R0 and R1.
    Register scratchReg = BaselineTailCallReg;
    masm.cmpl(R0.payloadReg(), R1.payloadReg());
    masm.setCC(cond, R0.payloadReg());
    masm.movzxbl(R0.payloadReg(), R0.payloadReg());

    // Box the result and return
    masm.boxNonDouble(JSVAL_TYPE_BOOLEAN, R0.payloadReg(), R0);
    masm.ret();

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    Linker linker(masm);
    return linker.newCode(cx);
}

// ICBinaryArith_Int32

IonCode *
ICBinaryArith_Int32::Compiler::generateStubCode()
{
    MacroAssembler masm;

    // Guard that R0 is an integer and R1 is an integer.
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    // Add R0 and R1.  Don't need to explicitly unbox, just use the TailCallReg which
    // should be available.
    Register scratchReg = BaselineTailCallReg;

    switch(op) {
      case JSOP_ADD:
        masm.movl(R1.payloadReg(), scratchReg);
        masm.addl(R0.payloadReg(), scratchReg);
        break;
      default:
        JS_ASSERT(!"Unhandled op for BinaryArith_Int32!");
        return NULL;
    }

    // Just jump to failure on overflow.  R0 and R1 are preserved, so we can just jump to
    // the next stub.
    masm.j(Assembler::Overflow, &failure);

    // Box the result and return
    masm.boxNonDouble(JSVAL_TYPE_INT32, scratchReg, R0);
    masm.ret();

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    Linker linker(masm);
    return linker.newCode(cx);
}


} // namespace ion
} // namespace js
