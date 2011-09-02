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
            MBasicBlock *split = MBasicBlock::NewSplitEdge(gen, *block);
            graph.addBlock(split);
            split->end(MGoto::New(target));

            block->replaceSuccessor(i, split);
            target->replacePredecessor(*block, split);
        }
    }
    return true;
}

// Instructions are useless if they are idempotent and unused.
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
            if (inst->isIdempotent() && !inst->hasUses())
                inst = block->removeAt(inst);
            else
                inst++;
        }

        // FIXME: Bug 678273.
        // All phi nodes currently have non-zero uses as determined
        // by hasUses(). They are kept alive by resume points, and therefore
        // cannot currently be eliminated.
    }

    return true;
}

// The type analysis algorithm inserts conversions and box/unbox instructions
// to make the IR graph well-typed for future passes. Each definition has the
// following type information:
//
//     * Actual type. This is the type the instruction will definitely return.
//     * Specialization. Some instructions, like MAdd, may be specialized to a 
//       particular type. This specialization directs the actual type.
//     * Observed type. If the actual type of a node is not known (Value), then
//       it may be annotated with the set of types it would be unboxed as
//       (determined by specialization). This directs whether unbox operations
//       can be hoisted to the definition versus placed near its uses.
//
// (1) Specialization.
//     ------------------------
//     All instructions and phis are added to a worklist, such that they are
//     initially observed in postorder.
//
//     Each instruction looks at the types of its inputs and decides whether to
//     respecialize (for example, prefer double inputs to int32). Instructions
//     may also annotate untyped values with a preferred type.
//
//     Each phi looks at the effective types of its inputs. If all inputs have
//     the same effective type, the phi specializes to that type.
//
//     If any definition's specialization changes, its uses are re-analyzed.
//     If any definition's effective type changes, its phi uses are
//     re-analyzed.
//
// (2) Conversions.
//     ------------------------
//     All instructions and phis are visited in reverse postorder.
//
//     (A) Output adjustment. If the definition's output is a Value, and has
//         exactly one observed type, then an Unbox instruction is placed right
//         after the definition. Each use is modified to take the narrowed
//         type.
//
//     (B) Input adjustment. Each input is asked to apply conversion operations
//         to its inputs. This may include Box, Unbox, or other
//         instruction-specific type conversion operations.
//
class TypeAnalyzer : public TypeAnalysis
{
    MIRGraph &graph;
    Vector<MInstruction *, 0, SystemAllocPolicy> worklist_;
    Vector<MPhi *, 0, SystemAllocPolicy> phiWorklist_;
    bool phisHaveBeenAnalyzed_;

    MInstruction *popInstruction() {
        MInstruction *ins = worklist_.popCopy();
        ins->setNotInWorklist();
        return ins;
    }
    MPhi *popPhi() {
        MPhi *phi = phiWorklist_.popCopy();
        phi->setNotInWorklist();
        return phi;
    }
    void repush(MDefinition *def) {
#ifdef DEBUG
        bool ok =
#endif
            push(def);
        JS_ASSERT(ok);
    }
    bool push(MDefinition *def) {
        if (def->isInWorklist())
            return true;
        if (!def->isPhi() && !def->typePolicy())
            return true;
        def->setInWorklist();
        if (def->isPhi())
            return phiWorklist_.append(def->toPhi());
        return worklist_.append(def->toInstruction());
    }

    // After building the worklist, insertion is infallible because memory for
    // all instructions has been reserved.
    bool buildWorklist();

    void addPreferredType(MDefinition *def, MIRType type);
    void reanalyzePhiUses(MDefinition *def);
    void reanalyzeUses(MDefinition *def);
    void despecializePhi(MPhi *phi);
    void specializePhi(MPhi *phi);
    void specializePhis();
    void specializeInstructions();
    void determineSpecializations();
    void replaceRedundantPhi(MPhi *phi);
    void adjustPhiInputs(MPhi *phi);
    bool adjustInputs(MDefinition *def);
    void adjustOutput(MDefinition *def);
    bool insertConversions();

  public:
    TypeAnalyzer(MIRGraph &graph)
      : graph(graph),
        phisHaveBeenAnalyzed_(false)
    { }

    bool analyze();
};

