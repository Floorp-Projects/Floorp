/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ValueNumbering.h"

#include "jit/AliasAnalysis.h"
#include "jit/IonAnalysis.h"
#include "jit/IonSpewer.h"
#include "jit/MIRGenerator.h"

using namespace js;
using namespace js::jit;

/**
 * Some notes on the main algorithm here:
 *  - The SSA identifier id() is the value number. We do replaceAllUsesWith as
 *    we go, so there's always at most one visible value with a given number.
 *
 *  - Consequently, the GVN algorithm is effectively pessimistic. This means it
 *    is not as powerful as an optimistic GVN would be, but it is simpler and
 *    faster.
 *
 *  - We iterate in RPO, so that when visiting a block, we've already optimized
 *    and hashed all values in dominating blocks. With occasional exceptions,
 *    this allows us to do everything in a single pass.
 *
 *  - When we do use multiple passes, we just re-run the algorithm on the whole
 *    graph instead of doing sparse propagation. This is a tradeoff to keep the
 *    algorithm simpler and lighter on inputs that don't have a lot of
 *    interesting unreachable blocks or degenerate loop induction variables, at
 *    the expense of being slower on inputs that do. The loop for this
 *    always terminates, because it only iterates when code is or will be
 *    removed, so eventually it must stop iterating.
 *
 *  - Values are not immediately removed from the hash set when they go out of
 *    scope. Instead, we check for dominance after a lookup. If the dominance
 *    check fails, the value is removed.
 */

HashNumber
ValueNumberer::VisibleValues::ValueHasher::hash(Lookup ins)
{
    return ins->valueHash();
}

// Test whether two MDefinitions are congruent.
bool
ValueNumberer::VisibleValues::ValueHasher::match(Key k, Lookup l)
{
    // If one of the instructions depends on a store, and the other instruction
    // does not depend on the same store, the instructions are not congruent.
    if (k->dependency() != l->dependency())
        return false;

    return k->congruentTo(l); // Ask the values themselves what they think.
}

void
ValueNumberer::VisibleValues::ValueHasher::rekey(Key &k, Key newKey)
{
    k = newKey;
}

ValueNumberer::VisibleValues::VisibleValues(TempAllocator &alloc)
  : set_(alloc)
{}

// Initialize the set in preparation for holding num elements.
bool
ValueNumberer::VisibleValues::init()
{
    return set_.init();
}

// Look up the first entry for the given def.
ValueNumberer::VisibleValues::Ptr
ValueNumberer::VisibleValues::findLeader(const MDefinition *def) const
{
    return set_.lookup(def);
}

// Look up the first entry for the given def.
ValueNumberer::VisibleValues::AddPtr
ValueNumberer::VisibleValues::findLeaderForAdd(MDefinition *def)
{
    return set_.lookupForAdd(def);
}

// Insert a value into the set.
bool
ValueNumberer::VisibleValues::insert(AddPtr p, MDefinition *def)
{
    return set_.add(p, def);
}

// Insert a value onto the set overwriting any existing entry.
void
ValueNumberer::VisibleValues::overwrite(AddPtr p, MDefinition *def)
{
    set_.rekeyInPlace(p, def);
}

// The given def will be deleted, so remove it from any sets.
void
ValueNumberer::VisibleValues::forget(const MDefinition *def)
{
    Ptr p = set_.lookup(def);
    if (p && *p == def)
        set_.remove(p);
}

// Clear all state.
void
ValueNumberer::VisibleValues::clear()
{
    set_.clear();
}

// Test whether the value would be needed if it had no uses.
static bool
DeadIfUnused(const MDefinition *def)
{
    return !def->isEffectful() && !def->isGuard() && !def->isControlInstruction() &&
           (!def->isInstruction() || !def->toInstruction()->resumePoint());
}

// Test whether the given definition is no longer needed.
static bool
IsDead(const MDefinition *def)
{
    return !def->hasUses() && DeadIfUnused(def);
}

// Test whether the given definition will no longer be needed after its user
// is deleted. TODO: This misses cases where the definition is used multiple
// times by the same user (bug 1031396).
static bool
WillBecomeDead(const MDefinition *def)
{
    return def->hasOneUse() && DeadIfUnused(def);
}

