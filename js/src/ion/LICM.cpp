/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "Ion.h"
#include "IonSpewer.h"
#include "LICM.h"
#include "MIR.h"
#include "MIRGraph.h"

using namespace js;
using namespace js::ion;

bool
ion::ExtractLinearInequality(MTest *test, BranchDirection direction,
                             LinearSum *plhs, MDefinition **prhs, bool *plessEqual)
{
    if (!test->getOperand(0)->isCompare())
        return false;

    MCompare *compare = test->getOperand(0)->toCompare();

    MDefinition *lhs = compare->getOperand(0);
    MDefinition *rhs = compare->getOperand(1);

    if (compare->specialization() != MIRType_Int32)
        return false;

    JS_ASSERT(lhs->type() == MIRType_Int32);
    JS_ASSERT(rhs->type() == MIRType_Int32);

    JSOp jsop = compare->jsop();
    if (direction == FALSE_BRANCH)
        jsop = analyze::NegateCompareOp(jsop);

    LinearSum lsum = ExtractLinearSum(lhs);
    LinearSum rsum = ExtractLinearSum(rhs);

    if (!SafeSub(lsum.constant, rsum.constant, &lsum.constant))
        return false;

    // Normalize operations to use <= or >=.
    switch (jsop) {
      case JSOP_LE:
        *plessEqual = true;
        break;
      case JSOP_LT:
        /* x < y ==> x + 1 <= y */
        if (!SafeAdd(lsum.constant, 1, &lsum.constant))
            return false;
        *plessEqual = true;
        break;
      case JSOP_GE:
        *plessEqual = false;
        break;
      case JSOP_GT:
        /* x > y ==> x - 1 >= y */
        if (!SafeSub(lsum.constant, 1, &lsum.constant))
            return false;
        *plessEqual = false;
        break;
      default:
        return false;
    }

    *plhs = lsum;
    *prhs = rsum.term;

    return true;
}

LICM::LICM(MIRGraph &graph)
  : graph(graph)
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
        Loop loop(header->backedge(), header, graph);

        Loop::LoopReturn lr = loop.init();
        if (lr == Loop::LoopReturn_Error)
            return false;
        if (lr == Loop::LoopReturn_Skip)
            continue;

        if (!loop.optimize())
            return false;
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

Loop::LoopReturn
Loop::init()
{
    IonSpew(IonSpew_LICM, "Loop identified, headed by block %d", header_->id());
    IonSpew(IonSpew_LICM, "footer is block %d", footer_->id());

    // The first predecessor of the loop header must dominate the header.
    JS_ASSERT(header_->id() > header_->getPredecessor(0)->id());

    LoopReturn lr = iterateLoopBlocks(footer_);
    if (lr == LoopReturn_Error)
        return LoopReturn_Error;

    graph.unmarkBlocks();
    return lr;
}

Loop::LoopReturn
Loop::iterateLoopBlocks(MBasicBlock *current)
{
    // Visited.
    current->mark();

    // Hoisting requires more finesse if the loop contains a block that
    // self-dominates: there exists control flow that may enter the loop
    // without passing through the loop preheader.
    //
    // Rather than perform a complicated analysis of the dominance graph,
    // just return a soft error to ignore this loop.
    if (current->immediateDominator() == current)
        return LoopReturn_Skip;

    // If we haven't reached the loop header yet, recursively explore predecessors
    // if we haven't seen them already.
    if (current != header_) {
        for (size_t i = 0; i < current->numPredecessors(); i++) {
            if (current->getPredecessor(i)->isMarked())
                continue;
            LoopReturn lr = iterateLoopBlocks(current->getPredecessor(i));
            if (lr != LoopReturn_Success)
                return lr;
        }
    }

    // Add all instructions in this block (but the control instruction) to the worklist
    for (MInstructionIterator i = current->begin(); i != current->end(); i++) {
        MInstruction *ins = *i;

        if (ins->isMovable() && !ins->isEffectful()) {
            if (!insertInWorklist(ins))
                return LoopReturn_Error;
        }
    }
    return LoopReturn_Success;
}

