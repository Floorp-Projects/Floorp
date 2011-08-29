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

#include "Ion.h"
#include "IonSpewer.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "IonBuilder.h"
#include "jsemit.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

MIRGenerator::MIRGenerator(JSContext *cx, TempAllocator &temp, JSScript *script, JSFunction *fun,
                           MIRGraph &graph)
  : script(script),
    cx(cx),
    pc(NULL),
    temp_(temp),
    fun_(fun),
    graph_(graph),
    error_(false)
{
    nslots_ = script->nslots + CountArgSlots(fun);
}

bool
MIRGenerator::abort(const char *message, ...)
{
    va_list ap;
    va_start(ap, message);
    IonSpewVA(IonSpew_Abort, message, ap);
    va_end(ap);
    error_ = true;
    return false;
}

void
MIRGraph::addBlock(MBasicBlock *block)
{
    block->setId(blockIdGen_++);
    blocks_.pushBack(block);
#ifdef DEBUG
    numBlocks_++;
#endif
}

void
MIRGraph::unmarkBlocks() {
    for (MBasicBlockIterator i(blocks_.begin()); i != blocks_.end(); i++)
        i->unmark();
}

MBasicBlock *
MBasicBlock::New(MIRGenerator *gen, MBasicBlock *pred, jsbytecode *entryPc, Kind kind)
{
    MBasicBlock *block = new MBasicBlock(gen, entryPc, kind);
    if (!block->init())
        return NULL;

    if (!block->inherit(pred))
        return NULL;

    return block;
}

MBasicBlock *
MBasicBlock::NewPendingLoopHeader(MIRGenerator *gen, MBasicBlock *pred, jsbytecode *entryPc)
{
    return MBasicBlock::New(gen, pred, entryPc, PENDING_LOOP_HEADER);
}

MBasicBlock *
MBasicBlock::NewSplitEdge(MIRGenerator *gen, MBasicBlock *pred)
{
    return MBasicBlock::New(gen, pred, pred->pc(), SPLIT_EDGE);
}

MBasicBlock::MBasicBlock(MIRGenerator *gen, jsbytecode *pc, Kind kind)
  : gen_(gen),
    slots_(NULL),
    stackPosition_(gen->firstStackSlot()),
    lastIns_(NULL),
    pc_(pc),
    lir_(NULL),
    start_(NULL),
    successorWithPhis_(NULL),
    positionInPhiSuccessor_(0),
    kind_(kind),
    mark_(false),
    immediateDominator_(NULL),
    numDominated_(0)
{
}

bool
MBasicBlock::init()
{
    slots_ = gen()->allocate<StackSlot>(gen()->nslots());
    if (!slots_)
        return false;
    return true;
}

void
MBasicBlock::copySlots(MBasicBlock *from)
{
    stackPosition_ = from->stackPosition_;

    for (uint32 i = 0; i < stackPosition_; i++)
        slots_[i] = from->slots_[i];
}

bool
MBasicBlock::inherit(MBasicBlock *pred)
{
    if (pred)
        copySlots(pred);

    // Create a snapshot using our initial stack state.
    entrySnapshot_ = new MSnapshot(this, pc());
    if (!entrySnapshot_->init(this))
        return false;

    if (pred) {
        if (!predecessors_.append(pred))
            return false;

        for (size_t i = 0; i < stackDepth(); i++)
            entrySnapshot()->initOperand(i, getSlot(i));
    }

    return true;
}

MDefinition *
MBasicBlock::getSlot(uint32 index)
{
    JS_ASSERT(index < stackPosition_);
    return slots_[index].def;
}

void
MBasicBlock::initSlot(uint32 slot, MDefinition *ins)
{
    slots_[slot].set(ins);
    entrySnapshot()->initOperand(slot, ins);
}