// Call MDefinition::replaceAllUsesWith, and add some GVN-specific asserts.
static void
ReplaceAllUsesWith(MDefinition *from, MDefinition *to)
{
    MOZ_ASSERT(from != to, "GVN shouldn't try to replace a value with itself");
    MOZ_ASSERT(from->type() == to->type(), "Def replacement has different type");

    from->replaceAllUsesWith(to);
}

// Test whether succ is a successor of newControl.
static bool
HasSuccessor(const MControlInstruction *newControl, const MBasicBlock *succ)
{
    for (size_t i = 0, e = newControl->numSuccessors(); i != e; ++i) {
        if (newControl->getSuccessor(i) == succ)
            return true;
    }
    return false;
}

// Given a block which has had predecessors removed but is still reachable,
// test whether the block's new dominator will be closer than its old one
// and whether it will expose potential optimization opportunities.
static MBasicBlock *
ComputeNewDominator(MBasicBlock *block, MBasicBlock *old)
{
    MBasicBlock *now = block->getPredecessor(0);
    for (size_t i = 1, e = block->numPredecessors(); i != e; ++i) {
        MBasicBlock *pred = block->getPredecessor(i);
        // Note that dominators haven't been recomputed yet, so we have to check
        // whether now dominates pred, not block.
        while (!now->dominates(pred)) {
            MBasicBlock *next = now->immediateDominator();
            if (next == old)
                return old;
            if (next == now) {
                MOZ_ASSERT(block == old, "Non-self-dominating block became self-dominating");
                return block;
            }
            now = next;
        }
    }
    MOZ_ASSERT(old != block || old != now, "Missed self-dominating block staying self-dominating");
    return now;
}

// Given a block which has had predecessors removed but is still reachable,
// test whether the block's new dominator will be closer than its old one
// and whether it will expose potential optimization opportunities.
static bool
IsDominatorRefined(MBasicBlock *block)
{
    MBasicBlock *old = block->immediateDominator();
    MBasicBlock *now = ComputeNewDominator(block, old);

    // We've computed block's new dominator. Test whether there are any
    // newly-dominating definitions which look interesting.
    MOZ_ASSERT(old->dominates(now), "Refined dominator not dominated by old dominator");
    for (MBasicBlock *i = now; i != old; i = i->immediateDominator()) {
        if (!i->phisEmpty() || *i->begin() != i->lastIns())
            return true;
    }

    return false;
}

// Delete the given instruction and anything in its use-def subtree which is no
// longer needed.
bool
ValueNumberer::deleteDefsRecursively(MDefinition *def)
{
    def->setInWorklist();
    return deadDefs_.append(def) && processDeadDefs();
}

// Assuming phi is dead, push each dead operand of phi not dominated by the phi
// to the delete worklist.
bool
ValueNumberer::pushDeadPhiOperands(MPhi *phi, const MBasicBlock *phiBlock)
{
    for (size_t o = 0, e = phi->numOperands(); o != e; ++o) {
        MDefinition *op = phi->getOperand(o);
        if (WillBecomeDead(op) && !op->isInWorklist() &&
            !phiBlock->dominates(phiBlock->getPredecessor(o)))
        {
            op->setInWorklist();
            if (!deadDefs_.append(op))
                return false;
        } else {
           op->setUseRemovedUnchecked();
        }
    }
    return true;
}

// Assuming ins is dead, push each dead operand of ins to the delete worklist.
bool
ValueNumberer::pushDeadInsOperands(MInstruction *ins)
{
    for (size_t o = 0, e = ins->numOperands(); o != e; ++o) {
        MDefinition *op = ins->getOperand(o);
        if (WillBecomeDead(op) && !op->isInWorklist()) {
            op->setInWorklist();
            if (!deadDefs_.append(op))
                return false;
        } else {
           op->setUseRemovedUnchecked();
        }
    }
    return true;
}

// Recursively delete all the defs on the deadDefs_ worklist.
bool
ValueNumberer::processDeadDefs()
{
    while (!deadDefs_.empty()) {
        MDefinition *def = deadDefs_.popCopy();
        IonSpew(IonSpew_GVN, "    Deleting %s%u", def->opName(), def->id());
        MOZ_ASSERT(def->isInWorklist(), "Deleting value not on the worklist");
        MOZ_ASSERT(IsDead(def), "Deleting non-dead definition");

        values_.forget(def);

        if (def->isPhi()) {
            MPhi *phi = def->toPhi();
            MBasicBlock *phiBlock = phi->block();
            if (!pushDeadPhiOperands(phi, phiBlock))
                 return false;
            MPhiIterator at(phiBlock->phisBegin(phi));
            phiBlock->discardPhiAt(at);
        } else {
            MInstruction *ins = def->toInstruction();
            if (!pushDeadInsOperands(ins))
                 return false;
            ins->block()->discard(ins);
        }
    }
    return true;
}

