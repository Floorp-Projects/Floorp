/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Ion.h"
#include "IonSpewer.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "IonBuilder.h"
#include "frontend/BytecodeEmitter.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::ion;

MIRGenerator::MIRGenerator(JSCompartment *compartment,
                           TempAllocator *temp, MIRGraph *graph, CompileInfo *info)
  : compartment(compartment),
    info_(info),
    temp_(temp),
    graph_(graph),
    error_(false),
    cancelBuild_(0)
{ }

bool
MIRGenerator::abortFmt(const char *message, va_list ap)
{
    IonSpewVA(IonSpew_Abort, message, ap);
    error_ = true;
    return false;
}

bool
MIRGenerator::abort(const char *message, ...)
{
    va_list ap;
    va_start(ap, message);
    abortFmt(message, ap);
    va_end(ap);
    return false;
}

void
MIRGraph::addBlock(MBasicBlock *block)
{
    JS_ASSERT(block);
    block->setId(blockIdGen_++);
    blocks_.pushBack(block);
#ifdef DEBUG
    numBlocks_++;
#endif
}

void
MIRGraph::insertBlockAfter(MBasicBlock *at, MBasicBlock *block)
{
    block->setId(blockIdGen_++);
    blocks_.insertAfter(at, block);
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
MBasicBlock::New(MIRGraph &graph, CompileInfo &info,
                 MBasicBlock *pred, jsbytecode *entryPc, Kind kind)
{
    MBasicBlock *block = new MBasicBlock(graph, info, entryPc, kind);
    if (!block->init())
        return NULL;

    if (!block->inherit(pred, 0))
        return NULL;

    return block;
}

MBasicBlock *
MBasicBlock::NewPopN(MIRGraph &graph, CompileInfo &info,
                     MBasicBlock *pred, jsbytecode *entryPc, Kind kind, uint32_t popped)
{
    MBasicBlock *block = new MBasicBlock(graph, info, entryPc, kind);
    if (!block->init())
        return NULL;

    if (!block->inherit(pred, popped))
        return NULL;

    return block;
}

MBasicBlock *
MBasicBlock::NewWithResumePoint(MIRGraph &graph, CompileInfo &info,
                                MBasicBlock *pred, jsbytecode *entryPc,
                                MResumePoint *resumePoint)
{
    MBasicBlock *block = new MBasicBlock(graph, info, entryPc, NORMAL);

    resumePoint->block_ = block;
    block->entryResumePoint_ = resumePoint;

    if (!block->init())
        return NULL;

    if (!block->inheritResumePoint(pred))
        return NULL;

    return block;
}

MBasicBlock *
MBasicBlock::NewPendingLoopHeader(MIRGraph &graph, CompileInfo &info,
                                  MBasicBlock *pred, jsbytecode *entryPc)
{
    return MBasicBlock::New(graph, info, pred, entryPc, PENDING_LOOP_HEADER);
}

MBasicBlock *
MBasicBlock::NewSplitEdge(MIRGraph &graph, CompileInfo &info, MBasicBlock *pred)
{
    return MBasicBlock::New(graph, info, pred, pred->pc(), SPLIT_EDGE);
}

MBasicBlock::MBasicBlock(MIRGraph &graph, CompileInfo &info, jsbytecode *pc, Kind kind)
    : earlyAbort_(false),
    graph_(graph),
    info_(info),
    stackPosition_(info_.firstStackSlot()),
    lastIns_(NULL),
    pc_(pc),
    lir_(NULL),
    start_(NULL),
    entryResumePoint_(NULL),
    successorWithPhis_(NULL),
    positionInPhiSuccessor_(0),
    kind_(kind),
    loopDepth_(0),
    mark_(false),
    immediateDominator_(NULL),
    numDominated_(0),
    loopHeader_(NULL),
    trackedPc_(pc)
{
}

bool
MBasicBlock::init()
{
    if (!slots_.init(info_.nslots()))
        return false;
    return true;
}

void
MBasicBlock::copySlots(MBasicBlock *from)
{
    JS_ASSERT(stackPosition_ <= from->stackPosition_);

    for (uint32_t i = 0; i < stackPosition_; i++)
        slots_[i] = from->slots_[i];
}

bool
MBasicBlock::inherit(MBasicBlock *pred, uint32_t popped)
{
    if (pred) {
        stackPosition_ = pred->stackPosition_;
        JS_ASSERT(stackPosition_ >= popped);
        stackPosition_ -= popped;
        if (kind_ != PENDING_LOOP_HEADER)
            copySlots(pred);
    } else {
        uint32_t stackDepth = info().script()->analysis()->getCode(pc()).stackDepth;
        stackPosition_ = info().firstStackSlot() + stackDepth;
        JS_ASSERT(stackPosition_ >= popped);
        stackPosition_ -= popped;
    }

    JS_ASSERT(info_.nslots() >= stackPosition_);
    JS_ASSERT(!entryResumePoint_);

    // Propagate the caller resume point from the inherited block.
    MResumePoint *callerResumePoint = pred ? pred->callerResumePoint() : NULL;

    // Create a resume point using our initial stack state.
    entryResumePoint_ = new MResumePoint(this, pc(), callerResumePoint, MResumePoint::ResumeAt);
    if (!entryResumePoint_->init(this))
        return false;

    if (pred) {
        if (!predecessors_.append(pred))
            return false;

        if (kind_ == PENDING_LOOP_HEADER) {
            for (size_t i = 0; i < stackDepth(); i++) {
                MPhi *phi = MPhi::New(i);
                if (!phi->addInput(pred->getSlot(i)))
                    return false;
                addPhi(phi);
                setSlot(i, phi);
                entryResumePoint()->initOperand(i, phi);
            }
        } else {
            for (size_t i = 0; i < stackDepth(); i++)
                entryResumePoint()->initOperand(i, getSlot(i));
        }
    }

    return true;
}

bool
MBasicBlock::inheritResumePoint(MBasicBlock *pred)
{
    // Copy slots from the resume point.
    stackPosition_ = entryResumePoint_->numOperands();
    for (uint32_t i = 0; i < stackPosition_; i++)
        slots_[i] = entryResumePoint_->getOperand(i);

    JS_ASSERT(info_.nslots() >= stackPosition_);
    JS_ASSERT(kind_ != PENDING_LOOP_HEADER);
    JS_ASSERT(pred != NULL);

    if (!predecessors_.append(pred))
        return false;

    return true;
}

void
MBasicBlock::inheritSlots(MBasicBlock *parent)
{
    stackPosition_ = parent->stackPosition_;
    copySlots(parent);
}

bool
MBasicBlock::initEntrySlots()
{
    // Create a resume point using our initial stack state.
    entryResumePoint_ = MResumePoint::New(this, pc(), callerResumePoint(), MResumePoint::ResumeAt);
    if (!entryResumePoint_)
        return false;
    return true;
}

MDefinition *
MBasicBlock::getSlot(uint32_t index)
{
    JS_ASSERT(index < stackPosition_);
    return slots_[index];
}

void
MBasicBlock::initSlot(uint32_t slot, MDefinition *ins)
{
    slots_[slot] = ins;
    entryResumePoint()->initOperand(slot, ins);
}

void
MBasicBlock::shimmySlots(int discardDepth)
{
    // Move all slots above the given depth down by one,
    // overwriting the MDefinition at discardDepth.

    JS_ASSERT(discardDepth < 0);
    JS_ASSERT(stackPosition_ + discardDepth >= info_.firstStackSlot());

    for (int i = discardDepth; i < -1; i++)
        slots_[stackPosition_ + i] = slots_[stackPosition_ + i + 1];

    --stackPosition_;
}

void
MBasicBlock::linkOsrValues(MStart *start)
{
    JS_ASSERT(start->startType() == MStart::StartType_Osr);

    MResumePoint *res = start->resumePoint();

    for (uint32_t i = 0; i < stackDepth(); i++) {
        MDefinition *def = slots_[i];
        if (i == info().scopeChainSlot())
            def->toOsrScopeChain()->setResumePoint(res);
        else
            def->toOsrValue()->setResumePoint(res);
    }
}

void
MBasicBlock::setSlot(uint32_t slot, MDefinition *ins)
{
    slots_[slot] = ins;
}

void
MBasicBlock::setVariable(uint32_t index)
{
    JS_ASSERT(stackPosition_ > info_.firstStackSlot());
    setSlot(index, slots_[stackPosition_ - 1]);
}

void
MBasicBlock::setArg(uint32_t arg)
{
    setVariable(info_.argSlot(arg));
}

void
MBasicBlock::setLocal(uint32_t local)
{
    setVariable(info_.localSlot(local));
}

void
MBasicBlock::setSlot(uint32_t slot)
{
    setVariable(slot);
}

void
MBasicBlock::rewriteSlot(uint32_t slot, MDefinition *ins)
{
    setSlot(slot, ins);
}

void
MBasicBlock::rewriteAtDepth(int32_t depth, MDefinition *ins)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(stackPosition_ + depth >= info_.firstStackSlot());
    rewriteSlot(stackPosition_ + depth, ins);
}

