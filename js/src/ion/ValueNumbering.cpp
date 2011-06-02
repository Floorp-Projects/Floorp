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
 *   Ryan Pearl <rpearl@mozilla.com>
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

#include "Ion.h"
#include "ValueNumbering.h"
#include "IonSpewer.h"

using namespace js;
using namespace js::ion;

ValueNumberer::ValueNumberer(MIRGraph &graph)
  : graph_(graph)
{ }


uint32
ValueNumberer::lookupValue(ValueMap &values, MInstruction *ins)
{

    ValueMap::AddPtr p = values.lookupForAdd(ins);

    if (!p) {
        if (!values.add(p, ins, ins->id()))
            return 0;
    }

    return p->value;
}

bool
ValueNumberer::computeValueNumbers()
{
    // At the end of this function, we will have the value numbering stored in
    // each instruction.
    //
    // We also need an "optimistic" value number, for temporary use, which is
    // stored in a hashtable.
    //
    // For the instruction x := y op z, we map (op, VN[y], VN[z]) to a value
    // number, say v. If it is not in the map, we use the instruction id.
    //
    // If the instruction in question's value number is not already
    // v, we break the congruence we have and set it to v. We repeat until
    // saturation. This will take at worst O(d) time, where d is the loop
    // connectedness of the SSA def/use graph.
    //
    // The algorithm is the simple RPO-based algorithm from
    // "SCC-Based Value Numbering" by Cooper and Simpson
    //

    IonSpew(IonSpew_GVN, "Numbering instructions");

    ValueMap values;

    if (!values.init())
        return false;

    bool changed = true;

    while (changed) {
        changed = false;
        for (size_t i = 0; i < graph_.numBlocks(); i++) {
            MBasicBlock *block = graph_.getBlock(i);

            MDefinitionIterator iter(block);

            while (iter.more()) {
                MInstruction *ins = *iter;

                uint32 value = lookupValue(values, ins);

                if (!value)
                    return false; // Hashtable insertion failed

                if (ins->valueNumber() != value) {
                    ins->setValueNumber(value);
                    changed = true;
                }

                iter.next();
            }
        }
        values.clear();
    }
    return true;
}

MInstruction *
ValueNumberer::findDominatingInstruction(InstructionMap &defs, MInstruction *ins, size_t index)
{
    InstructionMap::Ptr p = defs.lookup(ins->valueNumber());
    MInstruction *dom;
    if (!p || index > p->value.validUntil) {
        DominatingValue value;
        value.def = ins;
        // Since we are traversing the dominator tree in pre-order, when we
        // are looking at the |index|-th block, the next numDominated() blocks
        // we traverse are precisely the set of blocks that are dominated.
        //
        // So, this value is visible in all blocks if:
        // index <= index + ins->block->numDominated()
        // and becomes invalid after that.
        value.validUntil = index + ins->block()->numDominated();

        if(!defs.put(ins->valueNumber(), value))
            return NULL;

        dom = ins;
    } else {
        dom = p->value.def;
    }

    return dom;
}

bool
ValueNumberer::eliminateRedundancies()
{
    // a definition d1 is redundant if it is dominated by another definition d2
    // which has the same value number.
    //
    // So, we traverse the dominator tree pre-order, maintaining a hashmap from
    // value numbers to instructions.
    //
    // For each definition d, say with value number v, we look up v in the
    // hashmap.
    //
    // If there is a definition d' in the hashmap, and the current traversal
    // index is within that instruction's dominated range then we eliminate d,
    // replacing all uses of d with uses of d' and removing it from the
    // instruction stream.
    //
    // If there is no valid definition in the hashtable (the current definition
    // is not in dominated scope), then we insert the current instruction.
    //

    InstructionMap defs;

    if (!defs.init())
        return false;

    IonSpew(IonSpew_GVN, "Eliminating redundant instructions");

    size_t index = 0;

    Vector<MBasicBlock *, 1, IonAllocPolicy> nodes;

    MBasicBlock *start = graph_.getBlock(0);
    if (!nodes.append(start))
        return false;

    while (!nodes.empty()) {
        MBasicBlock *block = nodes.popCopy();

        IonSpew(IonSpew_GVN, "Looking at block %d", block->id());

        for (size_t i = 0; i < block->numImmediatelyDominatedBlocks(); i++) {
            if (!nodes.append(block->getImmediatelyDominatedBlock(i)))
                return false;
        }
        MInstructionIterator i = block->begin();
        while (i != block->lastIns()) {
            MInstruction *ins = *i;
            MInstruction *dom = findDominatingInstruction(defs, ins, index);

            if (!dom)
                return false; // Insertion failed

            if (dom == ins) {
                i++;
                continue;
            }

            IonSpew(IonSpew_GVN, "instruction %d is dominated by instruction %d (from block %d)",
                    ins->id(), dom->id(), dom->block()->id());

            MUseIterator uses(ins);
            while (uses.more())
                uses->ins()->replaceOperand(uses, dom);

            JS_ASSERT(ins->useCount() == 0);
            JS_ASSERT(ins->block() == block);
            i = ins->block()->removeAt(i);
        }
        index++;
    }

    JS_ASSERT(index == graph_.numBlocks());
    return true;
}

// Exported method, called by the compiler.
bool
ValueNumberer::analyze()
{
    return computeValueNumbers() && eliminateRedundancies();
}
