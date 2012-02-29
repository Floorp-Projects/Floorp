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

#include "IonBuilder.h"
#include "MIRGraph.h"
#include "Ion.h"
#include "IonAnalysis.h"

using namespace js;
using namespace js::ion;

// A critical edge is an edge which is neither its successor's only predecessor
// nor its predecessor's only successor. Critical edges must be split to
// prevent copy-insertion and code motion from affecting other edges.
bool
ion::SplitCriticalEdges(MIRGenerator *gen, MIRGraph &graph)
{
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        if (block->numSuccessors() < 2)
            continue;
        for (size_t i = 0; i < block->numSuccessors(); i++) {
            MBasicBlock *target = block->getSuccessor(i);
            if (target->numPredecessors() < 2)
                continue;

            // Create a new block inheriting from the predecessor.
            MBasicBlock *split = MBasicBlock::NewSplitEdge(graph, block->info(), *block);
            split->setLoopDepth(block->loopDepth());
            graph.addBlock(split);
            split->end(MGoto::New(target));

            block->replaceSuccessor(i, split);
            target->replacePredecessor(*block, split);
        }
    }
    return true;
}

// Instructions are useless if they are unused and have no side effects.
// This pass eliminates useless instructions.
// The graph itself is unchanged.
bool
ion::EliminateDeadCode(MIRGraph &graph)
{
    // Traverse in postorder so that we hit uses before definitions.
    // Traverse instruction list backwards for the same reason.
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        // Remove unused instructions.
        for (MInstructionReverseIterator inst = block->rbegin(); inst != block->rend(); ) {
            if (!inst->isEffectful() && !inst->hasUses() && !inst->isGuard() &&
                !inst->isControlInstruction()) {
                inst = block->discardAt(inst);
            } else {
                inst++;
            }
        }
    }

    return true;
}

static inline bool
IsPhiObservable(MPhi *phi)
{
    // Note that this skips reading resume points, which we don't count as
    // actual uses. This is safe as long as the SSA still mimics the actual
    // bytecode, i.e. no elimination has occurred. If the only uses are resume
    // points, then the SSA name is never consumed by the program.
    for (MUseDefIterator iter(phi); iter; iter++) {
        if (!iter.def()->isPhi())
            return true;
    }
    return false;
}

bool
ion::EliminateDeadPhis(MIRGraph &graph)
{
    Vector<MPhi *, 16, SystemAllocPolicy> worklist;

    // Add all observable phis to a worklist. We use the "in worklist" bit to
    // mean "this phi is live".
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        for (MPhiIterator iter = block->phisBegin(); iter != block->phisEnd(); iter++) {
            if (IsPhiObservable(*iter)) {
                iter->setInWorklist();
                if (!worklist.append(*iter))
                    return false;
            }
        }
    }

    // Iteratively mark all phis reacahble from live phis.
    while (!worklist.empty()) {
        MPhi *phi = worklist.popCopy();

        for (size_t i = 0; i < phi->numOperands(); i++) {
            MDefinition *in = phi->getOperand(i);
            if (!in->isPhi() || in->isInWorklist())
                continue;
            in->setInWorklist();
            if (!worklist.append(in->toPhi()))
                return false;
        }
    }

    // Sweep dead phis.
    for (PostorderIterator block = graph.poBegin(); block != graph.poEnd(); block++) {
        MPhiIterator iter = block->phisBegin();
        while (iter != block->phisEnd()) {
            if (iter->isInWorklist()) {
                iter->setNotInWorklist();
                iter++;
            } else {
                iter->setUnused();
                iter = block->discardPhiAt(iter);
            }
        }
    }

    return true;
}

void
ion::EliminateCopies(MIRGraph &graph)
{
    // The worklist is LIFO. We add items in postorder to get reverse-postorder
    // removal.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        MInstructionIterator iter = block->begin();
        while (iter != block->end()) {
            if (iter->isCopy()) {
                // Remove copies here.
                MCopy *copy = iter->toCopy();
                copy->replaceAllUsesWith(copy->getOperand(0));
                iter = block->discardAt(iter);
            } else {
                iter++;
            }
        }
    }
}

// The type analysis algorithm inserts conversions and box/unbox instructions
// to make the IR graph well-typed for future passes.
//
// Phi adjustment: If a phi's inputs are all the same type, the phi is
// specialized to return that type.
//
// Input adjustment: Each input is asked to apply conversion operations to its
// inputs. This may include Box, Unbox, or other instruction-specific type
// conversion operations.
//
class TypeAnalyzer
{
    MIRGraph &graph;
    Vector<MPhi *, 0, SystemAllocPolicy> phiWorklist_;

