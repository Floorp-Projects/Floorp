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
#include "IonSpew.h"
#include "IonAnalysis.h"

using namespace js;
using namespace js::ion;

// The type analyzer attempts to decide every instruction's output type. The
// following properties of instructions are used:
//
//  * Assumed Type. This encodes information from the TypeOracle. For example,
//    it may suggest that a BitAnd is known to not be specialized, or that a
//    getprop is known to be an integer.
//
//  * Requested Types. This is a bit vector of the types required by uses.
//
//  * Result Type. This is the type that will be used based on analysis of
//    uses. Some operations have a fixed result type. For example, a BitAnd
//    always returns an integer, and uses which request a wider type must
//    explicitly convert it.
//
// The algorithm works as follows:
//  * Each instruction is added to a worklist.
//  * For each instruction in the worklist,
//    * The instruction decides, based on its assumed type, its ideal inputs
//      types. These are propagated to its inputs' requested types.
//    * The instruction decides, based on its inputs, if it should widen its
//      result type.
//    * If any type information changes, its uses and inputs are re-added to
//      the worklist.
//  * During the lowering phase, each instruction decides whether to specialize
//    as a specific type, and whether to add conversion operations to its
//    inputs.
//
class TypeAnalyzer
{
    MIRGraph &graph;
    js::Vector<MInstruction *, 0, IonAllocPolicy> worklist;

  private:
    bool addToWorklist(MInstruction *ins);
    MInstruction *popFromWorklist();
    bool canSpecializeAtDef(MInstruction *ins);
    bool reflow(MInstruction *ins);

    bool populate();
    bool propagate();
    bool insertConversions();

    // Type propagation.
    bool inspectUses(MInstruction *ins);
    bool inspectOperands(MInstruction *ins);
    bool propagateUsedTypes(MInstruction *ins);

    // Type conversion.
    bool specializePhi(MPhi *phi);
    bool fixup(MInstruction *ins);
    void rewriteUses(MInstruction *old, MInstruction *ins);

  public:
    TypeAnalyzer(MIRGraph &graph);

    bool analyze();
};

TypeAnalyzer::TypeAnalyzer(MIRGraph &graph)
  : graph(graph)
{
}

bool
TypeAnalyzer::addToWorklist(MInstruction *ins)
{
    if (!ins->inWorklist()){
        ins->setInWorklist();
        return worklist.append(ins);
    }
    return true;
}

MInstruction *
TypeAnalyzer::popFromWorklist()
{
    MInstruction *ins = worklist.popCopy();
    ins->setNotInWorklist();
    return ins;
}

bool
TypeAnalyzer::populate()
{
    // Populate the worklist in block order. Instructions will be popped in
    // LIFO order, as we would like to see uses before their defs.
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
        for (size_t i = 0; i < block->numPhis(); i++) {
            if (!addToWorklist(block->getPhi(i)))
                return false;
        }
        MInstructionIterator i = block->begin();
        while (i != block->end()) {
            if (i->isCopy()) {
                // Remove copies.
                MCopy *copy = i->toCopy();
                MUseIterator uses(copy);
                while (uses.more())
                    uses->ins()->replaceOperand(uses, copy->getInput(0));
                i = copy->block()->removeAt(i);
                continue;
            }
            addToWorklist(*i);
            i++;
        }
    }
    
    return true;
}

// Annotate each operand with the type it should be for this instruction.
bool
TypeAnalyzer::inspectOperands(MInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i++) {
        MIRType required = ins->requiredInputType(i);
        if (required >= MIRType_Value)
            continue;
        ins->getInput(i)->useAsType(required);
    }

    return true;
}

// If this instruction will be narrowed by the final type analysis phase,
// adjust any uses that will need to be despecialized.
bool
TypeAnalyzer::inspectUses(MInstruction *ins)
{
    if (!ins->uses() || ins->type() == MIRType_None)
        return true;

    MIRType usedAs = ins->usedAsType();
    if (usedAs == MIRType_Value || ins->type() == usedAs)
        return true;

    // This instruction will narrow from Value to usedAs, so go through its
    // uses and ask each to adjust its specialization based on the new
    // information.
    MUseIterator uses(ins);
    while (uses.more()) {
        if (uses->ins()->adjustForInputs()) {
            if (!addToWorklist(uses->ins()))
                return false;
        }
        uses.next();
    }

    return true;
}

bool
TypeAnalyzer::propagateUsedTypes(MInstruction *ins)
{
    // Propagate the phi's used types to each input.
    MPhi *phi = ins->toPhi();
    for (size_t i = 0; i < phi->numOperands(); i++) {
        MInstruction *input = phi->getInput(i);
        bool changed = input->addUsedTypes(phi->usedTypes());
        if (changed && (input->isPhi() || ins->isCopy())) {
            // If the set of types on the input changed, and the input will
            // need to propagate its information again, then re-add it to the
            // worklist.
            if (!addToWorklist(input))
                return false;
        }
    }

    return true;
}

