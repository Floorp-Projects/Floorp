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
    size_t preSplitEdges = graph.numBlocks();
    for (size_t i = 0; i < preSplitEdges; i++) {
        MBasicBlock *block = graph.getBlock(i);
        if (block->numSuccessors() < 2)
            continue;
        for (size_t i = 0; i < block->numSuccessors(); i++) {
            MBasicBlock *target = block->getSuccessor(i);
            if (target->numPredecessors() < 2)
                continue;

            // Create a new block inheriting from the predecessor.
            MBasicBlock *split = MBasicBlock::NewSplitEdge(gen, block);
            if (!graph.addBlock(split))
                return false;
            split->end(MGoto::New(target));

            block->replaceSuccessor(i, split);
            target->replacePredecessor(block, split);
        }
    }

    return true;
}

class TypeAnalyzer
{
    MIRGraph &graph;
    js::Vector<MInstruction *, 0, SystemAllocPolicy> worklist_;

    bool empty() const {
        return worklist_.empty();
    }
    MInstruction *pop() {
        MInstruction *ins = worklist_.popCopy();
        ins->setNotInWorklist();
        return ins;
    }
    bool push(MDefinition *def) {
        if (def->isInWorklist() || !def->typePolicy())
            return true;
        JS_ASSERT(!def->isPhi());
        def->setInWorklist();
        return worklist_.append(def->toInstruction());
    }

    bool buildWorklist();
    bool reflow(MDefinition *def);
    bool despecializePhi(MPhi *phi);
    bool specializePhi(MPhi *phi);
    bool specializePhis();
    bool specializeInstructions();
    bool determineSpecializations();
    bool insertConversions();
    bool adjustPhiInputs(MPhi *phi);
    bool adjustInputs(MDefinition *def);
    bool adjustOutput(MDefinition *def);

  public:
    TypeAnalyzer(MIRGraph &graph)
      : graph(graph)
    { }

    bool analyze();
};

bool
TypeAnalyzer::buildWorklist()
{
    // The worklist is LIFO. We add items in postorder to get reverse-postorder
    // removal.
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
        MDefinitionIterator iter(block);
        while (iter) {
            if (iter->isCopy()) {
                // Remove copies here.
                MCopy *copy = iter->toCopy();
                copy->replaceAllUsesWith(copy->getOperand(0));
                iter = block->removeDefAt(iter);
                continue;
            }
            if (!push(*iter))
                return false;
            iter++;
        }
    }
    return true;
}

bool
TypeAnalyzer::reflow(MDefinition *def)
{
    // Reflow this definition's uses, since its output type changed.
    // Policies must guarantee this terminates by never narrowing
    // during a respecialization.
    for (MUseDefIterator uses(def); uses; uses++) {
        if (!push(uses.def()))
            return false;
    }
    return true;
}

bool
TypeAnalyzer::specializeInstructions()
{
    // For each instruction with a type policy, analyze its inputs to see if a
    // respecialization is needed, which may change its output type. If such a
    // change occurs, re-add each use of the instruction back to the worklist.
    while (!empty()) {
        MInstruction *ins = pop();

        TypePolicy *policy = ins->typePolicy();
        if (policy->respecialize(ins)) {
            if (!reflow(ins))
                return false;
        }
    }
    return true;
}

static inline MIRType
GetEffectiveType(MDefinition *def)
{
    return def->type() != MIRType_Value
           ? def->type()
           : def->usedAsType();
}

bool
TypeAnalyzer::despecializePhi(MPhi *phi)
{
    // If the phi is already despecialized, we're done.
    if (phi->type() == MIRType_Value)
        return true;

    phi->specialize(MIRType_Value);
    if (!reflow(phi))
        return false;
    return true;
}

bool
TypeAnalyzer::specializePhi(MPhi *phi)
{
    // If this phi was despecialized, but we have already tried to specialize
    // it, just give up.
    if (phi->triedToSpecialize() && phi->type() == MIRType_Value)
        return true;

    // Find the type of the first phi input.
    MDefinition *in = phi->getOperand(0);
    MIRType first = GetEffectiveType(in);

    // If it's a value, just give up and leave the phi unspecialized.
    if (first == MIRType_Value)
        return despecializePhi(phi);

    for (size_t i = 1; i < phi->numOperands(); i++) {
        MDefinition *other = phi->getOperand(i);
        if (GetEffectiveType(other) != first)
            return despecializePhi(phi);
    }

    if (phi->type() == first)
        return true;

    // All inputs have the same type - specialize this phi!
    phi->specialize(first);
    if (!reflow(phi))
        return false;

    return true;
}