void
MBasicBlock::push(MDefinition *ins)
{
    JS_ASSERT(stackPosition_ < info_.nslots());
    slots_[stackPosition_++] = ins;
}

void
MBasicBlock::pushVariable(uint32_t slot)
{
    push(slots_[slot]);
}

void
MBasicBlock::pushArg(uint32_t arg)
{
    pushVariable(info_.argSlot(arg));
}

void
MBasicBlock::pushLocal(uint32_t local)
{
    pushVariable(info_.localSlot(local));
}

void
MBasicBlock::pushSlot(uint32_t slot)
{
    pushVariable(slot);
}

MDefinition *
MBasicBlock::pop()
{
    JS_ASSERT(stackPosition_ > info_.firstStackSlot());
    return slots_[--stackPosition_];
}

MDefinition *
MBasicBlock::scopeChain()
{
    return getSlot(info().scopeChainSlot());
}

void
MBasicBlock::setScopeChain(MDefinition *scopeObj)
{
    setSlot(info().scopeChainSlot(), scopeObj);
}

void
MBasicBlock::pick(int32_t depth)
{
    // pick take an element and move it to the top.
    // pick(-2):
    //   A B C D E
    //   A B D C E [ swapAt(-2) ]
    //   A B D E C [ swapAt(-1) ]
    for (; depth < 0; depth++)
        swapAt(depth);
}

