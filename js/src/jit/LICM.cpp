/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "Ion.h"
#include "IonBuilder.h"
#include "IonSpewer.h"
#include "LICM.h"
#include "MIR.h"
#include "MIRGraph.h"

using namespace js;
using namespace js::ion;

LICM::LICM(MIRGenerator *mir, MIRGraph &graph)
  : mir(mir), graph(graph)
{
}

bool
LICM::analyze()
{
    IonSpew(IonSpew_LICM, "Beginning LICM pass.");

    // Iterate in RPO to visit outer loops before inner loops.
    for (ReversePostorderIterator i(graph.rpoBegin()); i != graph.rpoEnd(); i++) {
        MBasicBlock *header = *i;

        // Skip non-headers and self-loops.
        if (!header->isLoopHeader() || header->numPredecessors() < 2)
            continue;

        // Attempt to optimize loop.
        Loop loop(mir, header);

        Loop::LoopReturn lr = loop.init();
        if (lr == Loop::LoopReturn_Error)
            return false;
        if (lr == Loop::LoopReturn_Skip) {
            graph.unmarkBlocks();
            continue;
        }

        if (!loop.optimize())
            return false;

        graph.unmarkBlocks();
    }

    return true;
}

Loop::Loop(MIRGenerator *mir, MBasicBlock *header)
  : mir(mir),
    header_(header)
{
    preLoop_ = header_->getPredecessor(0);
}

Loop::LoopReturn
Loop::init()
{
    IonSpew(IonSpew_LICM, "Loop identified, headed by block %d", header_->id());
    IonSpew(IonSpew_LICM, "footer is block %d", header_->backedge()->id());

    // The first predecessor of the loop header must dominate the header.
    JS_ASSERT(header_->id() > header_->getPredecessor(0)->id());

    // Loops from backedge to header and marks all visited blocks
    // as part of the loop. At the same time add all hoistable instructions
    // (in RPO order) to the instruction worklist.
    Vector<MBasicBlock *, 1, IonAllocPolicy> inlooplist;
    if (!inlooplist.append(header_->backedge()))
        return LoopReturn_Error;
    header_->backedge()->mark();

    while (!inlooplist.empty()) {
        MBasicBlock *block = inlooplist.back();

        // Hoisting requires more finesse if the loop contains a block that
        // self-dominates: there exists control flow that may enter the loop
        // without passing through the loop preheader.
        //
        // Rather than perform a complicated analysis of the dominance graph,
        // just return a soft error to ignore this loop.
        if (block->immediateDominator() == block) {
            while (!worklist_.empty())
                popFromWorklist();
            return LoopReturn_Skip;
        }

        // Add not yet visited predecessors to the inlooplist.
        if (block != header_) {
            for (size_t i = 0; i < block->numPredecessors(); i++) {
                MBasicBlock *pred = block->getPredecessor(i);
                if (pred->isMarked())
                    continue;

                if (!inlooplist.append(pred))
                    return LoopReturn_Error;
                pred->mark();
            }
        }

        // If any block was added, process them first.
        if (block != inlooplist.back())
            continue;

        // Add all instructions in this block (but the control instruction) to the worklist
        for (MInstructionIterator i = block->begin(); i != block->end(); i++) {
            MInstruction *ins = *i;

            if (isHoistable(ins)) {
                if (!insertInWorklist(ins))
                    return LoopReturn_Error;
            }
        }

        // All successors of this block are visited.
        inlooplist.popBack();
    }

    return LoopReturn_Success;
}

bool
Loop::optimize()
{
    InstructionQueue invariantInstructions;

    IonSpew(IonSpew_LICM, "These instructions are in the loop: ");

    while (!worklist_.empty()) {
        if (mir->shouldCancel("LICM (worklist)"))
            return false;

        MInstruction *ins = popFromWorklist();

        IonSpewHeader(IonSpew_LICM);

        if (IonSpewEnabled(IonSpew_LICM)) {
            ins->printName(IonSpewFile);
            fprintf(IonSpewFile, " <- ");
            ins->printOpcode(IonSpewFile);
            fprintf(IonSpewFile, ":  ");
        }

        if (isLoopInvariant(ins)) {
            // Flag this instruction as loop invariant.
            ins->setLoopInvariant();
            if (!invariantInstructions.append(ins))
                return false;

            if (IonSpewEnabled(IonSpew_LICM))
                fprintf(IonSpewFile, " Loop Invariant!\n");
        }
    }

    if (!hoistInstructions(invariantInstructions))
        return false;
    return true;
}