    bool addPhiToWorklist(MPhi *phi) {
        if (phi->isInWorklist())
            return true;
        if (!phiWorklist_.append(phi))
            return false;
        phi->setInWorklist();
        return true;
    }
    MPhi *popPhi() {
        MPhi *phi = phiWorklist_.popCopy();
        phi->setNotInWorklist();
        return phi;
    }

    bool propagateSpecialization(MPhi *phi);
    bool specializePhis();
    void replaceRedundantPhi(MPhi *phi);
    void adjustPhiInputs(MPhi *phi);
    bool adjustInputs(MDefinition *def);
    bool insertConversions();

  public:
    TypeAnalyzer(MIRGraph &graph)
      : graph(graph)
    { }

    bool analyze();
};

// Try to specialize this phi based on its non-cyclic inputs.
static MIRType
GuessPhiType(MPhi *phi)
{
    MIRType type = MIRType_None;
    for (size_t i = 0; i < phi->numOperands(); i++) {
        MDefinition *in = phi->getOperand(i);
        if (in->isPhi() && !in->toPhi()->triedToSpecialize())
            continue;
        if (type == MIRType_None)
            type = in->type();
        else if (type != in->type())
            return MIRType_Value;
    }
    return type;
}

bool
TypeAnalyzer::propagateSpecialization(MPhi *phi)
{
    // Verify that this specialization matches any phis depending on it.
    for (MUseDefIterator iter(phi); iter; iter++) {
        if (!iter.def()->isPhi())
            continue;
        MPhi *use = iter.def()->toPhi();
        if (!use->triedToSpecialize())
            continue;
        if (use->type() == MIRType_None) {
            // We tried to specialize this phi, but were unable to guess its
            // type. Now that we know the type of one of its operands, we can
            // specialize it.
            use->specialize(phi->type());
            if (!addPhiToWorklist(use))
                return false;
            continue;
        }
        if (use->type() != phi->type()) {
            // This phi in our use chain can now no longer be specialized.
            use->specialize(MIRType_Value);
            if (!addPhiToWorklist(use))
                return false;
        }
    }

    return true;
}

bool
TypeAnalyzer::specializePhis()
{
    for (PostorderIterator block(graph.poBegin()); block != graph.poEnd(); block++) {
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
            phi->specialize(GuessPhiType(*phi));
            if (!propagateSpecialization(*phi))
                return false;
        }
    }

    while (!phiWorklist_.empty()) {
        MPhi *phi = popPhi();
        if (!propagateSpecialization(phi))
            return false;
    }

    return true;
}

void
TypeAnalyzer::adjustPhiInputs(MPhi *phi)
{
    MIRType phiType = phi->type();
    if (phiType != MIRType_Value)
        return;

    // Box every typed input.
    for (size_t i = 0; i < phi->numOperands(); i++) {
        MDefinition *in = phi->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;

        if (in->isUnbox()) {
            // The input is being explicitly unboxed, so sneak past and grab
            // the original box.
            phi->replaceOperand(i, in->toUnbox()->input());
        } else {
            MBox *box = MBox::New(in);
            in->block()->insertBefore(in->block()->lastIns(), box);
            phi->replaceOperand(i, box);
        }
    }
}

bool
TypeAnalyzer::adjustInputs(MDefinition *def)
{
    TypePolicy *policy = def->typePolicy();
    if (policy && !policy->adjustInputs(def->toInstruction()))
        return false;
    return true;
}

void
TypeAnalyzer::replaceRedundantPhi(MPhi *phi)
{
    MBasicBlock *block = phi->block();
    js::Value v = (phi->type() == MIRType_Undefined) ? UndefinedValue() : NullValue();
    MConstant *c = MConstant::New(v);
    // The instruction pass will insert the box
    block->insertBefore(*(block->begin()), c);
    phi->replaceAllUsesWith(c);
}

bool
TypeAnalyzer::insertConversions()
{
    // Instructions are processed in reverse postorder: all uses are defs are
    // seen before uses. This ensures that output adjustment (which may rewrite
    // inputs of uses) does not conflict with input adjustment.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd();) {
            if (phi->type() <= MIRType_Null) {
                replaceRedundantPhi(*phi);
                phi = block->discardPhiAt(phi);
            } else {
                adjustPhiInputs(*phi);
                phi++;
            }
        }
        for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++) {
            if (!adjustInputs(*iter))
                return false;
        }
    }
    return true;
}