bool
Loop::optimize()
{
    InstructionQueue invariantInstructions;
    InstructionQueue boundsChecks;

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
        } else if (ins->isBoundsCheck()) {
            if (!boundsChecks.append(ins))
                return false;
        }
    }

    if (!hoistInstructions(invariantInstructions, boundsChecks))
        return false;
    return true;
}

bool
Loop::hoistInstructions(InstructionQueue &toHoist, InstructionQueue &boundsChecks)
{
    // Hoist bounds checks first, so that hoistBoundsCheck can test for
    // invariant instructions, but delay actual insertion until the end to
    // handle dependencies on loop invariant instructions.
    InstructionQueue hoistedChecks;
    for (size_t i = 0; i < boundsChecks.length(); i++) {
        MBoundsCheck *ins = boundsChecks[i]->toBoundsCheck();
        if (isLoopInvariant(ins) || !isInLoop(ins))
            continue;

        // Try to find a test dominating the bounds check which can be
        // transformed into a hoistable check. Stop after the first such check
        // which could be transformed (the one which will be the closest to the
        // access in the source).
        MBasicBlock *block = ins->block();
        while (true) {
            BranchDirection direction;
            MTest *branch = block->immediateDominatorBranch(&direction);
            if (branch) {
                MInstruction *upper, *lower;
                tryHoistBoundsCheck(ins, branch, direction, &upper, &lower);
                if (upper && !hoistedChecks.append(upper))
                    return false;
                if (lower && !hoistedChecks.append(lower))
                    return false;
                if (upper || lower) {
                    // Note: replace all uses of the original bounds check with the
                    // actual index. This is usually done during bounds check elimination,
                    // but in this case it's safe to do it here since the load/store is
                    // definitely not loop-invariant, so we will never move it before
                    // one of the bounds checks we just added.
                    ins->replaceAllUsesWith(ins->index());
                    ins->block()->discard(ins);
                    break;
                }
            }
            MBasicBlock *dom = block->immediateDominator();
            if (dom == block)
                break;
            block = dom;
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

        if (checkHotness(ins->block())) {
            ins->block()->moveBefore(preLoop_->lastIns(), ins);
            ins->setNotLoopInvariant();
        }
    }

    for (size_t i = 0; i < hoistedChecks.length(); i++) {
        MInstruction *ins = hoistedChecks[i];
        preLoop_->insertBefore(preLoop_->lastIns(), ins);
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
    if (!isHoistable(ins))
        return false;

    // Don't hoist if this instruction depends on a store inside the loop.
    if (ins->dependency() && isInLoop(ins->dependency())) {
        if (IonSpewEnabled(IonSpew_LICM))
            fprintf(IonSpewFile, "depends on store inside loop.\n");
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

// Try to compute hoistable checks for the upper and lower bound on ins,
// according to a test in the loop which dominates ins.
//
// Given a bounds check within a loop which is not loop invariant, we would
// like to compute loop invariant bounds checks which imply that the inner
// check will succeed.  These invariant checks can then be added to the
// preheader, and the inner check eliminated.
//
// Example:
//
// for (i = v; i < n; i++)
//   x[i] = 0;
//
// There are two constraints captured by the bounds check here: i >= 0, and
// i < length(x).  'i' is not loop invariant, but we can still hoist these
// checks:
//
// - At the point of the check, it is known that i < n.  Given this,
//   if n <= length(x) then i < length(x), and since n and length(x) are loop
//   invariant the former condition can be hoisted and the i < length(x) check
//   removed.
//
// - i is only incremented within the loop, so if its initial value is >= 0
//   then all its values within the loop will also be >= 0.  The lower bounds
//   check can be hoisted as v >= 0.
//
// tryHoistBoundsCheck encodes this logic.  Given a bounds check B and a test T
// in the loop dominating that bounds check, where B and T share a non-invariant
// term lhs, a new check C is computed such that T && C imply B.
void
Loop::tryHoistBoundsCheck(MBoundsCheck *ins, MTest *test, BranchDirection direction,
                          MInstruction **pupper, MInstruction **plower)
{
    *pupper = NULL;
    *plower = NULL;

    if (!isLoopInvariant(ins->length()))
        return;

    LinearSum lhs(NULL, 0);
    MDefinition *rhs;
    bool lessEqual;
    if (!ExtractLinearInequality(test, direction, &lhs, &rhs, &lessEqual))
        return;

    // Ensure the rhs is a loop invariant term.
    if (rhs && !isLoopInvariant(rhs)) {
        if (lhs.term && !isLoopInvariant(lhs.term))
            return;
        MDefinition *temp = lhs.term;
        lhs.term = rhs;
        rhs = temp;
        if (!SafeSub(0, lhs.constant, &lhs.constant))
            return;
        lessEqual = !lessEqual;
    }

    JS_ASSERT_IF(rhs, isLoopInvariant(rhs));

    // Ensure the lhs is a phi node from the start of the loop body.
    if (!lhs.term || !lhs.term->isPhi() || lhs.term->block() != header_)
        return;

    // Check if the lhs in the conditional matches the bounds check index.
    LinearSum index = ExtractLinearSum(ins->index());
    if (index.term != lhs.term)
        return;

    if (!lessEqual)
        return;

    // At the point of the access, it is known that lhs + lhsN <= rhs, and the
    // bounds check is that lhs + indexN + maximum < length. To ensure the
    // bounds check holds then, we need to ensure that:
    //
    // rhs - lhsN + indexN + maximum < length

    int32 adjustment;
    if (!SafeSub(index.constant, lhs.constant, &adjustment))
        return;
    if (!SafeAdd(adjustment, ins->maximum(), &adjustment))
        return;

    // For the lower bound, check that lhs + indexN + minimum >= 0, e.g.
    //
    // lhs >= -indexN - minimum
    //
    // lhs is not loop invariant, but if this condition holds of the backing
    // variable at loop entry and the variable's value never decreases in the
    // loop body, it will hold throughout the loop.

    uint32 position = preLoop_->positionInPhiSuccessor();
    MDefinition *initialIndex = lhs.term->toPhi()->getOperand(position);
    if (!nonDecreasing(initialIndex, lhs.term))
        return;

    int32 lowerBound;
    if (!SafeSub(0, index.constant, &lowerBound))
        return;
    if (!SafeSub(lowerBound, ins->minimum(), &lowerBound))
        return;

    // XXX limit on how much can be hoisted, to ensure ballast works?

    if (!rhs) {
        rhs = MConstant::New(Int32Value(adjustment));
        adjustment = 0;
        preLoop_->insertBefore(preLoop_->lastIns(), rhs->toInstruction());
    }

    MBoundsCheck *upper = MBoundsCheck::New(rhs, ins->length());
    upper->setMinimum(adjustment);
    upper->setMaximum(adjustment);

    MBoundsCheckLower *lower = MBoundsCheckLower::New(initialIndex);
    lower->setMinimum(lowerBound);

    *pupper = upper;
    *plower = lower;
}

// Determine whether the possible value of start (a phi node within the loop)
// can become smaller than an initial value at loop entry.
bool
Loop::nonDecreasing(MDefinition *initial, MDefinition *start)
{
    MDefinitionVector worklist;
    MDefinitionVector seen;

    if (!worklist.append(start))
        return false;

    while (!worklist.empty()) {
        MDefinition *def = worklist.popCopy();
        bool duplicate = false;
        for (size_t i = 0; i < seen.length() && !duplicate; i++) {
            if (seen[i] == def)
                duplicate = true;
        }
        if (duplicate)
            continue;
        if (!seen.append(def))
            return false;

        if (def->type() != MIRType_Int32)
            return false;

        if (!isInLoop(def)) {
            if (def != initial)
                return false;
            continue;
        }

        if (def->isPhi()) {
            MPhi *phi = def->toPhi();
            for (size_t i = 0; i < phi->numOperands(); i++) {
                if (!worklist.append(phi->getOperand(i)))
                    return false;
            }
            continue;
        }

        if (def->isAdd()) {
            if (def->toAdd()->specialization() != MIRType_Int32)
                return false;
            MDefinition *lhs = def->toAdd()->getOperand(0);
            MDefinition *rhs = def->toAdd()->getOperand(1);
            if (!rhs->isConstant())
                return false;
            Value v = rhs->toConstant()->value();
            if (!v.isInt32() || v.toInt32() < 0)
                return false;
            if (!worklist.append(lhs))
                return false;
            continue;
        }

        return false;
    }

    return true;
}