bool
Loop::hoistInstructions(InstructionQueue &toHoist)
{
    // Iterate in post-order (uses before definitions)
    for (int32_t i = toHoist.length() - 1; i >= 0; i--) {
        MInstruction *ins = toHoist[i];

        // Don't hoist MConstantElements, MConstant and MBox
        // if it doesn't enable us to hoist one of its uses.
        // We want those instructions as close as possible to their use.
        if (ins->isConstantElements() || ins->isConstant() || ins->isBox()) {
            bool loopInvariantUse = false;
            for (MUseDefIterator use(ins); use; use++) {
                if (use.def()->isLoopInvariant()) {
                    loopInvariantUse = true;
                    break;
                }
            }

            if (!loopInvariantUse)
                ins->setNotLoopInvariant();
        }
    }

    // Move all instructions to the preLoop_ block just before the control instruction.
    for (size_t i = 0; i < toHoist.length(); i++) {
        MInstruction *ins = toHoist[i];

        // Loads may have an implicit dependency on either stores (effectful instructions) or
        // control instructions so we should never move these.
        JS_ASSERT(!ins->isControlInstruction());
        JS_ASSERT(!ins->isEffectful());
        JS_ASSERT(ins->isMovable());

        if (!ins->isLoopInvariant())
            continue;

        if (checkHotness(ins->block())) {
            ins->block()->moveBefore(preLoop_->lastIns(), ins);
            ins->setNotLoopInvariant();
        }
    }

    return true;
}

bool
Loop::isInLoop(MDefinition *ins)
{
    return ins->block()->isMarked();
}

bool
Loop::isBeforeLoop(MDefinition *ins)
{
    return ins->block()->id() < header_->id();
}

bool
Loop::isLoopInvariant(MInstruction *ins)
{
    if (!isHoistable(ins)) {
        if (IonSpewEnabled(IonSpew_LICM))
            fprintf(IonSpewFile, "not hoistable\n");
        return false;
    }

    // Don't hoist if this instruction depends on a store inside or after the loop.
    // Note: "after the loop" can sound strange, but Alias Analysis doesn't look
    // at the control flow. Therefore it doesn't match the definition here, that a block
    // is in the loop when there is a (directed) path from the block to the loop header.
    if (ins->dependency() && !isBeforeLoop(ins->dependency())) {
        if (IonSpewEnabled(IonSpew_LICM)) {
            fprintf(IonSpewFile, "depends on store inside or after loop: ");
            ins->dependency()->printName(IonSpewFile);
            fprintf(IonSpewFile, "\n");
        }
        return false;
    }

    // An instruction is only loop invariant if it and all of its operands can
    // be safely hoisted into the loop preheader.
    for (size_t i = 0; i < ins->numOperands(); i ++) {
        if (isInLoop(ins->getOperand(i)) &&
            !ins->getOperand(i)->isLoopInvariant()) {

            if (IonSpewEnabled(IonSpew_LICM)) {
                ins->getOperand(i)->printName(IonSpewFile);
                fprintf(IonSpewFile, " is in the loop.\n");
            }

            return false;
        }
    }

    return true;
}

bool
Loop::isLoopInvariant(MDefinition *ins)
{
    if (!isInLoop(ins))
        return true;

    return ins->isInstruction() && isLoopInvariant(ins->toInstruction());
}

bool
Loop::checkHotness(MBasicBlock *block)
{
    // TODO: determine if instructions within this block are worth hoisting.
    // They might not be if the block is cold enough within the loop.
    // BUG 669517
    return true;
}

bool
Loop::insertInWorklist(MInstruction *ins)
{
    if (!worklist_.insert(worklist_.begin(), ins))
        return false;
    ins->setInWorklist();
    return true;
}

MInstruction*
Loop::popFromWorklist()
{
    MInstruction* toReturn = worklist_.popCopy();
    toReturn->setNotInWorklist();
    return toReturn;
}
