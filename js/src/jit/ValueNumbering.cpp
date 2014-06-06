/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ValueNumbering.h"

#include "jit/IonSpewer.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace js::jit;

ValueNumberer::ValueNumberer(MIRGenerator *mir, MIRGraph &graph, bool optimistic)
  : mir(mir),
    graph_(graph),
    values(graph.alloc()),
    pessimisticPass_(!optimistic),
    count_(0)
{ }

TempAllocator &
ValueNumberer::alloc() const
{
    return graph_.alloc();
}

uint32_t
ValueNumberer::lookupValue(MDefinition *ins)
{
    ValueMap::AddPtr p = values.lookupForAdd(ins);
    if (p) {
        // make sure this is in the correct group
        setClass(ins, p->key());
        return p->value();
    }

    if (!values.add(p, ins, ins->id()))
        return 0;
    breakClass(ins);

    return ins->id();
}

MDefinition *
ValueNumberer::simplify(MDefinition *def, bool useValueNumbers)
{
    if (def->isEffectful())
        return def;

    MDefinition *ins = def->foldsTo(alloc(), useValueNumbers);
    if (ins == def)
        return def;

    // Ensure this instruction has a value number.
    if (!ins->valueNumberData())
        ins->setValueNumberData(new(alloc()) ValueNumberData);

    if (!ins->block()) {
        // In this case, we made a new def by constant folding, for
        // example, we replaced add(#3,#4) with a new const(#7) node.

        // We will only fold a phi into one of its operands.
        JS_ASSERT(!def->isPhi());

        def->block()->insertAfter(def->toInstruction(), ins->toInstruction());
        ins->setValueNumber(lookupValue(ins));
    }

    JS_ASSERT(ins->id() != 0);

    def->replaceAllUsesWith(ins);

    IonSpew(IonSpew_GVN, "Folding %d to be %d", def->id(), ins->id());
    return ins;
}

MControlInstruction *
ValueNumberer::simplifyControlInstruction(MControlInstruction *def)
{
    if (def->isEffectful())
        return def;

    MDefinition *repl = def->foldsTo(alloc(), false);
    if (repl == def)
        return def;

    // Ensure this instruction has a value number.
    if (!repl->valueNumberData())
        repl->setValueNumberData(new(alloc()) ValueNumberData);

    MBasicBlock *block = def->block();

    // MControlInstructions should not have consumers.
    JS_ASSERT(repl->isControlInstruction());
    JS_ASSERT(!def->hasUses());

    if (def->isInWorklist())
        repl->setInWorklist();

    block->discardLastIns();
    block->end(repl->toControlInstruction());
    return repl->toControlInstruction();
}

void
ValueNumberer::markDefinition(MDefinition *def)
{
    if (isMarked(def))
        return;

    IonSpew(IonSpew_GVN, "marked %d", def->id());
    def->setInWorklist();
    count_++;
}

void
ValueNumberer::unmarkDefinition(MDefinition *def)
{
    if (pessimisticPass_)
        return;

    JS_ASSERT(count_ > 0);
    IonSpew(IonSpew_GVN, "unmarked %d", def->id());
    def->setNotInWorklist();
    count_--;
}

void
ValueNumberer::markBlock(MBasicBlock *block)
{
    for (MDefinitionIterator iter(block); iter; iter++)
        markDefinition(*iter);
    markDefinition(block->lastIns());
}

