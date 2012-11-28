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
    Assembler::Condition notcond;
    switch(op) {
      case JSOP_LT:
        cond = Assembler::LessThan;
        break;
      case JSOP_GT:
        cond = Assembler::GreaterThan;
        break;
      default:
        JS_NOT_REACHED("Unhandled op for ICCompare_Int32.");
        return false;
    }

    // Guard that R0 is an integer and R1 is an integer.
    Label failure;
    masm.branchTestInt32(Assembler::NotEqual, R0, &failure);
    masm.branchTestInt32(Assembler::NotEqual, R1, &failure);

    // Compare payload regs of R0 and R1.
    masm.cmp32(R0.payloadReg(), R1.payloadReg());
    masm.ma_mov(Imm32(1), R0.payloadReg(), NoSetCond, cond);
    masm.ma_mov(Imm32(0), R0.payloadReg(), NoSetCond, Assembler::InvertCondition(cond));

    // Result is implicitly boxed already.
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

    // Add R0 and R1.  Don't need to explicitly unbox, just use R2's payloadReg.
    Register scratchReg = R2.payloadReg();

    switch(op_) {
      case JSOP_ADD:
        masm.ma_add(R0.payloadReg(), R1.payloadReg(), scratchReg);

        // Just jump to failure on overflow.  R0 and R1 are preserved, so we can just jump to
        // the next stub.
        masm.j(Assembler::Overflow, &failure);

        // Box the result and return.  We know R0.typeReg() already contains the integer
        // tag, so we just need to move the result value into place.
        masm.movePtr(scratchReg, R0.payloadReg());
        break;
      case JSOP_BITOR:
        masm.ma_orr(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_BITXOR:
        masm.ma_eor(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_BITAND:
        masm.ma_and(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_LSH:
        // ARM will happily try to shift by more than 0x1f.
        masm.ma_and(Imm32(0x1F), R1.payloadReg(), R1.payloadReg());
        masm.ma_lsl(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_RSH:
        masm.ma_and(Imm32(0x1F), R1.payloadReg(), R1.payloadReg());
        masm.ma_asr(R1.payloadReg(), R0.payloadReg(), R0.payloadReg());
        break;
      case JSOP_URSH:
        masm.ma_and(Imm32(0x1F), R1.payloadReg(), scratchReg);
        masm.ma_lsr(scratchReg, R0.payloadReg(), scratchReg);
        masm.ma_cmp(scratchReg, Imm32(0));
        masm.j(Assembler::LessThan, &failure);
        // Move result for return.
        masm.movePtr(scratchReg, R0.payloadReg());
        break;
      default:
        JS_NOT_REACHED("Unhandled op for BinaryArith_Int32.");
        return false;
    }

    EmitReturnFromIC(masm);

    // Failure case - jump to next stub
    masm.bind(&failure);
    EmitStubGuardFailure(masm);

    return true;
}


} // namespace ion
} // namespace js