void
MBasicBlock::setSlot(uint32 slot, MDefinition *ins)
{
    StackSlot &var = slots_[slot];

    // If |var| is copied, we must fix any of its copies so that they point to
    // a usable value.
    if (var.isCopied()) {
        // Find the lowest copy on the stack, to preserve the invariant that
        // the copy list starts near the top of the stack and proceeds
        // toward the bottom.
        uint32 lowest = var.firstCopy;
        uint32 prev = NotACopy;
        do {
            uint32 next = slots_[lowest].nextCopy;
            if (next == NotACopy)
                break;
            JS_ASSERT(next < lowest);
            prev = lowest;
            lowest = next;
        } while (true);

        // Rewrite every copy.
        for (uint32 copy = var.firstCopy; copy != lowest;) {
            slots_[copy].copyOf = lowest;

            uint32 next = slots_[copy].nextCopy;

            // The node whose "nextCopy" field is |lowest| is now the
            // terminator of the list.
            if (slots_[copy].nextCopy == lowest)
                slots_[copy].nextCopy = NotACopy;

            copy = next;
        }

        // Make the lowest the new store.
        slots_[lowest].copyOf = NotACopy;
        slots_[lowest].firstCopy = prev;
    }

    var.set(ins);
}

// We must avoid losing copies when inserting phis. For example, the code
// on the left must not be naively rewritten as shown.
//   t = 1           ||   0: const(#1)   ||   0: const(#1)
//   i = t           \|   do {           \|   do {
//   do {             ->    2: add(0, 0)  ->   1: phi(0, 1)
//      i = i + t    /|                  /|    2: add(1, 1)
//   } ...           ||   } ...          ||   } ...
//
// Which is not correct. To solve this, we create a new SSA name at the point
// where |t| is assigned to |i|, like so:
//   t = 1          ||   0: const(#1)    ||   0: const(#1)
//   t' = copy(t)   ||   1: copy(0)      ||   1: copy(0)
//   i = t'         \|   do {            \|   do {
//   do {            ->    3: add(0, 1)   ->    2: phi(1, 3)
//      i = i + t   /|   } ...           /|     3: add(0, 2)
//   } ...          ||                   ||   } ...
//                  ||                   ||
//
// We assume that the only way such copies can be created is via simple
// assignment, like (x = y), which will be reflected in the bytecode via
// a GET[LOCAL,ARG] that inherits into a SET[LOCAL,ARG]. Normal calls
// to push() will be compiler-created temporaries. So to minimize creation of
// new SSA names, we lazily create them when applying a setVariable() whose
// stack top was pushed by a pushVariable(). That also means we do not create
// "copies" for calls to push().
bool
MBasicBlock::setVariable(uint32 index)
{
    JS_ASSERT(stackPosition_ > gen()->firstStackSlot());
    StackSlot &top = slots_[stackPosition_ - 1];

    MDefinition *def = top.def;
    if (top.isCopy()) {
        // Set the local variable to be a copy of |def|. Note that unlike
        // JaegerMonkey, no complicated logic is needed to figure out how to
        // make |top| a copy of |var|. There is no need, because we only care
        // about (1) popping being fast, thus the backwarding ordering of
        // copies, and (2) knowing when a GET flows into a SET.
        MInstruction *ins = MCopy::New(def);
        add(ins);
        def = ins;
    }

    setSlot(index, def);

    if (!top.isCopy()) {
        // If the top is not a copy, we make it one anyway, in case the
        // bytecode ever emits something like:
        //    GETELEM
        //    SETLOCAL 0
        //    SETLOCAL 1
        //
        // In this case, we want the second assignment to act as though there
        // was an intervening POP; GETLOCAL. Note that |def| is already
        // correct, because we only created a new instruction if |top.isCopy()|
        // was true.
        top.copyOf = index;
        top.nextCopy = slots_[index].firstCopy;
        slots_[index].firstCopy = stackPosition_ - 1;
    }

    return true;
}

bool
MBasicBlock::setArg(uint32 arg)
{
    // :TODO:  assert not closed
    return setVariable(gen()->argSlot(arg));
}

bool
MBasicBlock::setLocal(uint32 local)
{
    // :TODO:  assert not closed
    return setVariable(gen()->localSlot(local));
}

void
MBasicBlock::push(MDefinition *ins)
{
    JS_ASSERT(stackPosition_ < gen()->nslots());
    slots_[stackPosition_].set(ins);
    stackPosition_++;
}