bool
TypeAnalyzer::analyze()
{
    if (!specializePhis())
        return false;
    if (!insertConversions())
        return false;
    return true;
}

bool
ion::ApplyTypeInformation(MIRGraph &graph)
{
    TypeAnalyzer analyzer(graph);

    if (!analyzer.analyze())
        return false;

    return true;
}

bool
ion::ReorderBlocks(MIRGraph &graph)
{
    InlineList<MBasicBlock> pending;
    Vector<unsigned int, 0, IonAllocPolicy> successors;
    InlineList<MBasicBlock> done;

    MBasicBlock *current = *graph.begin();

    // Since the block list will be reversed later, we visit successors
    // in reverse order. This way, the resulting block list more closely
    // resembles the order in which the IonBuilder adds the blocks.
    unsigned int nextSuccessor = current->numSuccessors() - 1;

#ifdef DEBUG
    size_t numBlocks = graph.numBlocks();
#endif

    graph.clearBlockList();

    // Build up a postorder traversal non-recursively.
    while (true) {
        if (!current->isMarked()) {
            current->mark();

            // Note: when we have visited all successors, nextSuccessor is
            // MAX_UINT. This case is handled correctly since the following
            // comparison is unsigned.
            if (nextSuccessor < current->numSuccessors()) {
                pending.pushFront(current);
                if (!successors.append(nextSuccessor))
                    return false;

                current = current->getSuccessor(nextSuccessor);
                nextSuccessor = current->numSuccessors() - 1;
                continue;
            }

            done.pushFront(current);
        }

        if (pending.empty())
            break;

        current = pending.popFront();
        current->unmark();
        nextSuccessor = successors.popCopy() - 1;
    }

    JS_ASSERT(pending.empty());
    JS_ASSERT(successors.empty());

    // The start block must have ID 0.
    current = done.popFront();
    current->unmark();
    graph.addBlock(current);

    // If an OSR block exists, it is a root, and therefore not included in the
    // above traversal. Since it is a root, it must have an ID below that of
    // its successor. Therefore we assign it an ID of 1.
    if (graph.osrBlock())
        graph.addBlock(graph.osrBlock());

    // Insert the remaining blocks in RPO. Loop blocks are treated specially,
    // to make sure no loop successor blocks are inserted before the loop's
    // backedge.
    uint32 loopDepth = 0;

    // List of loop successor blocks we need to insert after the backedge.
    Vector<MBasicBlock *, 8, IonAllocPolicy> pendingNonLoopBlocks;

    // For every active loop, this list contains the index of the first
    // block in pendingNonLoopBlocks.
    Vector<size_t, 4, IonAllocPolicy> loops;

    while (!done.empty()) {
        current = done.popFront();
        current->unmark();

        if (current->isLoopHeader()) {
            loopDepth = current->loopDepth();
            if (!loops.append(pendingNonLoopBlocks.length()))
                return false;
        }

        if (current->isLoopBackedge() && current->loopDepth() == loopDepth) {
            loopDepth--;
            graph.addBlock(current);

            // Re-visit all blocks we were not allowed to insert before the
            // current backedge.
            size_t nblocks = pendingNonLoopBlocks.length() - loops.popCopy();
            for (size_t i = 0; i < nblocks; i++)
                done.pushFront(pendingNonLoopBlocks.popCopy());
            continue;
        } else if (current->loopDepth() < loopDepth) {
            // We are not allowed to insert this loop successor block before the
            // backedge. Add it to the pending list so that we can insert it
            // after the backedge.
            if (!pendingNonLoopBlocks.append(current))
                return false;
            continue;
        }

        graph.addBlock(current);
    }

    JS_ASSERT(loopDepth == 0);
    JS_ASSERT(pendingNonLoopBlocks.empty());
    JS_ASSERT(graph.numBlocks() == numBlocks);

    return true;
}

// A Simple, Fast Dominance Algorithm by Cooper et al.
// Modified to support empty intersections for OSR, and in RPO.
static MBasicBlock *
IntersectDominators(MBasicBlock *block1, MBasicBlock *block2)
{
    MBasicBlock *finger1 = block1;
    MBasicBlock *finger2 = block2;

    JS_ASSERT(finger1);
    JS_ASSERT(finger2);

    // In the original paper, the block ID comparisons are on the postorder index.
    // This implementation iterates in RPO, so the comparisons are reversed.

    // For this function to be called, the block must have multiple predecessors.
    // If a finger is then found to be self-dominating, it must therefore be
    // reachable from multiple roots through non-intersecting control flow.
    // NULL is returned in this case, to denote an empty intersection.

    while (finger1->id() != finger2->id()) {
        while (finger1->id() > finger2->id()) {
            MBasicBlock *idom = finger1->immediateDominator();
            if (idom == finger1)
                return NULL; // Empty intersection.
            finger1 = idom;
        }

        while (finger2->id() > finger1->id()) {
            MBasicBlock *idom = finger2->immediateDominator();
            if (idom == finger2)
                return NULL; // Empty intersection.
            finger2 = finger2->immediateDominator();
        }
    }
    return finger1;
}

