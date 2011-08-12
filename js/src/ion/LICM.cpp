/*
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
 *   Andrew Scheff <ascheff@mozilla.com>
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

#include <stdio.h>

#include "MIR.h"
#include "MIRGraph.h"
#include "Ion.h"
#include "LICM.h"
#include "IonSpewer.h"

using namespace js;
using namespace js::ion;

LICM::LICM(MIRGraph &graph)
  : graph(graph)
{
}

bool
LICM::analyze()
{
    IonSpew(IonSpew_LICM, "Beginning LICM pass ...");
    // In reverse post order, look for loops:
    for (ReversePostorderIterator i(graph.rpoBegin()); i != graph.rpoEnd(); i++) {
        MBasicBlock *header = *i;
        // If we find a loop, make sure that it's actually a loop and not one with an empty body.
        // Loops with empty bodies aren't worth it, obviously.
        if (header->isLoopHeader() && header->numPredecessors() > 1) {
            // The loop footer is the last predecessor of the loop header.
            MBasicBlock *footer = header->getPredecessor(header->numPredecessors() - 1);

            // Construct a loop object on this backedge and attempt to optimize
            Loop loop(footer, header, graph);
            if (!loop.init())
                return false;
            if (!loop.optimize())
                return false;
        }
    }

    return true;
}

Loop::Loop(MBasicBlock *footer, MBasicBlock *header, MIRGraph &graph)
  : graph(graph),
    footer_(footer),
    header_(header)
{
    preLoop_ = header_->getPredecessor(0);
}

bool
Loop::init()
{
    IonSpew(IonSpew_LICM, "Loop identified, headed by block %d", header_->id());
    // The first predecessor of the loop header must be the only predecessor of the
    // loop header that is outside of the loop
    JS_ASSERT(header_->id() > header_->getPredecessor(0)->id());
#ifdef DEBUG
    for (size_t i = 1; i < header_->numPredecessors(); i ++) {
        JS_ASSERT(header_->id() <= header_->getPredecessor(i)->id());
    }
#endif

    IonSpew(IonSpew_LICM, "footer is block %d", footer_->id());

    if (!iterateLoopBlocks(footer_))
        return false;

    graph.unmarkBlocks();
    return true;
}

bool
Loop::iterateLoopBlocks(MBasicBlock *current)
{
    // This block has been visited
    current->mark();

    // If we haven't reached the loop header yet, recursively explore predecessors
    // if we haven't seen them already.
    if (current != header_) {
        for (size_t i = 0; i < current->numPredecessors(); i++) {
            if (current->getPredecessor(i)->isMarked())
                continue;
            if (!iterateLoopBlocks(current->getPredecessor(i)))
                return false;
        }
    }

    // Add all instructions in this block (but the control instruction) to the worklist
    for (MInstructionIterator i = current->begin(); i != current->end(); i ++) {
        MInstruction *ins = *i;

        if (ins->isIdempotent()) {
            if (!insertInWorklist(ins))
                return false;
        }
    }
    return true;
}

bool
Loop::optimize()
{
    InstructionQueue invariantInstructions;
    IonSpew(IonSpew_LICM, "These instructions are in the loop: ");

    while (!worklist_.empty()) {
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

            // Loop through uses of invariant instruction and add back to work list.
            for (MUseDefIterator iter(ins->toDefinition()); iter; iter++) {
                MDefinition *consumer = iter.def();

                if (consumer->isInWorklist())
                    continue;

                // if the consumer of this invariant instruction is in the
                // loop, and it is also worth hoisting, then process it.
                if (isInLoop(consumer) && isHoistable(consumer)) {
                    if (!insertInWorklist(consumer->toInstruction()))
                        return false;
                }
            }

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
    // Move all instructions to the preLoop_ block just before the control instruction.
    for (size_t i = 0; i < toHoist.length(); i++) {
        MInstruction *ins = toHoist[i];
        if (checkHotness(ins->block())) {
            ins->block()->remove(ins);
            preLoop_->insertBefore(preLoop_->lastIns(), ins);
            ins->setNotLoopInvariant();
        }
    }
    return true;
}

bool
Loop::isInLoop(MDefinition *ins)
{
    return ins->block()->id() >= header_->id();
}

bool
Loop::isLoopInvariant(MInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i ++) {

        // If the operand is in the loop and not loop invariant itself...
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

