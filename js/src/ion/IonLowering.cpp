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

#include "IonLIR.h"
#include "IonLowering.h"
#include "IonLowering-inl.h"
#include "MIR.h"
#include "MIRGraph.h"

using namespace js;
using namespace ion;

bool
LIRGenerator::emitAtUses(MInstruction *mir)
{
    mir->setEmitAtUses();
    mir->setId(0);
    return true;
}

LUse
LIRGenerator::use(MInstruction *mir, LUse policy)
{
    // It is illegal to call use() on an instruction with two defs.
#if BOX_PIECES > 1
    JS_ASSERT(mir->type() != MIRType_Value);
#endif
    if (!ensureDefined(mir))
        return policy;
    policy.setVirtualRegister(mir->id());
    return policy;
}

bool
LIRGenerator::assignSnapshot(LInstruction *ins)
{
    LSnapshot *snapshot = LSnapshot::New(gen, last_snapshot_);
    if (!snapshot)
        return false;
    fillSnapshot(snapshot);
    ins->assignSnapshot(snapshot);
    return true;
}

bool
LIRGenerator::visitConstant(MConstant *ins)
{
    const Value &v = ins->value();
    switch (ins->type()) {
      case MIRType_Boolean:
        return define(new LInteger(v.toBoolean()), ins);
      case MIRType_Int32:
        return define(new LInteger(v.toInt32()), ins);
      case MIRType_Double:
        return define(new LDouble(v.toDouble()), ins);
      case MIRType_String:
        return define(new LPointer(v.toString()), ins);
      case MIRType_Object:
        return define(new LPointer(&v.toObject()), ins);
      default:
        // Constants of special types (undefined, null) should never flow into
        // here directly. Operations blindly consuming them require a Box.
        JS_NOT_REACHED("unexepcted constant type");
        return false;
    }
    return true;
}

bool
LIRGenerator::visitParameter(MParameter *param)
{
    ptrdiff_t offset;
    if (param->index() == -2)
        offset = StackFrame::offsetOfCallee(gen->fun());
    else if (param->index() == -1)
        offset = StackFrame::offsetOfThis(gen->fun());
    else
        offset = StackFrame::offsetOfFormalArg(gen->fun(), param->index());

    JS_ASSERT(offset % sizeof(Value) == 0);
    JS_ASSERT(offset % STACK_SLOT_SIZE == 0);

    LParameter *ins = new LParameter;
    if (!defineBox(ins, param, LDefinition::PRESET))
        return false;

    offset /= STACK_SLOT_SIZE;
#if defined(JS_NUNBOX32)
# if defined(IS_BIG_ENDIAN)
    ins->getDef(0)->setOutput(LArgument(offset));
    ins->getDef(1)->setOutput(LArgument(offset + 1));
# else
    ins->getDef(0)->setOutput(LArgument(offset + 1));
    ins->getDef(1)->setOutput(LArgument(offset));
# endif
#elif defined(JS_PUNBOX64)
    ins->getDef(0)->setOutput(LArgument(offset));
#endif

    return true;
}

bool
LIRGenerator::visitGoto(MGoto *ins)
{
    return add(new LGoto(ins->target()));
}

bool
LIRGenerator::visitTest(MTest *test)
{
    MInstruction *opd = test->getInput(0);
    MBasicBlock *ifTrue = test->ifTrue();
    MBasicBlock *ifFalse = test->ifFalse();

    if (opd->type() == MIRType_Value) {
        LTestVAndBranch *lir = new LTestVAndBranch(ifTrue, ifFalse);
        if (!fillBoxUses(lir, 0, opd))
            return false;
        return add(lir);
    }

    // These must be explicitly sniffed out since they are constants and have
    // no payload.
    if (opd->type() == MIRType_Undefined || opd->type() == MIRType_Null)
        return add(new LGoto(ifFalse));

    if (opd->type() == MIRType_Double)
        return add(new LTestDAndBranch(useRegister(opd), temp(LDefinition::DOUBLE), ifTrue, ifFalse));

    return add(new LTestIAndBranch(useRegister(opd), ifTrue, ifFalse));
}

static void
ReorderCommutative(MInstruction **lhsp, MInstruction **rhsp)
{
    MInstruction *lhs = *lhsp;
    MInstruction *rhs = *rhsp;

    // Put the constant in the left-hand side, if there is one.
    if (lhs->isConstant()) {
        *rhsp = lhs;
        *lhsp = rhs;
    }
}

bool
LIRGenerator::doBitOp(JSOp op, MInstruction *ins)
{
    MInstruction *lhs = ins->getInput(0);
    MInstruction *rhs = ins->getInput(1);

    if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
        ReorderCommutative(&lhs, &rhs);
        LBitOp *bitop = new LBitOp(op, useRegister(lhs), useOrConstant(rhs));
        return defineReuseInput(bitop, ins);
    }

    JS_NOT_REACHED("NYI");
    return false;
}