void
MBasicBlock::pushVariable(uint32 slot)
{
    if (slots_[slot].isCopy())
        slot = slots_[slot].copyOf;

    JS_ASSERT(stackPosition_ < gen()->nslots());
    StackSlot &to = slots_[stackPosition_];
    StackSlot &from = slots_[slot];

    to.def = from.def;
    to.copyOf = slot;
    to.nextCopy = from.firstCopy;
    from.firstCopy = stackPosition_;

    stackPosition_++;
}

void
MBasicBlock::pushArg(uint32 arg)
{
    // :TODO:  assert not closed
    pushVariable(gen()->argSlot(arg));
}

void
MBasicBlock::pushLocal(uint32 local)
{
    // :TODO:  assert not closed
    pushVariable(gen()->localSlot(local));
}

MDefinition *
MBasicBlock::pop()
{
    JS_ASSERT(stackPosition_ > gen()->firstStackSlot());

    StackSlot &slot = slots_[--stackPosition_];
    if (slot.isCopy()) {
        // The latest copy is at the top of the stack, and is the first copy
        // in the linked list. We only remove copies from the head of the list.
        StackSlot &backing = slots_[slot.copyOf];
        JS_ASSERT(backing.isCopied());
        JS_ASSERT(backing.firstCopy == stackPosition_);

        backing.firstCopy = slot.nextCopy;
    }

    // The slot cannot have live copies if it is being removed.
    JS_ASSERT(!slot.isCopied());

    return slot.def;
}

MDefinition *
MBasicBlock::peek(int32 depth)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(stackPosition_ + depth >= gen()->firstStackSlot());
    return getSlot(stackPosition_ + depth);
}

void
MBasicBlock::remove(MInstruction *ins)
{
    instructions_.remove(ins);
}

MInstructionIterator
MBasicBlock::removeAt(MInstructionIterator &iter)
{
    for (size_t i = 0; i < iter->numOperands(); i++)
        iter->replaceOperand(i, NULL);

    return instructions_.removeAt(iter);
}

MInstructionReverseIterator
MBasicBlock::removeAt(MInstructionReverseIterator &iter)
{
    for (size_t i = 0; i < iter->numOperands(); i++)
        iter->replaceOperand(i, NULL);

    return instructions_.removeAt(iter);
}

MDefinitionIterator
MBasicBlock::removeDefAt(MDefinitionIterator &old)
{
    MDefinitionIterator iter(old);

    if (iter.atPhi())
        iter.phiIter_ = iter.block_->removePhiAt(iter.phiIter_);
    else
        iter.iter_ = iter.block_->removeAt(iter.iter_);

    return iter;
}

void
MBasicBlock::insertBefore(MInstruction *at, MInstruction *ins)
{
    ins->setBlock(this);
    gen()->graph().allocDefinitionId(ins);
    instructions_.insertBefore(at, ins);
}

void
MBasicBlock::insertAfter(MInstruction *at, MInstruction *ins)
{
    ins->setBlock(this);
    gen()->graph().allocDefinitionId(ins);
    instructions_.insertAfter(at, ins);
}

void
MBasicBlock::add(MInstruction *ins)
{
    JS_ASSERT(!lastIns_);
    ins->setBlock(this);
    gen()->graph().allocDefinitionId(ins);
    instructions_.pushBack(ins);
}

void
MBasicBlock::end(MControlInstruction *ins)
{
    JS_ASSERT(ins);
    add(ins);
    lastIns_ = ins;
}

void
MBasicBlock::addPhi(MPhi *phi)
{
    phis_.pushBack(phi);
    phi->setBlock(this);
    gen()->graph().allocDefinitionId(phi);
}

MPhiIterator
MBasicBlock::removePhiAt(MPhiIterator &at)
{
    JS_ASSERT(!phis_.empty());

    for (size_t i = 0; i < at->numOperands(); i++)
        at->replaceOperand(i, NULL);

    MPhiIterator result = phis_.removeAt(at);

    if (phis_.empty()) {
        for (MBasicBlock **pred = predecessors_.begin(); pred != predecessors_.end(); pred++)
            (*pred)->setSuccessorWithPhis(NULL, 0);
    }
    return result;
}

