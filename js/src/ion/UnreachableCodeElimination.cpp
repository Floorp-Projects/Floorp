/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UnreachableCodeElimination.h"
#include "IonAnalysis.h"

using namespace js;
using namespace ion;

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
    if (marked_ == graph_.numBlocks()) {
        // Everything is reachable.
        graph_.unmarkBlocks();
        return true;
    }

    // Pass 2: Remove unmarked blocks.
    if (!removeUnmarkedBlocksAndClearDominators())
        return false;
    graph_.unmarkBlocks();

    // Pass 3: Recompute dominators and tweak phis.
    BuildDominatorTree(graph_);
    if (redundantPhis_ && !EliminatePhis(mir_, graph_, ConservativeObservability))
        return false;

    return true;
}

bool
UnreachableCodeElimination::prunePointlessBranchesAndMarkReachableBlocks()
{
    Vector<MBasicBlock *, 16, SystemAllocPolicy> worklist;

    // Seed with the two entry points.
    MBasicBlock *entries[] = { graph_.entryBlock(), graph_.osrBlock() };
    for (size_t i = 0; i < sizeof(entries) / sizeof(entries[0]); i++) {
        if (entries[i]) {
            entries[i]->mark();
            marked_++;
            if (!worklist.append(entries[i]))
                return false;
        }
    }

    // Process everything reachable from there.
    while (!worklist.empty()) {
        if (mir_->shouldCancel("Eliminate Unreachable Code"))
            return false;

        MBasicBlock *block = worklist.popCopy();
        MControlInstruction *ins = block->lastIns();

        // Rewrite test false or test true to goto.
        if (ins->isTest()) {
            MTest *testIns = ins->toTest();
            MDefinition *v = testIns->getOperand(0);
            if (v->isConstant()) {
                const Value &val = v->toConstant()->value();
                BranchDirection bdir = ToBoolean(val) ? TRUE_BRANCH : FALSE_BRANCH;
                MBasicBlock *succ = testIns->branchSuccessor(bdir);
                MGoto *gotoIns = MGoto::New(succ);
                block->discardLastIns();
                block->end(gotoIns);
                MBasicBlock *successorWithPhis = block->successorWithPhis();
                if (successorWithPhis && successorWithPhis != succ)
                    block->setSuccessorWithPhis(NULL, 0);
            }
        }

        for (size_t i = 0; i < block->numSuccessors(); i++) {
            MBasicBlock *succ = block->getSuccessor(i);
            if (!succ->isMarked()) {
                succ->mark();
                marked_++;
                if (!worklist.append(succ))
                    return false;
            }
        }
    }
    return true;
}

void
UnreachableCodeElimination::removeUsesFromUnmarkedBlocks(MDefinition *instr)
{
    for (MUseIterator iter(instr->usesBegin()); iter != instr->usesEnd(); ) {
        if (!iter->node()->block()->isMarked())
            iter = instr->removeUse(iter);
        else
            iter++;
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
                removeUsesFromUnmarkedBlocks(*iter);
            for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++)
                removeUsesFromUnmarkedBlocks(*iter);
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

            graph_.removeBlock(block);
        }
    }

    JS_ASSERT(id == 0);

    return true;
}