bool
TypeAnalyzer::propagate()
{
    // Propagate information about desired types.
    while (!worklist.empty()) {
        MInstruction *ins = popFromWorklist();

        // Copies should have been removed earlier.
        JS_ASSERT(!ins->isCopy());

        if (ins->isPhi()) {
            if (!propagateUsedTypes(ins))
                return false;
            if (!inspectUses(ins))
                return false;
        } else {
            if (!inspectUses(ins))
                return false;
            if (!inspectOperands(ins))
                return false;
        }
    }

    return true;
}

// Rewrites uses of an old definition to point at a new, narrower-typed
// version. The purpose of this is to shorten the lifetime of the type tag on
// platforms with nun/wideboxing.
void
TypeAnalyzer::rewriteUses(MInstruction *old, MInstruction *ins)
{
    JS_ASSERT(old->type() == MIRType_Value && ins->type() < MIRType_Value);

    MUseIterator iter(old);
    while (iter.more()) {
        MInstruction *use = iter->ins();

        // We try to replace uses that accept any representation (boxed or
        // unboxed). Note, this will also rewrite entries in snapshots, which
        // is okay, because the unbox operation itself is resumable.
        MIRType required = use->requiredInputType(iter->index());
        if (required != MIRType_Any && required != ins->type())
        {
            iter.next();
            continue;
        }

        use->replaceOperand(iter, ins);
    }
}

bool
TypeAnalyzer::specializePhi(MPhi *phi)
{
    // If the phi is used as more than one type, or always as a box, don't
    // bother specializing it.
    MIRType usedAs = phi->usedAsType();
    if (usedAs == MIRType_Value)
        return true;

    // See if all its inputs can be specialized at their definition.
    for (size_t i = 0; i < phi->numOperands(); i++) {
        MInstruction *ins = phi->getInput(i);
        if (ins->type() == usedAs)
            continue;

        // Give up if the type is not a value. A conversion near the def would
        // break semantics if a guard failed after the phi.
        if (ins->type() != MIRType_Value)
            return true;
    }

    MBasicBlock *block = phi->block();
    MUnbox *unbox = MUnbox::New(phi, usedAs);
    block->insertAfter(*block->begin(), unbox);
    rewriteUses(phi, unbox);

    return true;
}

bool
TypeAnalyzer::fixup(MInstruction *ins)
{
    if (!ins->uses())
        return true;

    // Add unbox operations near the definition, if possible.
    MIRType usedAs = ins->usedAsType();
    if (usedAs != MIRType_Value && ins->type() == MIRType_Value) {
        MUnbox *unbox = MUnbox::New(ins, usedAs);
        MBasicBlock *block = ins->block();
        if (ins->isPhi())
            block->insertAfter(*block->begin(), unbox);
        else
            block->insertAfter(ins, unbox);
        rewriteUses(ins, unbox);
    }

    MUseIterator uses(ins);
    while (uses.more()) {
        MInstruction *use = uses->ins();
        MIRType required = use->requiredInputType(uses->index());
        MIRType actual = ins->type();

        if (required == actual || required == MIRType_Any) {
            // No conversion is needed.
            uses.next();
            continue;
        }

        if (required == MIRType_Value) {
            // The input is a specific type, but it must be boxed.
            MBox *box = MBox::New(ins);
            if (use->isPhi()) {
                MBasicBlock *pred = use->block()->getPredecessor(uses->index());
                pred->insertBefore(pred->lastIns(), box);
            } else {
                use->block()->insertBefore(use, box);
            }
            use->replaceOperand(uses, box);
        } else if (actual == MIRType_Value) {
            // The input is a value, but it wants explicit unboxing.
            MUnbox *unbox = MUnbox::New(ins, required);
            use->block()->insertBefore(use, unbox);
            use->replaceOperand(uses, unbox);
        } else {
            // Type mismatch. We'll figure out how to deal with this later.
            JS_NOT_REACHED("NYI");
        }
    }

    return true;
}

bool
TypeAnalyzer::insertConversions()
{
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
        for (size_t i = 0; i < block->numPhis(); i++) {
            if (!specializePhi(block->getPhi(i)))
                return false;
            if (!fixup(block->getPhi(i)))
                return false;
        }
        for (MInstructionIterator iter = block->begin(); iter != block->end(); iter++) {
            if (!fixup(*iter))
                return false;
        }
    }

    return true;
}

bool
TypeAnalyzer::analyze()
{
    if (!populate())
        return false;
    if (!propagate())
        return false;
    if (!insertConversions())
        return false;
    return true;
}

bool
ion::ApplyTypeInformation(MIRGraph &graph)
{
    TypeAnalyzer analysis(graph);
    if (!analysis.analyze())
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