bool
TypeAnalyzer::buildWorklist()
{
    // The worklist is LIFO. We add items in postorder to get reverse-postorder
    // removal.
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        for (MPhiIterator iter = block->phisBegin(); iter != block->phisEnd(); iter++) {
            if (!push(*iter))
                return false;
        }
        MInstructionIterator iter = block->begin();
        while (iter != block->end()) {
            if (iter->isCopy()) {
                // Remove copies here.
                MCopy *copy = iter->toCopy();
                copy->replaceAllUsesWith(copy->getOperand(0));
                iter = block->removeAt(iter);
                continue;
            }
            if (!push(*iter))
                return false;
            iter++;
        }
    }
    return true;
}

void
TypeAnalyzer::reanalyzePhiUses(MDefinition *def)
{
    // Only bother analyzing effective type changes if the phi queue has not
    // yet been analyzed.
    if (!phisHaveBeenAnalyzed_)
        return;

    for (MUseDefIterator uses(def); uses; uses++) {
        if (uses.def()->isPhi())
            repush(uses.def());
    }
}

void
TypeAnalyzer::reanalyzeUses(MDefinition *def)
{
    // Reflow this definition's uses, since its output type changed.
    // Policies must guarantee this terminates by never narrowing
    // during a respecialization.
    for (MUseDefIterator uses(def); uses; uses++)
        repush(uses.def());
}

void
TypeAnalyzer::addPreferredType(MDefinition *def, MIRType type)
{
    MIRType usedAsType = def->usedAsType();
    def->useAsType(type);
    if (usedAsType != def->usedAsType())
        reanalyzePhiUses(def);
}

void
TypeAnalyzer::specializeInstructions()
{
    // For each instruction with a type policy, analyze its inputs to see if a
    // respecialization is needed, which may change its output type. If such a
    // change occurs, re-add each use of the instruction back to the worklist.
    while (!worklist_.empty()) {
        MInstruction *ins = popInstruction();

        TypePolicy *policy = ins->typePolicy();
        if (policy->respecialize(ins))
            reanalyzeUses(ins);
        policy->specializeInputs(ins, this);
    }
}

static inline MIRType
GetObservedType(MDefinition *def)
{
    return def->type() != MIRType_Value
           ? def->type()
           : def->usedAsType();
}

void
TypeAnalyzer::despecializePhi(MPhi *phi)
{
    // If the phi is already despecialized, we're done.
    if (phi->type() == MIRType_Value)
        return;

    phi->specialize(MIRType_Value);
    reanalyzeUses(phi);
}

void
TypeAnalyzer::specializePhi(MPhi *phi)
{
    // If this phi was despecialized, but we have already tried to specialize
    // it, just give up.
    if (phi->triedToSpecialize() && phi->type() == MIRType_Value)
        return;

    MIRType phiType = GetObservedType(phi);
    if (phiType != MIRType_Value) {
        // This phi is expected to be a certain type, so propagate this up to
        // its uses. While doing so, prevent re-adding this phi to the phi
        // worklist.
        phi->setInWorklist();
        for (size_t i = 0; i < phi->numOperands(); i++)
            addPreferredType(phi->getOperand(i), phiType);
        phi->setNotInWorklist();
    }

    // Find the type of the first phi input.
    MDefinition *in = phi->getOperand(0);
    MIRType first = GetObservedType(in);

    // If it's a value, just give up and leave the phi unspecialized.
    if (first == MIRType_Value) {
        despecializePhi(phi);
        return;
    }

    for (size_t i = 1; i < phi->numOperands(); i++) {
        MDefinition *other = phi->getOperand(i);
        if (GetObservedType(other) != first) {
            despecializePhi(phi);
            return;
        }
    }

    if (phi->type() == first)
        return;

    // All inputs have the same type - specialize this phi!
    phi->specialize(first);
    reanalyzeUses(phi);
}

void
TypeAnalyzer::specializePhis()
{
    phisHaveBeenAnalyzed_ = true;

    while (!phiWorklist_.empty()) {
        MPhi *phi = popPhi();
        specializePhi(phi);
    }
}
 
// Part 1: Determine specializations.
void
TypeAnalyzer::determineSpecializations()
{
    do {
        // First, specialize all non-phi instructions.
        specializeInstructions();

        // Now, go through phis, and try to specialize those. If any phis
        // become specialized, their uses are re-added to the worklist.
        specializePhis();
    } while (!worklist_.empty());
}