void
MBasicBlock::swapAt(int32_t depth)
{
    uint32_t lhsDepth = stackPosition_ + depth - 1;
    uint32_t rhsDepth = stackPosition_ + depth;

    MDefinition *temp = slots_[lhsDepth];
    slots_[lhsDepth] = slots_[rhsDepth];
    slots_[rhsDepth] = temp;
}

MDefinition *
MBasicBlock::peek(int32_t depth)
{
    JS_ASSERT(depth < 0);
    JS_ASSERT(stackPosition_ + depth >= info_.firstStackSlot());
    return getSlot(stackPosition_ + depth);
}

void
MBasicBlock::discardLastIns()
{
    JS_ASSERT(lastIns_);
    discard(lastIns_);
    lastIns_ = NULL;
}

void
MBasicBlock::addFromElsewhere(MInstruction *ins)
{
    JS_ASSERT(ins->block() != this);

    // Remove |ins| from its containing block.
    ins->block()->instructions_.remove(ins);

    // Add it to this block.
    add(ins);
}

void
MBasicBlock::moveBefore(MInstruction *at, MInstruction *ins)
{
    // Remove |ins| from the current block.
    JS_ASSERT(ins->block() == this);
    instructions_.remove(ins);

    // Insert into new block, which may be distinct.
    // Uses and operands are untouched.
    at->block()->insertBefore(at, ins);
}

void
MBasicBlock::discard(MInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i++)
        ins->replaceOperand(i, NULL);

    instructions_.remove(ins);
}

MInstructionIterator
MBasicBlock::discardAt(MInstructionIterator &iter)
{
    for (size_t i = 0; i < iter->numOperands(); i++)
        iter->replaceOperand(i, NULL);

    return instructions_.removeAt(iter);
}

MInstructionReverseIterator
MBasicBlock::discardAt(MInstructionReverseIterator &iter)
{
    for (size_t i = 0; i < iter->numOperands(); i++)
        iter->replaceOperand(i, NULL);

    return instructions_.removeAt(iter);
}

MDefinitionIterator
MBasicBlock::discardDefAt(MDefinitionIterator &old)
{
    MDefinitionIterator iter(old);

    if (iter.atPhi())
        iter.phiIter_ = iter.block_->discardPhiAt(iter.phiIter_);
    else
        iter.iter_ = iter.block_->discardAt(iter.iter_);

    return iter;
}

void
MBasicBlock::insertBefore(MInstruction *at, MInstruction *ins)
{
    ins->setBlock(this);
    graph().allocDefinitionId(ins);
    instructions_.insertBefore(at, ins);
    ins->setTrackedPc(at->trackedPc());
}

void
MBasicBlock::insertAfter(MInstruction *at, MInstruction *ins)
{
    ins->setBlock(this);
    graph().allocDefinitionId(ins);
    instructions_.insertAfter(at, ins);
    ins->setTrackedPc(at->trackedPc());
}