bool
MBasicBlock::addPredecessor(MBasicBlock *pred)
{
    JS_ASSERT(predecessors_.length() > 0);

    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(pred->lastIns_);
    JS_ASSERT(pred->stackPosition_ == stackPosition_);

    for (uint32 i = 0; i < stackPosition_; i++) {
        MDefinition *mine = getSlot(i);
        MDefinition *other = pred->getSlot(i);

        if (mine != other) {
            MPhi *phi;

            // If the current instruction is a phi, and it was created in this
            // basic block, then we have already placed this phi and should
            // instead append to its operands.
            if (mine->isPhi() && mine->block() == this) {
                JS_ASSERT(predecessors_.length());
                phi = mine->toPhi();
            } else {
                // Otherwise, create a new phi node.
                phi = MPhi::New(i);
                addPhi(phi);

                // Prime the phi for each predecessor, so input(x) comes from
                // predecessor(x).
                for (size_t j = 0; j < predecessors_.length(); j++) {
                    JS_ASSERT(predecessors_[j]->getSlot(i) == mine);
                    if (!phi->addInput(mine))
                        return false;
                }

                setSlot(i, phi);
                entrySnapshot()->replaceOperand(i, phi);
            }

            if (!phi->addInput(other))
                return false;
        }
    }

    return predecessors_.append(pred);
}

bool
MBasicBlock::addImmediatelyDominatedBlock(MBasicBlock *child)
{
    return immediatelyDominated_.append(child);
}

void
MBasicBlock::assertUsesAreNotWithin(MUseIterator use, MUseIterator end)
{
#ifdef DEBUG
    for (; use != end; use++) {
        JS_ASSERT_IF(use->node()->isDefinition(),
                     use->node()->toDefinition()->block()->id() < id());
    }
#endif
}

static inline MDefinition *
FollowCopy(MDefinition *def)
{
    MDefinition *ret = def->isCopy() ? def->getOperand(0) : def;
    JS_ASSERT(!ret->isCopy());
    return ret;
}

bool
MBasicBlock::setBackedge(MBasicBlock *pred)
{
    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(lastIns_);
    JS_ASSERT(pred->lastIns_);
    JS_ASSERT(pred->stackPosition_ == stackPosition_);
    JS_ASSERT(entrySnapshot()->stackDepth() == stackPosition_);

    // We must be a pending loop header
    JS_ASSERT(kind_ == PENDING_LOOP_HEADER);

    // Place minimal phi nodes by comparing the set of defintions at loop entry
    // with the loop exit. For each mismatching slot, we create a phi node, and
    // rewrite all uses of the entry definition to use the phi node instead.
    //
    // This algorithm would break in the face of copies, so we take care to
    // give every assignment its own unique SSA name. See
    // MBasicBlock::setVariable for more information.
    for (uint32 i = 0; i < stackPosition_; i++) {
        MDefinition *entryDef = entrySnapshot()->getOperand(i);
        MDefinition *exitDef = pred->slots_[i].def;

        // If the entry def is a phi, it must not be a phi owned by this block,
        // since a loop header can only have two entry points, and one already
        // exists. This assert would trigger if we accidentally copy propagated,
        // and two locals had the same def.
        JS_ASSERT_IF(entryDef->isPhi(), entryDef->block() != this);

        // So long as we make sure that local variables are distinct SSA names,
        // and generate a unique copy for each assignment until copy
        // propagation occurs after SSA building is complete, then we need not
        // insert phis if the entry definition is just a copy of the exit
        // definition.
        //
        // If at any point, we perform an operation on a local variable that is
        // NOT just a copy (say, we perform an add), then the exit definition
        // will differ and we will insert the phi as necessary.
        //
        // Essentially, we are capturing the fact that copy propagation WILL
        // occur, so that there will be no modifying operations in the loop,
        // and we will not need to insert a phi to merge a back edge. So,
        // inserting a phi is not necessary.
        //
        // Consider:
        //   i = 1          ||   0: const(#1)    ||   0: const(#1)
        //                  ||   1: copy(0)      ||   1: copy(0)
        //   t = i          \|   do {            \|   do {
        //   do {            ->    3: copy(0)     ->    2: phi(0, 3)
        //      i = t       /|   } ...           /|     3: copy(2)
        //   } ...          ||                   ||   } ...
        //                  ||                   ||
        //
        // Note that the inserted phi is unecessary. After copy propagation
        // occurs, we will have:
        // 0: const(#1)     ||   0: const(#1)
        // 1: copy(0)       ||   [eliminated copy]
        // do {             \|   do {
        //   2: phi(0, 3)    ->     2: phi(0, 2)
        //   3: copy(2)     /|   }
        // } ...            ||   4 : op(2, ...)
        // 4 : op(3, ...)   ||
        //
        // So, the phi joins two definitions which are actually the same, and
        // there was no reason to insert it to begin with.

        // If the entry definition and exit definition do not differ, then
        // no phi placement is necessary.
        if (FollowCopy(entryDef) == FollowCopy(exitDef))
            continue;

        // Create a new phi. Do not add inputs yet, as we don't want to
        // accidentally rewrite the phi's operands.
        MPhi *phi = MPhi::New(i);
        addPhi(phi);

        for (MUseIterator use(entryDef->usesBegin()); use != entryDef->usesEnd(); ) {
            JS_ASSERT(use->node()->getOperand(use->index()) == entryDef);

            // Uses are initially sorted, with the head of the list being the
            // most recently inserted. This ordering is maintained while
            // placing phis.
            if (use->node()->block()->id() < id()) {
                assertUsesAreNotWithin(use, entryDef->usesEnd());
                break;
            }

            // Replace the instruction's use with the phi. Note that |prev|
            // does not change, and is really NULL since we always remove
            // from the head of the list,
            use = use->node()->replaceOperand(use, phi);
        }

#ifdef DEBUG
        // Assert that no slot after this one has the same entryDef. This would
        // imply that the SSA building process has accidentally allowed the
        // same SSA name to occupy two slots. Note, this is actually allowed
        // when the expression stack is non-empty, for example, (a + a) will
        // push two stack slots with the same SSA name as |a|. However, at loop
        // edges, the expression stack is empty, and thus we expect there to be
        // no copies.
        for (uint32 j = i + 1; j < stackPosition_; j++)
            JS_ASSERT(slots_[j].def != entryDef);
#endif

        if (!phi->addInput(entryDef) || !phi->addInput(exitDef))
            return false;

        setSlot(i, phi);
    }

    // We are now a loop header proper
    kind_ = LOOP_HEADER;

    return predecessors_.append(pred);
}