// Delete an edge from the CFG. Return true if the block becomes unreachable.
bool
ValueNumberer::removePredecessor(MBasicBlock *block, MBasicBlock *pred)
{
    bool isUnreachableLoop = false;
    if (block->isLoopHeader()) {
        if (block->loopPredecessor() == pred) {
            // Deleting the entry into the loop makes the loop unreachable.
            isUnreachableLoop = true;
            IonSpew(IonSpew_GVN, "    Loop with header block%u is no longer reachable", block->id());
#ifdef DEBUG
        } else if (block->hasUniqueBackedge() && block->backedge() == pred) {
            IonSpew(IonSpew_GVN, "    Loop with header block%u is no longer a loop", block->id());
#endif
        }
    }

    // TODO: Removing a predecessor removes operands from phis, and these
    // operands may become dead. We should detect this and delete them.
    // In practice though, when this happens, we often end up re-running GVN
    // for other reasons anyway.
    block->removePredecessor(pred);
    return block->numPredecessors() == 0 || isUnreachableLoop;
}

// Delete the given block and any block in its dominator subtree.
bool
ValueNumberer::removeBlocksRecursively(MBasicBlock *start, const MBasicBlock *dominatorRoot)
{
    MOZ_ASSERT(start != graph_.entryBlock(), "Removing normal entry block");
    MOZ_ASSERT(start != graph_.osrBlock(), "Removing OSR entry block");

    // Remove this block from its dominator parent's subtree. This is the only
    // immediately-dominated-block information we need to manually update,
    // because everything dominated by this block is about to be swept away.
    MBasicBlock *parent = start->immediateDominator();
    if (parent != start)
        parent->removeImmediatelyDominatedBlock(start);

    if (!unreachableBlocks_.append(start))
        return false;
    do {
        MBasicBlock *block = unreachableBlocks_.popCopy();
        if (block->isDead())
            continue;

        // If a block is removed while it is on the worklist, skip it.
        for (size_t i = 0, e = block->numSuccessors(); i != e; ++i) {
            MBasicBlock *succ = block->getSuccessor(i);
            if (!succ->isDead()) {
                if (removePredecessor(succ, block)) {
                    if (!unreachableBlocks_.append(succ))
                        return false;
                } else if (!rerun_) {
                    if (!remainingBlocks_.append(succ))
                        return false;
                }
            }
        }

#ifdef DEBUG
        IonSpew(IonSpew_GVN, "    Deleting block%u%s%s%s", block->id(),
                block->isLoopHeader() ? " (loop header)" : "",
                block->isSplitEdge() ? " (split edge)" : "",
                block->immediateDominator() == block ? " (dominator root)" : "");
        for (MDefinitionIterator iter(block); iter; iter++) {
            MDefinition *def = *iter;
            IonSpew(IonSpew_GVN, "      Deleting %s%u", def->opName(), def->id());
        }
        MControlInstruction *control = block->lastIns();
        IonSpew(IonSpew_GVN, "      Deleting %s%u", control->opName(), control->id());
#endif

        // Keep track of how many blocks within dominatorRoot's tree have been deleted.
        if (dominatorRoot->dominates(block))
            ++numBlocksDeleted_;

        // TODO: Removing a block deletes the phis, instructions, and resume
        // points in the block, and their operands may become dead. We should
        // detect this and delete them. In practice though, when this happens,
        // we often end up re-running GVN for other reasons anyway (bug 1031412).
        graph_.removeBlockIncludingPhis(block);
        blocksRemoved_ = true;
    } while (!unreachableBlocks_.empty());

    return true;
}

// Return a simplified form of the given definition, if we can.
MDefinition *
ValueNumberer::simplified(MDefinition *def) const
{
    return def->foldsTo(graph_.alloc());
}