void
MBasicBlock::add(MInstruction *ins)
{
    JS_ASSERT(!lastIns_);
    ins->setBlock(this);
    graph().allocDefinitionId(ins);
    instructions_.pushBack(ins);
    ins->setTrackedPc(trackedPc_);
}

void
MBasicBlock::end(MControlInstruction *ins)
{
    JS_ASSERT(!lastIns_); // Existing control instructions should be removed first.
    JS_ASSERT(ins);
    add(ins);
    lastIns_ = ins;
}

void
MBasicBlock::addPhi(MPhi *phi)
{
    phis_.pushBack(phi);
    phi->setBlock(this);
    graph().allocDefinitionId(phi);
}

MPhiIterator
MBasicBlock::discardPhiAt(MPhiIterator &at)
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
    return addPredecessorPopN(pred, 0);
}

bool
MBasicBlock::addPredecessorPopN(MBasicBlock *pred, uint32_t popped)
{
    JS_ASSERT(pred);
    JS_ASSERT(predecessors_.length() > 0);

    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(pred->lastIns_);
    JS_ASSERT(pred->stackPosition_ == stackPosition_ + popped);

    for (uint32_t i = 0; i < stackPosition_; i++) {
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
                entryResumePoint()->replaceOperand(i, phi);
            }

            if (!phi->addInput(other))
                return false;
        }
    }

    return predecessors_.append(pred);
}

bool
MBasicBlock::addPredecessorWithoutPhis(MBasicBlock *pred)
{
    // Predecessors must be finished.
    JS_ASSERT(pred && pred->lastIns_);
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

bool
MBasicBlock::dominates(MBasicBlock *other)
{
    uint32_t high = domIndex() + numDominated();
    uint32_t low  = domIndex();
    return other->domIndex() >= low && other->domIndex() <= high;
}

bool
MBasicBlock::setBackedge(MBasicBlock *pred)
{
    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(lastIns_);
    JS_ASSERT(pred->lastIns_);
    JS_ASSERT(pred->stackDepth() == entryResumePoint()->stackDepth());

    // We must be a pending loop header
    JS_ASSERT(kind_ == PENDING_LOOP_HEADER);

    // Add exit definitions to each corresponding phi at the entry.
    for (uint32_t i = 0; i < pred->stackDepth(); i++) {
        MPhi *entryDef = entryResumePoint()->getOperand(i)->toPhi();
        MDefinition *exitDef = pred->slots_[i];

        // Assert that we already placed phis for each slot.
        JS_ASSERT(entryDef->block() == this);

        if (entryDef == exitDef) {
            // If the exit def is the same as the entry def, make a redundant
            // phi. Since loop headers have exactly two incoming edges, we
            // know that that's just the first input.
            //
            // Note that we eliminate later rather than now, to avoid any
            // weirdness around pending continue edges which might still hold
            // onto phis.
            exitDef = entryDef->getOperand(0);
        }

        if (!entryDef->addInput(exitDef))
            return false;

        setSlot(i, entryDef);
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
        // setBackedge() because exits are not captured in resume points.
        setSlot(phi->slot(), phi);
    }
}

void
MBasicBlock::dumpStack(FILE *fp)
{
#ifdef DEBUG
    fprintf(fp, " %-3s %-16s %-6s %-10s\n", "#", "name", "copyOf", "first/next");
    fprintf(fp, "-------------------------------------------\n");
    for (uint32_t i = 0; i < stackPosition_; i++) {
        fprintf(fp, " %-3d", i);
        fprintf(fp, " %-16p\n", (void *)slots_[i]);
    }
#endif
}

MTest *
MBasicBlock::immediateDominatorBranch(BranchDirection *pdirection)
{
    *pdirection = FALSE_BRANCH;

    if (numPredecessors() != 1)
        return NULL;

    MBasicBlock *dom = immediateDominator();
    if (dom != getPredecessor(0))
        return NULL;

    // Look for a trailing MTest branching to this block.
    MInstruction *ins = dom->lastIns();
    if (ins->isTest()) {
        MTest *test = ins->toTest();

        JS_ASSERT(test->ifTrue() == this || test->ifFalse() == this);
        if (test->ifTrue() == this && test->ifFalse() == this)
            return NULL;

        *pdirection = (test->ifTrue() == this) ? TRUE_BRANCH : FALSE_BRANCH;
        return test;
    }

    return NULL;
}