static inline bool
ShouldSpecializeInput(MDefinition *box, MNode *use, MUnbox *unbox)
{
    // If the node is a resume point, always replace the input to avoid
    // carrying around a wider type.
    if (use->isResumePoint()) {
        MResumePoint *resumePoint = use->toResumePoint();
            
        // If this resume point is attached to the definition, being effectful,
        // we *cannot* replace its use! The resume point comes in between the
        // definition and the unbox.
        MResumePoint *defResumePoint;
        if (box->isInstruction())
            defResumePoint = box->toInstruction()->resumePoint();
        else if (box->isPhi())
            defResumePoint = box->block()->entryResumePoint();
        return (defResumePoint != resumePoint);
    }

    MDefinition *def = use->toDefinition();

    // Phis do not have type policies, but if they are specialized need
    // specialized inputs.
    if (def->isPhi())
        return def->type() != MIRType_Value;

    // Otherwise, only replace nodes that have a type policy. Otherwise, we
    // would replace an unbox into its own input.
    if (def->typePolicy())
        return true;

    return false;
}

void
TypeAnalyzer::adjustOutput(MDefinition *def)
{
    JS_ASSERT(def->type() == MIRType_Value);

    MIRType usedAs = def->usedAsType();
    if (usedAs == MIRType_Value) {
        // This definition is used as more than one type, so give up on
        // specializing its definition. Its uses instead will insert
        // appropriate conversion operations.
        return;
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
        if (ShouldSpecializeInput(def, use->node(), unbox))
            use = use->node()->replaceOperand(use, unbox);
        else
            use++;
    }
}

void
TypeAnalyzer::adjustPhiInputs(MPhi *phi)
{
    // If the phi returns a specific type, assert that its inputs are correct.
    if (phi->type() != MIRType_Value) {
#ifdef DEBUG
        for (size_t i = 0; i < phi->numOperands(); i++) {
            MDefinition *in = phi->getOperand(i);
            JS_ASSERT(GetObservedType(in) == phi->type());
        }
#endif
        return;
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
                phi = block->removePhiAt(phi);
            } else {
                adjustPhiInputs(*phi);
                if (phi->type() == MIRType_Value)
                    adjustOutput(*phi);
                phi++;
            }
        }
        for (MInstructionIterator iter(block->begin()); iter != block->end(); iter++) {
            if (!adjustInputs(*iter))
                return false;
            if (iter->type() == MIRType_Value)
                adjustOutput(*iter);
        }
    }
    return true;
}

bool
TypeAnalyzer::analyze()
{
    if (!buildWorklist())
        return false;
    determineSpecializations();
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
    unsigned int nextSuccessor = 0;

    graph.clearBlockList();

    // Build up a postorder traversal non-recursively.
    while (true) {
        if (!current->isMarked()) {
            current->mark();

            if (nextSuccessor < current->lastIns()->numSuccessors()) {
                pending.pushFront(current);
                if (!successors.append(nextSuccessor))
                    return false;

                current = current->lastIns()->getSuccessor(nextSuccessor);
                nextSuccessor = 0;
                continue;
            }

            done.pushFront(current);
        }

        if (pending.empty())
            break;

        current = pending.popFront();
        current->unmark();
        nextSuccessor = successors.popCopy() + 1;
    }

    JS_ASSERT(pending.empty());
    JS_ASSERT(successors.empty());

    // Insert in reverse order so blocks are in RPO order in the graph
    while (!done.empty()) {
        current = done.popFront();
        current->unmark();
        graph.addBlock(current);
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
    MBasicBlock *startBlock = *graph.begin();
    startBlock->setImmediateDominator(startBlock);

    bool changed = true;

    while (changed) {
        changed = false;
        // We intentionally exclude the start node.
        MBasicBlockIterator block(graph.begin());
        block++;
        for (; block != graph.end(); block++) {
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
    ComputeImmediateDominators(graph);

    // Since traversing through the graph in post-order means that every use
    // of a definition is visited before the def itself. Since a def must
    // dominate all its uses, this means that by the time we reach a particular
    // block, we have processed all of its dominated children, so
    // block->numDominated() is accurate.
    for (PostorderIterator i(graph.poBegin()); *i != *graph.begin(); i++) {
        MBasicBlock *child = *i;
        MBasicBlock *parent = child->immediateDominator();

        if (!parent->addImmediatelyDominatedBlock(child))
            return false;

        // an additional +1 because of this child block.
        parent->addNumDominated(child->numDominated() + 1);
    }
    JS_ASSERT(graph.begin()->numDominated() == graph.numBlocks() - 1);
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