// If an equivalent and dominating value already exists in the set, return it.
// Otherwise insert the given definition into the set and return it.
MDefinition *
ValueNumberer::leader(MDefinition *def)
{
    // If the value isn't suitable for eliminating, don't bother hashing it. The
    // convention is that congruentTo returns false for node kinds that wish to
    // opt out of redundance elimination.
    // TODO: It'd be nice to clean up that convention (bug 1031406).
    if (!def->isEffectful() && def->congruentTo(def)) {
        // Look for a match.
        VisibleValues::AddPtr p = values_.findLeaderForAdd(def);
        if (p) {
            MDefinition *rep = *p;
            if (rep->block()->dominates(def->block())) {
                // We found a dominating congruent value.
                MOZ_ASSERT(!rep->isInWorklist(), "Dead value in set");
                return rep;
            }

            // The congruent value doesn't dominate. It never will again in this
            // dominator tree, so overwrite it.
            values_.overwrite(p, def);
        } else {
            // No match. Add a new entry.
            if (!values_.insert(p, def))
                return nullptr;
        }
    }

    return def;
}

// Test whether the given phi is dominated by a congruent phi.
bool
ValueNumberer::hasLeader(const MPhi *phi, const MBasicBlock *phiBlock) const
{
    if (VisibleValues::Ptr p = values_.findLeader(phi)) {
        const MDefinition *rep = *p;
        return rep != phi && rep->block()->dominates(phiBlock);
    }
    return false;
}

// Test whether there are any phis in the backedge's loop header which are
// newly optimizable, as a result of optimizations done inside the loop. This
// is not a sparse approach, but restarting is rare enough in practice.
// Termination is ensured by deleting the phi triggering the iteration.
bool
ValueNumberer::loopHasOptimizablePhi(MBasicBlock *backedge) const
{
    // Rescan the phis for any that can be simplified, since they may be reading
    // values from backedges.
    MBasicBlock *header = backedge->loopHeaderOfBackedge();
    for (MPhiIterator iter(header->phisBegin()), end(header->phisEnd()); iter != end; ++iter) {
        MPhi *phi = *iter;
        MOZ_ASSERT(phi->hasUses(), "Missed an unused phi");

        if (phi->operandIfRedundant() || hasLeader(phi, header))
            return true; // Phi can be simplified.
    }
    return false;
}

// Visit the given definition.
bool
ValueNumberer::visitDefinition(MDefinition *def)
{
    // Look for a simplified form of this def.
    MDefinition *sim = simplified(def);
    if (sim != def) {
        if (sim == nullptr)
            return false;

        // If sim doesn't belong to a block, insert it next to def.
        if (sim->block() == nullptr)
            def->block()->insertAfter(def->toInstruction(), sim->toInstruction());

        IonSpew(IonSpew_GVN, "    Folded %s%u to %s%u",
                def->opName(), def->id(), sim->opName(), sim->id());
        ReplaceAllUsesWith(def, sim);
        if (IsDead(def) && !deleteDefsRecursively(def))
            return false;
        def = sim;
    }

    // Look for a dominating def which makes this def redundant.
    MDefinition *rep = leader(def);
    if (rep != def) {
        if (rep == nullptr)
            return false;
        if (rep->updateForReplacement(def)) {
            IonSpew(IonSpew_GVN,
                    "    Replacing %s%u with %s%u",
                    def->opName(), def->id(), rep->opName(), rep->id());
            ReplaceAllUsesWith(def, rep);

            // This is effectively what the old GVN did. It allows isGuard()
            // instructions to be deleted if they are redundant, and the
            // replacement is not even guaranteed to have isGuard() set.
            // TODO: Clean this up (bug 1031410).
            def->setNotGuardUnchecked();

            if (IsDead(def) && !deleteDefsRecursively(def))
                return false;
            def = rep;
        }
    }

    // If this instruction has a dependency() into an unreachable block, we'll
    // need to update AliasAnalysis.
    if (updateAliasAnalysis_ && !dependenciesBroken_) {
        const MDefinition *dep = def->dependency();
        if (dep != nullptr && dep->block()->isDead()) {
            IonSpew(IonSpew_GVN, "    AliasAnalysis invalidated; will recompute!");
            dependenciesBroken_ = true;
        }
    }

    return true;
}