void
ValueNumberer::markConsumers(MDefinition *def)
{
    if (pessimisticPass_)
        return;

    JS_ASSERT(!def->isInWorklist());
    JS_ASSERT(!def->isControlInstruction());
    for (MUseDefIterator use(def); use; use++)
        markDefinition(use.def());
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
    // v, we break the congruence and set it to v. We repeat until saturation.
    // This will take at worst O(d) time, where d is the loop connectedness
    // of the SSA def/use graph.
    //
    // The algorithm is the simple RPO-based algorithm from
    // "SCC-Based Value Numbering" by Cooper and Simpson.
    //
    // If we are performing a pessimistic pass, then we assume that every
    // definition is in its own congruence class, since we know nothing about
    // values that enter Phi nodes through back edges. We then make one pass
    // through the graph, ignoring back edges. This yields less congruences on
    // any graph with back-edges, but is much faster to perform.

    IonSpew(IonSpew_GVN, "Numbering instructions");

    if (!values.init())
        return false;
    // Stick a VN object onto every mdefinition
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        if (mir->shouldCancel("Value Numbering (preparation loop"))
            return false;
        for (MDefinitionIterator iter(*block); iter; iter++)
            iter->setValueNumberData(new(alloc()) ValueNumberData);
        MControlInstruction *jump = block->lastIns();
        jump->setValueNumberData(new(alloc()) ValueNumberData);
    }

    // Assign unique value numbers if pessimistic.
    // It might be productive to do this in the MDefinition constructor or
    // possibly in a previous pass, if it seems reasonable.
    if (pessimisticPass_) {
        for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
            for (MDefinitionIterator iter(*block); iter; iter++)
                iter->setValueNumber(iter->id());
        }
    } else {
        // For each root block, add all of its instructions to the worklist.
        markBlock(*(graph_.begin()));
        if (graph_.osrBlock())
            markBlock(graph_.osrBlock());
    }

    while (count_ > 0) {
#ifdef DEBUG
        if (!pessimisticPass_) {
            size_t debugCount = 0;
            IonSpew(IonSpew_GVN, "The following instructions require processing:");
            for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
                for (MDefinitionIterator iter(*block); iter; iter++) {
                    if (iter->isInWorklist()) {
                        IonSpew(IonSpew_GVN, "\t%d", iter->id());
                        debugCount++;
                    }
                }
                if (block->lastIns()->isInWorklist()) {
                    IonSpew(IonSpew_GVN, "\t%d", block->lastIns()->id());
                    debugCount++;
                }
            }
            if (!debugCount)
                IonSpew(IonSpew_GVN, "\tNone");
            JS_ASSERT(debugCount == count_);
        }
#endif
        for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
            if (mir->shouldCancel("Value Numbering (main loop)"))
                return false;
            for (MDefinitionIterator iter(*block); iter; ) {

                if (!isMarked(*iter)) {
                    iter++;
                    continue;
                }

                JS_ASSERT_IF(!pessimisticPass_, count_ > 0);
                unmarkDefinition(*iter);

                MDefinition *ins = simplify(*iter, false);

                if (ins != *iter) {
                    iter = block->discardDefAt(iter);
                    continue;
                }

                // Don't bother storing this instruction in the HashMap if
                // (a) eliminateRedundancies will never eliminate it (because
                // it's non-movable or effectful) and (b) no other instruction's
                // value number depends on it.
                if (!ins->hasDefUses() && (!ins->isMovable() || ins->isEffectful())) {
                    iter++;
                    continue;
                }

                uint32_t value = lookupValue(ins);

                if (!value)
                    return false; // Hashtable insertion failed

                if (ins->valueNumber() != value) {
                    IonSpew(IonSpew_GVN,
                            "Broke congruence for instruction %d (%p) with VN %d (now using %d)",
                            ins->id(), (void *) ins, ins->valueNumber(), value);
                    ins->setValueNumber(value);
                    markConsumers(ins);
                }

                iter++;
            }
            // Process control flow instruction:
            MControlInstruction *jump = block->lastIns();
            jump = simplifyControlInstruction(jump);

            // If we are pessimistic, then this will never get set.
            if (!jump->isInWorklist())
                continue;
            unmarkDefinition(jump);
            if (jump->valueNumber() == 0) {
                jump->setValueNumber(jump->id());
                for (size_t i = 0; i < jump->numSuccessors(); i++)
                    markBlock(jump->getSuccessor(i));
            }

        }

        // If we are doing a pessimistic pass, we only go once through the
        // instruction list.
        if (pessimisticPass_)
            break;
    }
#ifdef DEBUG
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        for (MDefinitionIterator iter(*block); iter; iter++) {
            JS_ASSERT(!iter->isInWorklist());
            JS_ASSERT_IF(iter->valueNumber() == 0,
                         !iter->hasDefUses() && (!iter->isMovable() || iter->isEffectful()));
        }
    }
#endif
    return true;
}