static void
ComputeImmediateDominators(MIRGraph &graph)
{
    // The default start block is a root and therefore only self-dominates.
    MBasicBlock *startBlock = *graph.begin();
    startBlock->setImmediateDominator(startBlock);

    // Any OSR block is a root and therefore only self-dominates.
    MBasicBlock *osrBlock = graph.osrBlock();
    if (osrBlock)
        osrBlock->setImmediateDominator(osrBlock);

    bool changed = true;

    while (changed) {
        changed = false;

        ReversePostorderIterator block = graph.rpoBegin();

        // For each block in RPO, intersect all dominators.
        for (; block != graph.rpoEnd(); block++) {
            // If a node has once been found to have no exclusive dominator,
            // it will never have an exclusive dominator, so it may be skipped.
            if (block->immediateDominator() == *block)
                continue;

            MBasicBlock *newIdom = block->getPredecessor(0);

            // Find the first common dominator.
            for (size_t i = 1; i < block->numPredecessors(); i++) {
                MBasicBlock *pred = block->getPredecessor(i);
                if (pred->immediateDominator() != NULL)
                    newIdom = IntersectDominators(pred, newIdom);

                // If there is no common dominator, the block self-dominates.
                if (newIdom == NULL) {
                    block->setImmediateDominator(*block);
                    changed = true;
                    break;
                }
            }

            if (newIdom && block->immediateDominator() != newIdom) {
                block->setImmediateDominator(newIdom);
                changed = true;
            }
        }
    }

#ifdef DEBUG
    // Assert that all blocks have dominator information.
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        JS_ASSERT(block->immediateDominator() != NULL);
    }
#endif
}

bool
ion::BuildDominatorTree(MIRGraph &graph)
{
    ComputeImmediateDominators(graph);

    // Traversing through the graph in post-order means that every use
    // of a definition is visited before the def itself. Since a def
    // dominates its uses, by the time we reach a particular
    // block, we have processed all of its dominated children, so
    // block->numDominated() is accurate.
    for (PostorderIterator i(graph.poBegin()); i != graph.poEnd(); i++) {
        MBasicBlock *child = *i;
        MBasicBlock *parent = child->immediateDominator();

        // If the block only self-dominates, it has no definite parent.
        if (child == parent)
            continue;

        if (!parent->addImmediatelyDominatedBlock(child))
            return false;

        // An additional +1 for the child block.
        parent->addNumDominated(child->numDominated() + 1);
    }

#ifdef DEBUG
    // If compiling with OSR, many blocks will self-dominate.
    // Without OSR, there is only one root block which dominates all.
    if (!graph.osrBlock())
        JS_ASSERT(graph.begin()->numDominated() == graph.numBlocks() - 1);
#endif

    return true;
}

bool
ion::BuildPhiReverseMapping(MIRGraph &graph)
{
    // Build a mapping such that given a basic block, whose successor has one or
    // more phis, we can find our specific input to that phi. To make this fast
    // mapping work we rely on a specific property of our structured control
    // flow graph: For a block with phis, its predecessors each have only one
    // successor with phis. Consider each case:
    //   * Blocks with less than two predecessors cannot have phis.
    //   * Breaks. A break always has exactly one successor, and the break
    //             catch block has exactly one predecessor for each break, as
    //             well as a final predecessor for the actual loop exit.
    //   * Continues. A continue always has exactly one successor, and the
    //             continue catch block has exactly one predecessor for each
    //             continue, as well as a final predecessor for the actual
    //             loop continuation. The continue itself has exactly one
    //             successor.
    //   * An if. Each branch as exactly one predecessor.
    //   * A switch. Each branch has exactly one predecessor.
    //   * Loop tail. A new block is always created for the exit, and if a
    //             break statement is present, the exit block will forward
    //             directly to the break block.
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        if (block->numPredecessors() < 2) {
            JS_ASSERT(block->phisEmpty());
            continue;
        }

        // Assert on the above.
        for (size_t j = 0; j < block->numPredecessors(); j++) {
            MBasicBlock *pred = block->getPredecessor(j);

#ifdef DEBUG
            size_t numSuccessorsWithPhis = 0;
            for (size_t k = 0; k < pred->numSuccessors(); k++) {
                MBasicBlock *successor = pred->getSuccessor(k);
                if (!successor->phisEmpty())
                    numSuccessorsWithPhis++;
            }
            JS_ASSERT(numSuccessorsWithPhis <= 1);
#endif

            pred->setSuccessorWithPhis(*block, j);
        }
    }

    return true;
}