bool
TypeAnalyzer::specializePhis()
{
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
            if (!specializePhi(*phi))
                return false;
        }
    }
    return true;
}

bool
TypeAnalyzer::determineSpecializations()
{
    do {
        // First, specialize all non-phi instructions.
        if (!specializeInstructions())
            return false;

        // Now, go through phis, and try to specialize those. If any phis
        // become specialized, their uses are re-added to the worklist. Phis
        // can "toggle" between specializations, so to ensure a fixpoint, we
        // forcibly prevent a phi from specializing more than twice.
        if (!specializePhis())
            return false;
    } while (!empty());
    return true;
}

bool
TypeAnalyzer::adjustOutput(MDefinition *def)
{
    JS_ASSERT(def->type() == MIRType_Value);

    MIRType usedAs = def->usedAsType();
    if (usedAs == MIRType_Value) {
        // This definition is used as more than one type, so give up on
        // specializing its definition. Its uses instead will insert
        // appropriate conversion operations.
        return true;
    }

    MBasicBlock *block = def->block();
    MUnbox *unbox = MUnbox::New(def, usedAs);
    if (def->isPhi()) {
        // Insert at the beginning of the block.
        block->insertBefore(*block->begin(), unbox);
    } else if (block->start() && def->id() < block->start()->id()) {
        // This definition comes before the start of the program, so insert
        // the unbox after the start instruction.
        block->insertAfter(block->start(), unbox);
    } else {
        // Insert directly after the instruction.
        block->insertAfter(def->toInstruction(), unbox);
    }

    JS_ASSERT(def->usesBegin()->node() == unbox);

    for (MUseIterator use(def->usesBegin()); use != def->usesEnd(); ) {
        bool replace = true;

        if (use->node()->isSnapshot()) {
            MSnapshot *snapshot = use->node()->toSnapshot();
            
            // If this snapshot is the definition's snapshot (in case it is
            // effectful), we *cannot* replace its use! The snapshot comes in
            // between the definition and the unbox.
            if (def->isInstruction() && def->toInstruction()->snapshot() == snapshot)
                replace = false;
        } else {
            MDefinition *other = use->node()->toDefinition();
            if (TypePolicy *policy = other->typePolicy())
                replace = policy->useSpecializedInput(def->toInstruction(), use->index(), unbox);
        }

        if (replace)
            use = use->node()->replaceOperand(use, unbox);
        else
            use++;
    }

    return true;
}

bool
TypeAnalyzer::adjustPhiInputs(MPhi *phi)
{
    // If the phi returns a specific type, assert that its inputs are correct.
    if (phi->type() != MIRType_Value) {
#ifdef DEBUG
        for (size_t i = 0; i < phi->numOperands(); i++) {
            MDefinition *in = phi->getOperand(i);
            JS_ASSERT(GetEffectiveType(in) == phi->type());
        }
#endif
        return true;
    }

    // Box every typed input.
    for (size_t i = 0; i < phi->numOperands(); i++) {
        MDefinition *in = phi->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;

        MBox *box = MBox::New(in);
        in->block()->insertBefore(in->block()->lastIns(), box);
        phi->replaceOperand(i, box);
    }

    return true;
}

bool
TypeAnalyzer::adjustInputs(MDefinition *def)
{
    // The adjustOutput pass of our inputs' defs may not have have been
    // satisfactory, so double check now, inserting conversions as necessary.
    TypePolicy *policy = def->typePolicy();
    if (policy && !policy->adjustInputs(def->toInstruction()))
        return false;
    return true;
}

bool
TypeAnalyzer::insertConversions()
{
    // Instructions are processed in postorder: all uses are defs are seen
    // before uses. This ensures that output adjustment (which may rewrite
    // inputs of uses) does not conflict with input adjustment.
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
        for (MPhiIterator phi(block->phisBegin()); phi != block->phisEnd(); phi++) {
            if (!adjustPhiInputs(*phi))
                return false;
            if (phi->type() == MIRType_Value && !adjustOutput(*phi))
                return false;
        }
        for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++) {
            if (!adjustInputs(*iter))
                return false;
            if (iter->type() == MIRType_Value && !adjustOutput(*iter))
                return false;
        }
    }
    return true;
}

