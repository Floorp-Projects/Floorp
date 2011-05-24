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

  public:
    TypeAnalyzer(MIRGraph &graph);

    bool analyze();
    void inspectOperands(MInstruction *ins);
    bool propagateUsedTypes(MInstruction *ins);
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

void
TypeAnalyzer::inspectOperands(MInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i++) {
        MIRType required = ins->requiredInputType(i);
        if (required >= MIRType_Value)
            continue;
        ins->getInput(i)->useAsType(required);
    }
}

bool
TypeAnalyzer::propagateUsedTypes(MInstruction *ins)
{
    // If this is a copy, just propagate to its sole input.
    if (ins->isCopy()) {
        MCopy *copy = ins->toCopy();
        MInstruction *input = copy->getInput(0);
        input->addUsedTypes(copy->usedTypes());
        return true;
    }

    // Otherwise, propagate the phi's used types to each input.
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
TypeAnalyzer::analyze()
{
    // Populate the worklist in block order. Instructions will be popped in
    // LIFO order, as we would like to see uses before their defs.
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        MBasicBlock *block = graph.getBlock(i);
        for (size_t i = 0; i < block->numPhis(); i++) {
            if (!addToWorklist(block->getPhi(i)))
                return false;
        }
        for (MInstructionIterator i = block->begin(); i != block->end(); i++) {
            if (!addToWorklist(*i))
                return false;
        }
    }

    while (!worklist.empty()) {
        MInstruction *ins = popFromWorklist();
        if (ins->isPhi() || ins->isCopy()) {
            if (!propagateUsedTypes(ins))
                return false;
        } else {
            // We see all uses before their defs, so we do not reflow type
            // information on a normal instruction.
            inspectOperands(ins);
        }
    }

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