bool
LIRGenerator::visitBitAnd(MBitAnd *ins)
{
    return doBitOp(JSOP_AND, ins);
}

bool
LIRGenerator::visitAdd(MAdd *ins)
{
    MInstruction *lhs = ins->getInput(0);
    MInstruction *rhs = ins->getInput(1);

    if (lhs->type() == MIRType_Int32 && rhs->type() == MIRType_Int32) {
        ReorderCommutative(&lhs, &rhs);
        return lowerForALU(new LAddI, ins, lhs, rhs);
    }

    JS_NOT_REACHED("NYI");
    return false;
}

bool
LIRGenerator::visitStart(MStart *start)
{
    // This is a no-op.
    return true;
}

bool
LIRGenerator::visitCopy(MCopy *ins)
{
    JS_NOT_REACHED("unexpected copy");
    return false;
}

bool
LIRGenerator::lowerPhi(MPhi *ins)
{
    // The virtual register of the phi was determined in the first pass.
    JS_ASSERT(ins->id());

    LPhi *phi = LPhi::New(gen, ins);
    if (!phi)
        return false;
    for (size_t i = 0; i < ins->numOperands(); i++) {
        MInstruction *opd = ins->getInput(i);
        phi->setOperand(i, LUse(opd->id(), LUse::ANY));
    }
    phi->setDef(0, LDefinition(ins->id(), LDefinition::TypeFrom(ins->type())));
    return addPhi(phi);
}

bool
LIRGenerator::visitPhi(MPhi *ins)
{
    JS_NOT_REACHED("Not used.");
    return true;
}

bool
LIRGenerator::visitBox(MBox *ins)
{
    JS_NOT_REACHED("Must be implemented by arch");
    return false;
}

bool
LIRGenerator::visitUnbox(MUnbox *ins)
{
    JS_NOT_REACHED("Must be implemented by arch");
    return false;
}

bool
LIRGenerator::visitReturn(MReturn *ins)
{
    JS_NOT_REACHED("Must be implemented by arch");
    return false;
}

bool
LIRGenerator::visitSnapshot(MSnapshot *snapshot)
{
    JS_ASSERT(!snapshot->inWorklist());
    last_snapshot_ = snapshot;
    last_snapshot_->setInWorklist();
    return true;
}

void
LIRGenerator::rewriteDefsInSnapshots(MInstruction *ins, MInstruction *old)
{
    MUseIterator iter(old);
    while (iter.more()) {
        MInstruction *use = iter->ins();
        if (!ins->isSnapshot() || ins->inWorklist()) {
            iter.next();
            continue;
        }
        use->replaceOperand(iter, ins);
    }
}

bool
LIRGenerator::visitBlock(MBasicBlock *block)
{
    current = LBlock::New(block);
    if (!current)
        return false;

    last_snapshot_ = block->entrySnapshot();

    for (size_t i = 0; i < block->numPhis(); i++) {
        if (!gen->ensureBallast())
            return false;
        if (!preparePhi(block->getPhi(i)))
            return false;
#ifdef DEBUG
        block->getPhi(i)->setInWorklist();
#endif
    }

    for (MInstructionIterator iter = block->begin(); iter != block->end(); iter++) {
        if (!gen->ensureBallast())
            return false;
        if (iter->rewritesDef())
            rewriteDefsInSnapshots(*iter, iter->rewrittenDef());
        if (!iter->accept(this))
            return false;
        if (gen->errored())
            return false;
#ifdef DEBUG
        iter->setInWorklistUnchecked();
#endif
    }

    // For each successor, make sure we've assigned a virtual register to any
    // phi inputs that emit at uses.
    if (block->successorWithPhis()) {
        MBasicBlock *successor = block->successorWithPhis();
        uint32 position = block->positionInPhiSuccessor();
        for (size_t i = 0; i < successor->numPhis(); i++) {
            MPhi *phi = successor->getPhi(i);
            MInstruction *opd = phi->getInput(position);
            if (opd->emitAtUses() && !opd->id()) {
                if (!ensureDefined(opd))
                    return false;
            }
        }
    }

    block->assignLir(current);
    return true;
}

bool
LIRGenerator::generate()
{
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        if (!visitBlock(graph.getBlock(i)))
            return false;
    }

    // Emit phis now that all their inputs have definitions.
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
        current = block->lir();
        for (size_t j = 0; j < block->numPhis(); j++) {
            if (!block->getPhi(j)->accept(this))
                return false;
        }
    }

    return true;
}

