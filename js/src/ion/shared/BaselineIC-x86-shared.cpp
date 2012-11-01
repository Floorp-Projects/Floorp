/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ion/BaselineJIT.h"
#include "ion/BaselineIC.h"
#include "ion/BaselineCompiler.h"
#include "ion/IonLinker.h"

using namespace js;
using namespace js::ion;

bool
BinaryOpCache::generateInt32(JSContext *cx, MacroAssembler &masm)
{
    Label notInt32, overflow;
    masm.branchTestInt32(Assembler::NotEqual, R0, &notInt32);
    masm.branchTestInt32(Assembler::NotEqual, R1, &notInt32);

    masm.addl(R1.payloadReg(), R0.payloadReg());
    masm.j(Assembler::Overflow, &overflow);

    masm.ret();

    // Overflow.
    masm.bind(&overflow);
    //XXX: restore R0.

    // Update cache state.
    masm.bind(&notInt32);
    generateUpdate(cx, masm);
    return true;
}

bool
CompareCache::generateInt32(JSContext *cx, MacroAssembler &masm)
{
    Label notInt32;
    masm.branchTestInt32(Assembler::NotEqual, R0, &notInt32);
    masm.branchTestInt32(Assembler::NotEqual, R1, &notInt32);

    masm.cmpl(R0.payloadReg(), R1.payloadReg());

    switch (JSOp(*data.pc)) {
      case JSOP_LT:
        masm.setCC(Assembler::LessThan, R0.payloadReg());
        break;

      default:
        JS_NOT_REACHED("Unexpected compare op");
        break;
    }

    masm.movzxbl(R0.payloadReg(), R0.payloadReg());
    masm.movl(ImmType(JSVAL_TYPE_BOOLEAN), R0.typeReg());

    masm.ret();

    // Update cache state.
    masm.bind(&notInt32);
    generateUpdate(cx, masm);
    return true;
}
