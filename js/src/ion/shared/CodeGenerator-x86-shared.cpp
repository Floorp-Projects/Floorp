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
 *   David Anderson <danderson@mozilla.com>
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
#include "CodeGenerator-x86-shared.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
#include "CodeGenerator-shared-inl.h"

using namespace js;
using namespace js::ion;

CodeGeneratorX86Shared::CodeGeneratorX86Shared(MIRGenerator *gen, LIRGraph &graph)
  : CodeGeneratorShared(gen, graph),
    frameDepth_(graph.stackHeight() * sizeof(STACK_SLOT_SIZE)),
    framePushed_(0)
{

}

bool
CodeGeneratorX86Shared::generatePrologue()
{
    masm.reserveStack(frameDepth_);
    return true;
}

bool
CodeGeneratorX86Shared::visitLabel(LLabel *label)
{
    masm.bind(label->label());
    return true;
}

bool
CodeGeneratorX86Shared::visitGoto(LGoto *jump)
{
    LBlock *target = jump->target()->lir();
    LLabel *header = target->begin()->toLabel();

    // Don't bother emitting a jump if we'll flow through to the next block.
    if (current->mir()->id() + 1 == target->mir()->id())
        return true;

    masm.jmp(header->label());
    return true;
}
    
bool
CodeGeneratorX86Shared::visitAddI(LAddI *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);

    if (rhs->isConstant())
        masm.addl(Imm32(ToInt32(rhs)), ToOperand(lhs));
    else
        masm.addl(ToOperand(rhs), ToRegister(lhs));

    return true;
}

bool
CodeGeneratorX86Shared::visitBitOp(LBitOp *ins)
{
    const LAllocation *lhs = ins->getOperand(0);
    const LAllocation *rhs = ins->getOperand(1);

    switch (ins->bitop()) {
        case JSOP_BITOR:
            if (rhs->isConstant())
                masm.orl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.orl(ToOperand(rhs), ToRegister(lhs));
            break;
        case JSOP_BITXOR:
            if (rhs->isConstant())
                masm.xorl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.xorl(ToOperand(rhs), ToRegister(lhs));
            break;
        case JSOP_BITAND:
            if (rhs->isConstant())
                masm.andl(Imm32(ToInt32(rhs)), ToOperand(lhs));
            else
                masm.andl(ToOperand(rhs), ToRegister(lhs));
            break;
        default:
            JS_NOT_REACHED("unexpected binary opcode");
    }

    return true;
}

