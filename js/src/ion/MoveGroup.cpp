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
 *   Andrew Drake <adrake@adrake.org>
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

#include <cstdio>
#include "IonAllocPolicy.h"
#include "IonSpewer.h"
#include "MoveGroup.h"

using namespace js;
using namespace js::ion;

/*
 * This function takes the moves in a move group and resolves any cycles
 * in them by the insertion of a move to a temporary stack slot or available
 * register. The cycle resolution algorithm is greedy, and works as follows:
 *
 *   1. Select an arbitrary move A -> B from the move group, and add it to
 *      the work stack.
 *   2. If there is a move from B -> C, add it to the work stack, and repeat
 *      this step.
 *   3. If the work stack forms a cycle (i.e. the most recent 'to' is the
 *      original 'from') emit a move to a temporary.
 *   4. Pop each entry off the stack and emit the corresponding move.
 *
 * This algorithm works given the observation that all moves form "chains",
 * a move from A->B, and B->C, and C->A or C->D -- and chains are either
 * linear or closed loops: a value can not be moved two places in the same
 * move group, nor can two values be moved to the same place.
 *
 * If the chain is a cycle, we will find all moves in the cycle and emit them
 * with the temporary to break it. If the chain is not a cycle, we may not emit
 * all of it on the first iteration, but we are still guaranteed from step 2
 * that we haven't clobbered the inputs to any other moves, so correctness is
 * preserved.
 */
bool
MoveGroup::toInstructionsBefore(LBlock *block, LInstruction *ins, uint32 stack)
{
#ifdef DEBUG
    // Ensure that the "either linear or cyclic" property holds
    for (Entry *i = entries_.begin(); i != entries_.end(); i++) {
        for (Entry *j = entries_.begin(); j != i; j++) {
            JS_ASSERT(*i->from != *j->from);
            JS_ASSERT(*i->to != *j->to);
        }
    }
#endif

    Vector<Entry, 0, IonAllocPolicy> workStack;
    while (!entries_.empty()) {
        bool done;
        Entry *currentPos = &entries_.back();
        do {
            Entry current = *currentPos;
            entries_.erase(currentPos);
            if (!workStack.append(current))
                return false;

            done = true;
            for (Entry *i = entries_.begin(); i != entries_.end(); i++) {
                if (*i->from == *current.to) {
                    done = false;
                    currentPos = i;
                    break;
                }
            }
        } while (!done);

        spewWorkStack(workStack);

        LAllocation temp;
        bool cycle = *workStack.back().to == *workStack.begin()->from;
        if (cycle) {
            // We pick some arbitrary element of the cycle to figure out if
            // the value is a double
            bool isDouble = false;
            if (workStack[0].from->isFloatReg())
                isDouble = true;
            else if (workStack[0].from->isStackSlot())
                isDouble = workStack[0].from->toStackSlot()->isDouble();

            if (freeRegs.empty(isDouble)) {
                temp = LStackSlot(stack, isDouble);
            } else {
                if (isDouble)
                    temp = LFloatReg(freeRegs.takeAny(true).fpu());
                else
                    temp = LGeneralReg(freeRegs.takeAny(false).gpr());
            }
        }

        if (cycle)
            block->insertBefore(ins, new LMove(*workStack.back().to, temp));

        while (!workStack.empty()) {
            Entry *i = &workStack.back();
            if (cycle && i == workStack.begin())
                block->insertBefore(ins, new LMove(temp, *i->to));
            else
                block->insertBefore(ins, new LMove(*i->from, *i->to));
            workStack.erase(i);
        }
        if (cycle && (temp.isGeneralReg() || temp.isFloatReg()))
            freeRegs.add(temp.toRegister());
    }

    return true;
}

bool
MoveGroup::toInstructionsAfter(LBlock *block, LInstruction *ins, uint32 stack)
{
    LInstructionIterator iter(ins);
    iter++;
    return toInstructionsBefore(block, *iter, stack);
}

#ifdef DEBUG
void
MoveGroup::spewWorkStack(const Vector<Entry, 0, IonAllocPolicy>& workStack)
{
    IonSpew(IonSpew_LSRA, "  Resolving move chain:");
    for (Entry *i = workStack.begin(); i != workStack.end(); i++) {
        IonSpewHeader(IonSpew_LSRA);
        if (IonSpewEnabled(IonSpew_LSRA)) {
            fprintf(IonSpewFile, "   ");
            LAllocation::PrintAllocation(IonSpewFile, i->from);
            fprintf(IonSpewFile, " -> ");
            LAllocation::PrintAllocation(IonSpewFile, i->to);
            fprintf(IonSpewFile, "\n");
        }
    }
}
#endif