// Visit the control instruction at the end of the given block.
bool
ValueNumberer::visitControlInstruction(MBasicBlock *block, const MBasicBlock *dominatorRoot)
{
    // Look for a simplified form of the control instruction.
    MControlInstruction *control = block->lastIns();
    MDefinition *rep = simplified(control);
    if (rep == control)
        return true;

    if (rep == nullptr)
        return false;

    MControlInstruction *newControl = rep->toControlInstruction();
    MOZ_ASSERT(!newControl->block(),
               "Control instruction replacement shouldn't already be in a block");
    IonSpew(IonSpew_GVN, "    Folded control instruction %s%u to %s%u",
            control->opName(), control->id(), newControl->opName(), graph_.getNumInstructionIds());

    // If the simplification removes any CFG edges, update the CFG and remove
    // any blocks that become dead.
    size_t oldNumSuccs = control->numSuccessors();
    size_t newNumSuccs = newControl->numSuccessors();
    if (newNumSuccs != oldNumSuccs) {
        MOZ_ASSERT(newNumSuccs < oldNumSuccs, "New control instruction has too many successors");
        for (size_t i = 0; i != oldNumSuccs; ++i) {
            MBasicBlock *succ = control->getSuccessor(i);
            if (!HasSuccessor(newControl, succ)) {
                if (removePredecessor(succ, block)) {
                    if (!removeBlocksRecursively(succ, dominatorRoot))
                        return false;
                } else if (!rerun_) {
                    if (!remainingBlocks_.append(succ))
                        return false;
                }
            }
        }
    }

    if (!pushDeadInsOperands(control))
        return false;
    block->discardLastIns();
    block->end(newControl);
    return processDeadDefs();
}

// Visit all the phis and instructions in the given block.
bool
ValueNumberer::visitBlock(MBasicBlock *block, const MBasicBlock *dominatorRoot)
{
    MOZ_ASSERT(!block->unreachable(), "Blocks marked unreachable during GVN");
    MOZ_ASSERT(!block->isDead(), "Block to visit is already dead");

    // Visit the definitions in the block top-down.
    for (MDefinitionIterator iter(block); iter; ) {
        MDefinition *def = *iter++;

        // If the definition is dead, delete it.
        if (IsDead(def)) {
            if (!deleteDefsRecursively(def))
                return false;
            continue;
        }

        if (!visitDefinition(def))
            return false;
    }

    return visitControlInstruction(block, dominatorRoot);
}

// Visit all the blocks dominated by dominatorRoot.
bool
ValueNumberer::visitDominatorTree(MBasicBlock *dominatorRoot, size_t *totalNumVisited)
{
    IonSpew(IonSpew_GVN, "  Visiting dominator tree (with %llu blocks) rooted at block%u%s",
            uint64_t(dominatorRoot->numDominated()), dominatorRoot->id(),
            dominatorRoot == graph_.entryBlock() ? " (normal entry block)" :
            dominatorRoot == graph_.osrBlock() ? " (OSR entry block)" :
            " (normal entry and OSR entry merge point)");
    MOZ_ASSERT(numBlocksDeleted_ == 0, "numBlocksDeleted_ wasn't reset");
    MOZ_ASSERT(dominatorRoot->immediateDominator() == dominatorRoot,
            "root is not a dominator tree root");

    // Visit all blocks dominated by dominatorRoot, in RPO. This has the nice property
    // that we'll always visit a block before any block it dominates, so we can
    // make a single pass through the list and see every full redundance.
    size_t numVisited = 0;
    for (ReversePostorderIterator iter(graph_.rpoBegin(dominatorRoot)); ; ++iter) {
        MOZ_ASSERT(iter != graph_.rpoEnd(), "Inconsistent dominator information");
        MBasicBlock *block = *iter;
        // We're only visiting blocks in dominatorRoot's tree right now.
        if (!dominatorRoot->dominates(block))
            continue;
        // Visit the block!
        if (!visitBlock(block, dominatorRoot))
            return false;
        // If this was the end of a loop, check for optimization in the header.
        if (!rerun_ && block->isLoopBackedge() && loopHasOptimizablePhi(block)) {
            IonSpew(IonSpew_GVN, "    Loop phi in block%u can now be optimized; will re-run GVN!",
                    block->id());
            rerun_ = true;
            remainingBlocks_.clear();
        }
        ++numVisited;
        MOZ_ASSERT(numVisited <= dominatorRoot->numDominated() - numBlocksDeleted_,
                   "Visited blocks too many times");
        if (numVisited >= dominatorRoot->numDominated() - numBlocksDeleted_)
            break;
    }

    *totalNumVisited += numVisited;
    values_.clear();
    numBlocksDeleted_ = 0;
    return true;
}

