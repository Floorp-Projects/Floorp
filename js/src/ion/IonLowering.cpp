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

// The lowering phase looks at each opcode and decides its result and input
// types.
class LoweringPhase : public MInstructionVisitor
{
    MIRGenerator *gen;
    MIRGraph &graph;

  public:
    LoweringPhase(MIRGenerator *gen, MIRGraph &graph)
      : gen(gen),
        graph(graph)
    { }

    bool lowerBlock(MBasicBlock *block);
    bool lowerInstruction(MInstruction *ins);
    bool analyze();
    bool insertBox(MInstruction *def, MOperand *operand);
    bool insertUnbox(MInstruction *def, MOperand *operand, MIRType type);
    bool insertConversion(MInstruction *def, MOperand *operand, MIRType type);

#define VISITOR(op) bool visit(M##op *ins) { return false; }
    MIR_OPCODE_LIST(VISITOR)
#undef VISITOR
};

bool
LoweringPhase::insertBox(MInstruction *def, MOperand *operand)
{
    return false;
}

bool
LoweringPhase::insertUnbox(MInstruction *def, MOperand *operand, MIRType type)
{
    return false;
}

bool
LoweringPhase::insertConversion(MInstruction *def, MOperand *operand, MIRType type)
{
    return false;
}

static inline bool
IsConversionPure(MIRType from, MIRType to)
{
    return from != MIRType_Object;
}

bool
LoweringPhase::lowerInstruction(MInstruction *ins)
{
    MInstruction *narrowed = NULL;
    MIRType usedAs = ins->usedAsType();
    if (usedAs != MIRType_Value && usedAs != ins->type() &&
        IsConversionPure(ins->type(), usedAs)) {
        // This instruction returns something, but all of its uses accept
        // either any type, a value, or a narrower type X. In this case, it
        // makes sense to attach a conversion operation after the definition
        // of this instruction, to avoid keeping the type tag alive longer than
        // needed. Note that we can only hoist conversions like this if the
        // definition has a snapshot attached, because the conversion is
        // fallible.
        narrowed = MConvert::New(gen, ins, usedAs);
        if (!narrowed)
            return false;
    }

    MUseIterator uses(ins);
    while (uses.more()) {
        MInstruction *use = uses->ins();
        MIRType required = use->requiredInputType(uses->index());

        // An instruction cannot return an abstract type, or if it has inputs,
        // can it accept "none" as a type.
        JS_ASSERT(ins->type() < MIRType_Any);
        JS_ASSERT(required < MIRType_None);

        // If the types are compatible, do nothing. Note that even though a
        // narrowed type is compatible with "any", we would like to replace it
        // anyway, to avoid potentially keeping a type tag alive longer that
        // necessary.
        if (required == ins->type() || (required == MIRType_Any && !narrowed)) {
            uses.next();
            continue;
        }

        // At this point, the required type is not equal to ins->type(), and
        // usedAs is either Value or a specific type. If required == usedAs,
        // then a narrowed version is available.
        //
        // We also take the more specialized version if the input accepts any
        // type.
        if (required == usedAs || (required == MIRType_Any && narrowed)) {
            JS_ASSERT(narrowed);
            use->replaceOperand(uses, narrowed);
            continue;
        }

        // At this point, if usedAs is not Value, then required == Value. If
        // usedAs is Value, required is anything.
        JS_ASSERT_IF(usedAs != MIRType_Value, required == MIRType_Value);

        // If a value is desired, create a box instruction near the use.
        if (required == MIRType_Value) {
            MBox *box = MBox::New(gen, ins);
            use->replaceOperand(uses, box);
            continue;
        }

        // At this point, usedAs is definitely Value, and required is not
        // Value. A narrow type is needed here, and this generally means type
        // information is poor or the JS code has messy types. For example, an
        // ADD with a known object input should not trigger an integer
        // specialization, but a boolean input could be okay at the cost of a
        // conversion.
        JS_ASSERT(required < MIRType_Value);
        MConvert *converted = MConvert::New(gen, ins, required);
        use->replaceOperand(uses, converted);
    }

    // Finally, ask the instruction to reserve its register allocation
    // structures.
    return ins->accept(this);
}

bool
LoweringPhase::lowerBlock(MBasicBlock *block)
{
    // Analyze phis first.
    for (size_t i = 0; i < block->numPhis(); i++) {
        if (!lowerInstruction(block->getPhi(i)))
            return false;
    }
    for (MInstructionIterator i = block->begin(); i != block->end(); i++) {
        if (!lowerInstruction(*i))
            return false;
    }
    return true;
}

bool
LoweringPhase::analyze()
{
    for (size_t i = 0; i < graph.numBlocks(); i++) {
        if (!lowerBlock(graph.getBlock(i)))
            return false;
    }
    return true;
}

bool
ion::Lower(MIRGenerator *gen, MIRGraph &graph)
{
    LoweringPhase phase(gen, graph);
    return phase.analyze();
}