MDefinition *
ValueNumberer::findDominatingDef(InstructionMap &defs, MDefinition *ins, size_t index)
{
    JS_ASSERT(ins->valueNumber() != 0);
    InstructionMap::Ptr p = defs.lookup(ins->valueNumber());
    MDefinition *dom;
    if (!p || index >= p->value().validEnd) {
        DominatingValue value;
        value.def = ins;
        // Since we are traversing the dominator tree in pre-order, when we
        // are looking at the |index|-th block, the next numDominated() blocks
        // we traverse are precisely the set of blocks that are dominated.
        //
        // So, this value is visible in all blocks if:
        // index < index + ins->block->numDominated()
        // and becomes invalid after that.
        value.validEnd = index + ins->block()->numDominated();

        if(!defs.put(ins->valueNumber(), value))
            return nullptr;

        dom = ins;
    } else {
        dom = p->value().def;
    }

    return dom;
}

bool
ValueNumberer::eliminateRedundancies()
{
    // A definition is 'redundant' iff it is dominated by another definition
    // with the same value number.
    //
    // So, we traverse the dominator tree in pre-order, maintaining a hashmap
    // from value numbers to instructions.
    //
    // For each definition d with value number v, we look up v in the hashmap.
    //
    // If there is a definition d' in the hashmap, and the current traversal
    // index is within that instruction's dominated range, then we eliminate d,
    // replacing all uses of d with uses of d'.
    //
    // If there is no valid definition in the hashtable (the current definition
    // is not in dominated scope), then we insert the current instruction,
    // since it is the most dominant instruction with the given value number.

    InstructionMap defs(alloc());

    if (!defs.init())
        return false;

    IonSpew(IonSpew_GVN, "Eliminating redundant instructions");

    // Stack for pre-order CFG traversal.
    Vector<MBasicBlock *, 1, IonAllocPolicy> worklist(alloc());

    // The index of the current block in the CFG traversal.
    size_t index = 0;

    // Add all self-dominating blocks to the worklist.
    // This includes all roots. Order does not matter.
    for (MBasicBlockIterator i(graph_.begin()); i != graph_.end(); i++) {
        MBasicBlock *block = *i;
        if (block->immediateDominator() == block) {
            if (!worklist.append(block))
                return false;
        }
    }

    // Starting from each self-dominating block, traverse the CFG in pre-order.
    while (!worklist.empty()) {
        if (mir->shouldCancel("Value Numbering (eliminate loop)"))
            return false;
        MBasicBlock *block = worklist.popCopy();

        IonSpew(IonSpew_GVN, "Looking at block %d", block->id());

        // Add all immediate dominators to the front of the worklist.
        if (!worklist.append(block->immediatelyDominatedBlocksBegin(),
                             block->immediatelyDominatedBlocksEnd())) {
            return false;
        }

        // For each instruction, attempt to look up a dominating definition.
        for (MDefinitionIterator iter(block); iter; ) {
            MDefinition *ins = simplify(*iter, true);

            // Instruction was replaced, and all uses have already been fixed.
            if (ins != *iter) {
                iter = block->discardDefAt(iter);
                continue;
            }

            // Instruction has side-effects and cannot be folded.
            if (!ins->isMovable() || ins->isEffectful()) {
                iter++;
                continue;
            }

            MDefinition *dom = findDominatingDef(defs, ins, index);
            if (!dom)
                return false; // Insertion failed.

            if (dom == ins || !dom->updateForReplacement(ins)) {
                iter++;
                continue;
            }

            IonSpew(IonSpew_GVN, "instruction %d is dominated by instruction %d (from block %d)",
                    ins->id(), dom->id(), dom->block()->id());

            ins->replaceAllUsesWith(dom);

            JS_ASSERT(!ins->hasUses());
            JS_ASSERT(ins->block() == block);
            JS_ASSERT(!ins->isEffectful());
            JS_ASSERT(ins->isMovable());

            iter = ins->block()->discardDefAt(iter);
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

// Called by the compiler if we need to re-run GVN.
bool
ValueNumberer::clear()
{
    IonSpew(IonSpew_GVN, "Clearing value numbers");

    // Clear the VN of every MDefinition
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        if (mir->shouldCancel("Value Numbering (clearing)"))
            return false;
        for (MDefinitionIterator iter(*block); iter; iter++)
            iter->clearValueNumberData();
        block->lastIns()->clearValueNumberData();
    }

    return true;
}

uint32_t
MDefinition::valueNumber() const
{
    JS_ASSERT(block_);
    if (valueNumber_ == nullptr)
        return 0;
    return valueNumber_->valueNumber();
}
void
MDefinition::setValueNumber(uint32_t vn)
{
    valueNumber_->setValueNumber(vn);
}
// Set the class of this to the given representative value.
void
ValueNumberer::setClass(MDefinition *def, MDefinition *rep)
{
    def->valueNumberData()->setClass(def, rep);
}

MDefinition *
ValueNumberer::findSplit(MDefinition *def)
{
    for (MDefinition *vncheck = def->valueNumberData()->classNext;
         vncheck != nullptr;
         vncheck = vncheck->valueNumberData()->classNext) {
        if (!def->congruentTo(vncheck)) {
            IonSpew(IonSpew_GVN, "Proceeding with split because %d is not congruent to %d",
                    def->id(), vncheck->id());
            return vncheck;
        }
    }
    return nullptr;
}

void
ValueNumberer::breakClass(MDefinition *def)
{
    if (def->valueNumber() == def->id()) {
        IonSpew(IonSpew_GVN, "Breaking congruence with itself: %d", def->id());
        ValueNumberData *defdata = def->valueNumberData();
        JS_ASSERT(defdata->classPrev == nullptr);
        // If the def was the only member of the class, then there is nothing to do.
        if (defdata->classNext == nullptr)
            return;
        // If upon closer inspection, we are still equivalent to this class
        // then there isn't anything for us to do.
        MDefinition *newRep = findSplit(def);
        if (!newRep)
            return;
        markConsumers(def);
        ValueNumberData *newdata = newRep->valueNumberData();

        // Right now, |defdata| is at the front of the list, and |newdata| is
        // somewhere in the middle.
        //
        // We want to move |defdata| and everything up to but excluding
        // |newdata| to a new list, with |defdata| still as the canonical
        // element.
        //
        // We then want to take |newdata| and everything after, and
        // mark them for processing (since |newdata| is now a new canonical
        // element).
        //
        MDefinition *lastOld = newdata->classPrev;

        JS_ASSERT(lastOld); // newRep is NOT the first element of the list.
        JS_ASSERT(lastOld->valueNumberData()->classNext == newRep);

        //lastOld is now the last element of the old list (congruent to
        //|def|)
        lastOld->valueNumberData()->classNext = nullptr;

#ifdef DEBUG
        for (MDefinition *tmp = def; tmp != nullptr; tmp = tmp->valueNumberData()->classNext) {
            JS_ASSERT(tmp->valueNumber() == def->valueNumber());
            JS_ASSERT(tmp->congruentTo(def));
            JS_ASSERT(tmp != newRep);
        }
#endif
        //|newRep| is now the first element of a new list, therefore it is the
        //new canonical element. Mark the remaining elements in the list
        //(including |newRep|)
        newdata->classPrev = nullptr;
        IonSpew(IonSpew_GVN, "Choosing a new representative: %d", newRep->id());

        // make the VN of every member in the class the VN of the new representative number.
        for (MDefinition *tmp = newRep; tmp != nullptr; tmp = tmp->valueNumberData()->classNext) {
            // if this instruction is already scheduled to be processed, don't do anything.
            if (tmp->isInWorklist()) {
                IonSpew(IonSpew_GVN, "Defer  to a new congruence class: %d", tmp->id());
                continue;
            }
            IonSpew(IonSpew_GVN, "Moving to a new congruence class: %d", tmp->id());
            tmp->setValueNumber(newRep->id());
            markConsumers(tmp);
            markDefinition(tmp);
        }

        // Insert the new representative => number mapping into the table
        // Logically, there should not be anything in the table currently, but
        // old values are never removed, so there's a good chance something will
        // already be there.
        values.put(newRep, newRep->id());
    } else {
        // The element that is breaking from the list isn't the representative element
        // just strip it from the list
        ValueNumberData *defdata = def->valueNumberData();
        if (defdata->classPrev)
            defdata->classPrev->valueNumberData()->classNext = defdata->classNext;
        if (defdata->classNext)
            defdata->classNext->valueNumberData()->classPrev = defdata->classPrev;

        // Make sure there is no nastinees accidentally linking elements into the old list later.
        defdata->classPrev = nullptr;
        defdata->classNext = nullptr;
    }
}