size_t
MBasicBlock::numSuccessors() const
{
    JS_ASSERT(lastIns());
    return lastIns()->numSuccessors();
}

MBasicBlock *
MBasicBlock::getSuccessor(size_t index) const
{
    JS_ASSERT(lastIns());
    return lastIns()->getSuccessor(index);
}

void
MBasicBlock::replaceSuccessor(size_t pos, MBasicBlock *split)
{
    JS_ASSERT(lastIns());
    lastIns()->replaceSuccessor(pos, split);

    // Note, successors-with-phis is not yet set.
    JS_ASSERT(!successorWithPhis_);
}

void
MBasicBlock::replacePredecessor(MBasicBlock *old, MBasicBlock *split)
{
    for (size_t i = 0; i < numPredecessors(); i++) {
        if (getPredecessor(i) == old) {
            predecessors_[i] = split;

#ifdef DEBUG
            // The same block should not appear twice in the predecessor list.
            for (size_t j = i; j < numPredecessors(); j++)
                JS_ASSERT(predecessors_[j] != old);
#endif

            return;
        }
    }
    JS_NOT_REACHED("predecessor was not found");
}

void
MBasicBlock::inheritPhis(MBasicBlock *header)
{
    for (MPhiIterator iter = header->phisBegin(); iter != header->phisEnd(); iter++) {
        MPhi *phi = *iter;
        JS_ASSERT(phi->numOperands() == 2);

        // The entry definition is always the leftmost input to the phi.
        MDefinition *entryDef = phi->getOperand(0);
        MDefinition *exitDef = getSlot(phi->slot());

        if (entryDef != exitDef)
            continue;

        // If the entryDef is the same as exitDef, then we must propagate the
        // phi down to this successor. This chance was missed as part of
        // setBackedge() because exits are not captured in snapshots.
        setSlot(phi->slot(), phi);
    }
}