// Visit all the blocks in the graph.
bool
ValueNumberer::visitGraph()
{
    // Due to OSR blocks, the set of blocks dominated by a blocks may not be
    // contiguous in the RPO. Do a separate traversal for each dominator tree
    // root. There's always the main entry, and sometimes there's an OSR entry,
    // and then there are the roots formed where the OSR paths merge with the
    // main entry paths.
    size_t totalNumVisited = 0;
    for (ReversePostorderIterator iter(graph_.rpoBegin()); ; ++iter) {
         MBasicBlock *block = *iter;
         if (block->immediateDominator() == block) {
             if (!visitDominatorTree(block, &totalNumVisited))
                 return false;
             MOZ_ASSERT(totalNumVisited <= graph_.numBlocks(), "Visited blocks too many times");
             if (totalNumVisited >= graph_.numBlocks())
                 break;
         }
         MOZ_ASSERT(iter != graph_.rpoEnd(), "Inconsistent dominator information");
    }
    return true;
}

ValueNumberer::ValueNumberer(MIRGenerator *mir, MIRGraph &graph)
  : mir_(mir), graph_(graph),
    values_(graph.alloc()),
    deadDefs_(graph.alloc()),
    unreachableBlocks_(graph.alloc()),
    remainingBlocks_(graph.alloc()),
    numBlocksDeleted_(0),
    rerun_(false),
    blocksRemoved_(false),
    updateAliasAnalysis_(false),
    dependenciesBroken_(false)
{}

bool
ValueNumberer::run(UpdateAliasAnalysisFlag updateAliasAnalysis)
{
    updateAliasAnalysis_ = updateAliasAnalysis == UpdateAliasAnalysis;

    // Initialize the value set. It's tempting to pass in a size here of some
    // function of graph_.getNumInstructionIds(), however if we start out with
    // a large capacity, it will be far larger than the actual element count
    // for most of the pass, so when we remove elements, it would often think
    // it needs to compact itself. Empirically, just letting the HashTable grow
    // as needed on its own seems to work pretty well.
    if (!values_.init())
        return false;

    IonSpew(IonSpew_GVN, "Running GVN on graph (with %llu blocks)",
            uint64_t(graph_.numBlocks()));

    // Top level non-sparse iteration loop. If an iteration performs a
    // significant change, such as deleting a block which changes the dominator
    // tree and may enable more optimization, this loop takes another iteration.
    int runs = 0;
    for (;;) {
        if (!visitGraph())
            return false;

        // Test whether any block which was not removed but which had at least
        // one predecessor removed will have a new dominator parent.
        while (!remainingBlocks_.empty()) {
            MBasicBlock *block = remainingBlocks_.popCopy();
            if (!block->isDead() && IsDominatorRefined(block)) {
                IonSpew(IonSpew_GVN, "  Dominator for block%u can now be refined; will re-run GVN!",
                        block->id());
                rerun_ = true;
                remainingBlocks_.clear();
                break;
            }
        }

        if (blocksRemoved_) {
            if (!AccountForCFGChanges(mir_, graph_, dependenciesBroken_))
                return false;

            blocksRemoved_ = false;
            dependenciesBroken_ = false;
        }

        if (mir_->shouldCancel("GVN (outer loop)"))
            return false;

        // If no further opportunities have been discovered, we're done.
        if (!rerun_)
            break;

        IonSpew(IonSpew_GVN, "Re-running GVN on graph (run %d, now with %llu blocks)",
                runs, uint64_t(graph_.numBlocks()));
        rerun_ = false;

        // Enforce an arbitrary iteration limit. This is rarely reached, and
        // isn't even strictly necessary, as the algorithm is guaranteed to
        // terminate on its own in a finite amount of time (since every time we
        // re-run we delete the construct which triggered the re-run), but it
        // does help avoid slow compile times on pathlogical code.
        ++runs;
        if (runs == 6) {
            IonSpew(IonSpew_GVN, "Re-run cutoff reached. Terminating GVN!");
            break;
        }
    }

    return true;
}
