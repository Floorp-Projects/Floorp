/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/UnreachableCodeElimination.h"

#include "jit/AliasAnalysis.h"
#include "jit/IonAnalysis.h"
#include "jit/MIRGenerator.h"
#include "jit/ValueNumbering.h"

using namespace js;
using namespace jit;

bool
UnreachableCodeElimination::analyze()
{
    // The goal of this routine is to eliminate code that is
    // unreachable, either because there is no path from the entry
    // block to the code, or because the path traverses a conditional
    // branch where the condition is a constant (e.g., "if (false) {
    // ... }").  The latter can either appear in the source form or
    // arise due to optimizations.
    //
    // The stategy is straightforward.  The pass begins with a
    // depth-first search.  We set a bit on each basic block that
    // is visited.  If a block terminates in a conditional branch
    // predicated on a constant, we rewrite the block to an unconditional
    // jump and do not visit the now irrelevant basic block.
    //
    // Once the initial DFS is complete, we do a second pass over the
    // blocks to find those that were not reached.  Those blocks are
    // simply removed wholesale.  We must also correct any phis that
    // may be affected..

    // Pass 1: Identify unreachable blocks (if any).
    if (!prunePointlessBranchesAndMarkReachableBlocks())
        return false;

    return removeUnmarkedBlocksAndCleanup();
}

bool
UnreachableCodeElimination::removeUnmarkedBlocks(size_t marked)
{
    marked_ = marked;
    return removeUnmarkedBlocksAndCleanup();
}

bool
UnreachableCodeElimination::removeUnmarkedBlocksAndCleanup()
{
    // Everything is reachable, no work required.
    JS_ASSERT(marked_ <= graph_.numBlocks());
    if (marked_ == graph_.numBlocks()) {
        graph_.unmarkBlocks();
        return true;
    }

    // Pass 2: Remove unmarked blocks (see analyze() above).
    if (!removeUnmarkedBlocksAndClearDominators())
        return false;
    graph_.unmarkBlocks();

    AssertGraphCoherency(graph_);

    IonSpewPass("UCE-mid-point");

    // Pass 3: Recompute dominators and tweak phis.
    BuildDominatorTree(graph_);
    if (redundantPhis_ && !EliminatePhis(mir_, graph_, ConservativeObservability))
        return false;

    // Pass 4: Rerun alias analysis
    if (rerunAliasAnalysis_) {
        AliasAnalysis analysis(mir_, graph_);
        if (!analysis.analyze())
            return false;
    }

    // Pass 5: It's important for optimizations to re-run GVN (and in
    // turn alias analysis) after UCE if we eliminated branches.
    if (rerunAliasAnalysis_ && js_IonOptions.gvn) {
        ValueNumberer gvn(mir_, graph_, js_IonOptions.gvnIsOptimistic);
        if (!gvn.clear() || !gvn.analyze())
            return false;
        IonSpewPass("GVN-after-UCE");
        AssertExtendedGraphCoherency(graph_);

        if (mir_->shouldCancel("GVN-after-UCE"))
            return false;
    }

    return true;
}

bool
UnreachableCodeElimination::enqueue(MBasicBlock *block, BlockList &list)
{
    if (block->isMarked())
        return true;

    block->mark();
    marked_++;
    return list.append(block);
}

MBasicBlock *
UnreachableCodeElimination::optimizableSuccessor(MBasicBlock *block)
{
    // If the last instruction in `block` is a test instruction of a
    // constant value, returns the successor that the branch will
    // always branch to at runtime. Otherwise, returns NULL.

    MControlInstruction *ins = block->lastIns();
    if (!ins->isTest())
        return NULL;

    MTest *testIns = ins->toTest();
    MDefinition *v = testIns->getOperand(0);
    if (!v->isConstant())
        return NULL;

    const Value &val = v->toConstant()->value();
    BranchDirection bdir = ToBoolean(val) ? TRUE_BRANCH : FALSE_BRANCH;
    return testIns->branchSuccessor(bdir);
}