static inline MBasicBlock *
SkipContainedLoop(MBasicBlock *block, MBasicBlock *header)
{
    while (block->loopHeader() || block->isLoopHeader()) {
        if (block->loopHeader())
            block = block->loopHeader();
        if (block == header)
            break;
        block = block->loopPredecessor();
    }
    return block;
}

// Mark every block in a loop body with the closest containing loop header.
bool
ion::FindNaturalLoops(MIRGraph &graph)
{
    Vector<MBasicBlock *, 8, SystemAllocPolicy> worklist;

    // Our RPO block ordering guarantees we'll see the loop body (and therefore inner
    // backedges) before outer backedges.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        if (!block->isLoopBackedge())
            continue;

        MBasicBlock *header = block->loopHeaderOfBackedge();
        JS_ASSERT(!block->loopHeader());
        JS_ASSERT(!header->loopHeader());

        // The header contains itself.
        header->setLoopHeader(header);
        if (!header->addContainedInLoop(header))
            return false;

        MBasicBlock *current = *block;
        do {
            // Find blocks belonging to the loop body by scanning predecessors.
            for (size_t i = 0; i < current->numPredecessors(); i++) {
                MBasicBlock *pred = current->getPredecessor(i);

                // If this block was already scanned (diamond in graph), just
                // ignore it.
                if (pred->loopHeader() == header)
                    continue;

                // Assert that all blocks are contained between the loop
                // header and the backedge.
                JS_ASSERT_IF(pred != graph.osrBlock(),
                             header->id() < pred->id() && pred->id() < block->id());

                // If this block belongs to another loop body, skip past that
                // entire loop (which is contained within this one).
                pred = SkipContainedLoop(pred, header);
                if (pred == header)
                    continue;

                JS_ASSERT(!pred->isLoopBackedge());

                if (!worklist.append(pred))
                    return false;
            }

            current->setLoopHeader(header);
            if (!header->addContainedInLoop(current))
                return false;
            if (worklist.empty())
                break;
            current = worklist.popCopy();
        } while (true);
    }

    return true;
}

#ifdef DEBUG
static bool
CheckSuccessorImpliesPredecessor(MBasicBlock *A, MBasicBlock *B)
{
    // Assuming B = succ(A), verify A = pred(B).
    for (size_t i = 0; i < B->numPredecessors(); i++) {
        if (A == B->getPredecessor(i))
            return true;
    }
    return false;
}

static bool
CheckPredecessorImpliesSuccessor(MBasicBlock *A, MBasicBlock *B)
{
    // Assuming B = pred(A), verify A = succ(B).
    for (size_t i = 0; i < B->numSuccessors(); i++) {
        if (A == B->getSuccessor(i))
            return true;
    }
    return false;
}

static bool
CheckMarkedAsUse(MInstruction *ins, MDefinition *operand)
{
    for (MUseIterator i = operand->usesBegin(); i != operand->usesEnd(); i++) {
        if (i->node()->isDefinition()) {
            if (ins == i->node()->toDefinition());
                return true;
        }
    }
    return false;
}
#endif // DEBUG

void
ion::AssertGraphCoherency(MIRGraph &graph)
{
#ifdef DEBUG
    // Assert successor and predecessor list coherency.
    for (MBasicBlockIterator block(graph.begin()); block != graph.end(); block++) {
        for (size_t i = 0; i < block->numSuccessors(); i++)
            JS_ASSERT(CheckSuccessorImpliesPredecessor(*block, block->getSuccessor(i)));

        for (size_t i = 0; i < block->numPredecessors(); i++)
            JS_ASSERT(CheckPredecessorImpliesSuccessor(*block, block->getPredecessor(i)));

        for (MInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
            for (uint32 i = 0; i < ins->numOperands(); i++)
                JS_ASSERT(CheckMarkedAsUse(*ins, ins->getOperand(i)));
        }
    }
#endif
}