bool
TypeAnalyzer::analyze()
{
    if (!buildWorklist())
        return false;
    if (!determineSpecializations())
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
    Vector<MBasicBlock *, 0, IonAllocPolicy> pending;
    Vector<unsigned int, 0, IonAllocPolicy> successors;
    Vector<MBasicBlock *, 0, IonAllocPolicy> done;

    MBasicBlock *current = graph.getBlock(0);
    unsigned int nextSuccessor = 0;

    graph.clearBlockList();

    // Build up a postorder traversal non-recursively.
    while (true) {
        if (!current->isMarked()) {
            current->mark();

            if (nextSuccessor < current->lastIns()->numSuccessors()) {
                if (!pending.append(current))
                    return false;
                if (!successors.append(nextSuccessor))
                    return false;

                current = current->lastIns()->getSuccessor(nextSuccessor);
                nextSuccessor = 0;
                continue;
            }

            if (!done.append(current))
                return false;
        }

        if (pending.empty())
            break;

        current = pending.popCopy();
        current->unmark();
        nextSuccessor = successors.popCopy() + 1;
    }

    JS_ASSERT(pending.empty());
    JS_ASSERT(successors.empty());

    while (!done.empty()) {
        current = done.popCopy();
        current->unmark();
        if (!graph.addBlock(current))
            return false;
    }

    return true;
}

// A Simple, Fast Dominance Algorithm by Cooper et al.
static MBasicBlock *
IntersectDominators(MBasicBlock *block1, MBasicBlock *block2)
{
    MBasicBlock *finger1 = block1;
    MBasicBlock *finger2 = block2;

    while (finger1->id() != finger2->id()) {
        // In the original paper, the comparisons are on the postorder index.
        // In this implementation, the id of the block is in reverse postorder,
        // so we reverse the comparison.
        while (finger1->id() > finger2->id())
            finger1 = finger1->immediateDominator();

        while (finger2->id() > finger1->id())
            finger2 = finger2->immediateDominator();
    }
    return finger1;
}

static void
ComputeImmediateDominators(MIRGraph &graph)
{

    if (graph.numBlocks() == 0)
        return;

    MBasicBlock *startBlock = graph.getBlock(0);
    startBlock->setImmediateDominator(startBlock);

    bool changed = true;

    while (changed) {
        changed = false;
        // We start at 1, not 0, intentionally excluding the start node.
        for (size_t i = 1; i < graph.numBlocks(); i++) {
            MBasicBlock *block = graph.getBlock(i);

            if (block->numPredecessors() == 0)
                continue;

            MBasicBlock *newIdom = block->getPredecessor(0);

            for (size_t i = 1; i < block->numPredecessors(); i++) {
                MBasicBlock *pred = block->getPredecessor(i);
                if (pred->immediateDominator() != NULL)
                    newIdom = IntersectDominators(pred, newIdom);
            }

            if (block->immediateDominator() != newIdom) {
                block->setImmediateDominator(newIdom);
                changed = true;
            }
        }
    }
}

bool
ion::BuildDominatorTree(MIRGraph &graph)
{
    if (graph.numBlocks() == 0)
        return true;

    ComputeImmediateDominators(graph);

    // Since traversing through the graph in post-order means that every use
    // of a definition is visited before the def itself. Since a def must
    // dominate all its uses, this means that by the time we reach a particular
    // block, we have processed all of its dominated children, so
    // block->numDominated() is accurate.
    for (size_t i = graph.numBlocks() - 1; i > 0; i--) { //Exclude start block.
        MBasicBlock *child = graph.getBlock(i);
        MBasicBlock *parent = child->immediateDominator();

        if (!parent->addImmediatelyDominatedBlock(child))
            return false;

        // an additional +1 because of this child block.
        parent->addNumDominated(child->numDominated() + 1);
    }
    JS_ASSERT(graph.getBlock(0)->numDominated() == graph.numBlocks() - 1);
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
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
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

            pred->setSuccessorWithPhis(block, j);
        }
    }

    return true;
}