bool
UnreachableCodeElimination::prunePointlessBranchesAndMarkReachableBlocks()
{
    BlockList worklist, optimizableBlocks;

    // Process everything reachable from the start block, ignoring any
    // OSR block.
    if (!enqueue(graph_.entryBlock(), worklist))
        return false;
    while (!worklist.empty()) {
        if (mir_->shouldCancel("Eliminate Unreachable Code"))
            return false;

        MBasicBlock *block = worklist.popCopy();

        // If this block is a test on a constant operand, only enqueue
        // the relevant successor. Also, remember the block for later.
        if (MBasicBlock *succ = optimizableSuccessor(block)) {
            if (!optimizableBlocks.append(block))
                return false;
            if (!enqueue(succ, worklist))
                return false;
        } else {
            // Otherwise just visit all successors.
            for (size_t i = 0; i < block->numSuccessors(); i++) {
                MBasicBlock *succ = block->getSuccessor(i);
                if (!enqueue(succ, worklist))
                    return false;
            }
        }
    }

    // Now, if there is an OSR block, check that all of its successors
    // were reachable (bug 880377). If not, we are in danger of
    // creating a CFG with two disjoint parts, so simply mark all
    // blocks as reachable. This generally occurs when the TI info for
    // stack types is incorrect or incomplete, due to operations that
    // have not yet executed in baseline.
    if (graph_.osrBlock()) {
        MBasicBlock *osrBlock = graph_.osrBlock();
        JS_ASSERT(!osrBlock->isMarked());
        if (!enqueue(osrBlock, worklist))
            return false;
        for (size_t i = 0; i < osrBlock->numSuccessors(); i++) {
            if (!osrBlock->getSuccessor(i)->isMarked()) {
                // OSR block has an otherwise unreachable successor, abort.
                for (MBasicBlockIterator iter(graph_.begin()); iter != graph_.end(); iter++)
                    iter->mark();
                marked_ = graph_.numBlocks();
                return true;
            }
        }
    }

    // Now that we know we will not abort due to OSR, go back and
    // transform any tests on constant operands into gotos.
    for (uint32_t i = 0; i < optimizableBlocks.length(); i++) {
        MBasicBlock *block = optimizableBlocks[i];
        MBasicBlock *succ = optimizableSuccessor(block);
        JS_ASSERT(succ);

        MGoto *gotoIns = MGoto::New(succ);
        block->discardLastIns();
        block->end(gotoIns);
        MBasicBlock *successorWithPhis = block->successorWithPhis();
        if (successorWithPhis && successorWithPhis != succ)
            block->setSuccessorWithPhis(NULL, 0);
    }

    return true;
}

void
UnreachableCodeElimination::checkDependencyAndRemoveUsesFromUnmarkedBlocks(MDefinition *instr)
{
    // When the instruction depends on removed block,
    // alias analysis needs to get rerun to have the right dependency.
    if (instr->dependency() && !instr->dependency()->block()->isMarked())
        rerunAliasAnalysis_ = true;

    for (MUseIterator iter(instr->usesBegin()); iter != instr->usesEnd(); ) {
        if (!iter->consumer()->block()->isMarked()) {
            instr->setUseRemovedUnchecked();
            iter = instr->removeUse(iter);
        } else {
            iter++;
        }
    }
}

bool
UnreachableCodeElimination::removeUnmarkedBlocksAndClearDominators()
{
    // Removes blocks that are not marked from the graph.  For blocks
    // that *are* marked, clears the mark and adjusts the id to its
    // new value.  Also adds blocks that are immediately reachable
    // from an unmarked block to the frontier.

    size_t id = marked_;
    for (PostorderIterator iter(graph_.poBegin()); iter != graph_.poEnd();) {
        if (mir_->shouldCancel("Eliminate Unreachable Code"))
            return false;

        MBasicBlock *block = *iter;
        iter++;

        // Unconditionally clear the dominators.  It's somewhat complex to
        // adjust the values and relatively fast to just recompute.
        block->clearDominatorInfo();

        if (block->isMarked()) {
            block->setId(--id);
            for (MPhiIterator iter(block->phisBegin()); iter != block->phisEnd(); iter++)
                checkDependencyAndRemoveUsesFromUnmarkedBlocks(*iter);
            for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++)
                checkDependencyAndRemoveUsesFromUnmarkedBlocks(*iter);
        } else {
            if (block->numPredecessors() > 1) {
                // If this block had phis, then any reachable
                // predecessors need to have the successorWithPhis
                // flag cleared.
                for (size_t i = 0; i < block->numPredecessors(); i++)
                    block->getPredecessor(i)->setSuccessorWithPhis(NULL, 0);
            }

            if (block->isLoopBackedge()) {
                // NB. We have to update the loop header if we
                // eliminate the backedge. At first I thought this
                // check would be insufficient, because it would be
                // possible to have code like this:
                //
                //    while (true) {
                //       ...;
                //       if (1 == 1) break;
                //    }
                //
                // in which the backedge is removed as part of
                // rewriting the condition, but no actual blocks are
                // removed.  However, in all such cases, the backedge
                // would be a critical edge and hence the critical
                // edge block is being removed.
                block->loopHeaderOfBackedge()->clearLoopHeader();
            }

            for (size_t i = 0, c = block->numSuccessors(); i < c; i++) {
                MBasicBlock *succ = block->getSuccessor(i);
                if (succ->isMarked()) {
                    // succ is on the frontier of blocks to be removed:
                    succ->removePredecessor(block);

                    if (!redundantPhis_) {
                        for (MPhiIterator iter(succ->phisBegin()); iter != succ->phisEnd(); iter++) {
                            if (iter->operandIfRedundant()) {
                                redundantPhis_ = true;
                                break;
                            }
                        }
                    }
                }
            }

            // When we remove a call, we can't leave the corresponding MPassArg
            // in the graph. Since lowering will fail. Replace it with the
            // argument for the exceptional case when it is kept alive in a
            // ResumePoint. DCE will remove the unused MPassArg instruction.
            for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++) {
                if (iter->isCall()) {
                    MCall *call = iter->toCall();
                    for (size_t i = 0; i < call->numStackArgs(); i++) {
                        JS_ASSERT(call->getArg(i)->isPassArg());
                        JS_ASSERT(call->getArg(i)->hasOneDefUse());
                        MPassArg *arg = call->getArg(i)->toPassArg();
                        arg->replaceAllUsesWith(arg->getArgument());
                    }
                }
            }

            graph_.removeBlock(block);
        }
    }

    JS_ASSERT(id == 0);

    return true;
}
