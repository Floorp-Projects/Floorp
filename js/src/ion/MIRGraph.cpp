/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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
    cancelBuild_(0),
    maxAsmJSStackArgBytes_(0),
    performsAsmJSCall_(false)
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
    numBlocks_++;
}

void
MIRGraph::insertBlockAfter(MBasicBlock *at, MBasicBlock *block)
{
    block->setId(blockIdGen_++);
    blocks_.insertAfter(at, block);
    numBlocks_++;
}

void
MIRGraph::removeBlocksAfter(MBasicBlock *start)
{
    MBasicBlockIterator iter(begin());
    iter++;
    while (iter != end()) {
        MBasicBlock *block = *iter;
        iter++;

        if (block->id() <= start->id())
            continue;

        if (block == osrBlock_)
            osrBlock_ = NULL;
        block->discardAllInstructions();
        block->discardAllPhis();
        block->markAsDead();
        removeBlock(block);
    }
}

void
MIRGraph::unmarkBlocks() {
    for (MBasicBlockIterator i(blocks_.begin()); i != blocks_.end(); i++)
        i->unmark();
}

MDefinition *
MIRGraph::parSlice() {
    // Search the entry block to find a par slice instruction.  If we do not
    // find one, add one after the Start instruction.
    //
    // Note: the original design used a field in MIRGraph to cache the
    // parSlice rather than searching for it again.  However, this
    // could become out of date due to DCE.  Given that we do not
    // generally have to search very far to find the par slice
    // instruction if it exists, and that we don't look for it that
    // often, I opted to simply eliminate the cache and search anew
    // each time, so that it is that much easier to keep the IR
    // coherent. - nmatsakis

    MBasicBlock *entry = entryBlock();
    JS_ASSERT(entry->info().executionMode() == ParallelExecution);

    MInstruction *start = NULL;
    for (MInstructionIterator ins(entry->begin()); ins != entry->end(); ins++) {
        if (ins->isParSlice())
            return *ins;
        else if (ins->isStart())
            start = *ins;
    }
    JS_ASSERT(start);

    MParSlice *parSlice = new MParSlice();
    entry->insertAfter(start, parSlice);
    return parSlice;
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

MBasicBlock *
MBasicBlock::NewParBailout(MIRGraph &graph, CompileInfo &info,
                           MBasicBlock *pred, jsbytecode *entryPc,
                           MResumePoint *resumePoint)
{
    MBasicBlock *block = new MBasicBlock(graph, info, entryPc, NORMAL);

    resumePoint->block_ = block;
    block->entryResumePoint_ = resumePoint;

    if (!block->init())
        return NULL;

    if (!block->addPredecessorWithoutPhis(pred))
        return NULL;

    block->end(new MParBailout());
    return block;
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
    return slots_.init(info_.nslots());
}

bool
MBasicBlock::increaseSlots(size_t num)
{
    return slots_.growBy(num);
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
    } else if (pc()) {
        uint32_t stackDepth = info().script()->analysis()->getCode(pc()).stackDepth;
        stackPosition_ = info().firstStackSlot() + stackDepth;
        JS_ASSERT(stackPosition_ >= popped);
        stackPosition_ -= popped;
    } else {
        stackPosition_ = info().firstStackSlot();
    }

    JS_ASSERT(info_.nslots() >= stackPosition_);
    JS_ASSERT(!entryResumePoint_);

    if (pc()) {
        // Propagate the caller resume point from the inherited block.
        MResumePoint *callerResumePoint = pred ? pred->callerResumePoint() : NULL;

        // Create a resume point using our initial stack state.
        entryResumePoint_ = new MResumePoint(this, pc(), callerResumePoint, MResumePoint::ResumeAt);
        if (!entryResumePoint_->init())
            return false;
    }

    if (pred) {
        if (!predecessors_.append(pred))
            return false;

        if (kind_ == PENDING_LOOP_HEADER) {
            for (size_t i = 0; i < stackDepth(); i++) {
                MPhi *phi = MPhi::New(i);
                if (!phi->addInputSlow(pred->getSlot(i)))
                    return false;
                addPhi(phi);
                setSlot(i, phi);
                if (entryResumePoint())
                    entryResumePoint()->setOperand(i, phi);
            }
        } else if (entryResumePoint()) {
            for (size_t i = 0; i < stackDepth(); i++)
                entryResumePoint()->setOperand(i, getSlot(i));
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
    if (entryResumePoint())
        entryResumePoint()->setOperand(slot, ins);
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
        if (i == info().scopeChainSlot()) {
            if (def->isOsrScopeChain())
                def->toOsrScopeChain()->setResumePoint(res);
        } else if (info().hasArguments() && i == info().argsObjSlot()) {
            JS_ASSERT(def->isConstant() && def->toConstant()->value() == UndefinedValue());
        } else {
            def->toOsrValue()->setResumePoint(res);
        }
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
    JS_ASSERT(stackPosition_ < nslots());
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

void
MBasicBlock::popn(uint32_t n)
{
    JS_ASSERT(stackPosition_ - n >= info_.firstStackSlot());
    JS_ASSERT(stackPosition_ >= stackPosition_ - n);
    stackPosition_ -= n;
}

MDefinition *
MBasicBlock::scopeChain()
{
    return getSlot(info().scopeChainSlot());
}

MDefinition *
MBasicBlock::argumentsObject()
{
    return getSlot(info().argsObjSlot());
}

void
MBasicBlock::setScopeChain(MDefinition *scopeObj)
{
    setSlot(info().scopeChainSlot(), scopeObj);
}

void
MBasicBlock::setArgumentsObject(MDefinition *argsObj)
{
    setSlot(info().argsObjSlot(), argsObj);
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

static inline void
AssertSafelyDiscardable(MDefinition *def)
{
#ifdef DEBUG
    // Instructions captured by resume points cannot be safely discarded, since
    // they are necessary for interpreter frame reconstruction in case of bailout.
    JS_ASSERT(def->useCount() == 0);
#endif
}

void
MBasicBlock::discard(MInstruction *ins)
{
    AssertSafelyDiscardable(ins);
    for (size_t i = 0; i < ins->numOperands(); i++)
        ins->discardOperand(i);

    instructions_.remove(ins);
}

MInstructionIterator
MBasicBlock::discardAt(MInstructionIterator &iter)
{
    AssertSafelyDiscardable(*iter);
    for (size_t i = 0; i < iter->numOperands(); i++)
        iter->discardOperand(i);

    return instructions_.removeAt(iter);
}

MInstructionReverseIterator
MBasicBlock::discardAt(MInstructionReverseIterator &iter)
{
    AssertSafelyDiscardable(*iter);
    for (size_t i = 0; i < iter->numOperands(); i++)
        iter->discardOperand(i);

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
MBasicBlock::discardAllInstructions()
{
    for (MInstructionIterator iter = begin(); iter != end(); ) {
        for (size_t i = 0; i < iter->numOperands(); i++)
            iter->discardOperand(i);
        iter = instructions_.removeAt(iter);
    }
    lastIns_ = NULL;
}

void
MBasicBlock::discardAllPhis()
{
    for (MPhiIterator iter = phisBegin(); iter != phisEnd(); ) {
        MPhi *phi = *iter;
        for (size_t i = 0; i < phi->numOperands(); i++)
            phi->discardOperand(i);
        iter = phis_.removeAt(iter);
    }

    for (MBasicBlock **pred = predecessors_.begin(); pred != predecessors_.end(); pred++)
        (*pred)->setSuccessorWithPhis(NULL, 0);
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
        at->discardOperand(i);

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
            // If the current instruction is a phi, and it was created in this
            // basic block, then we have already placed this phi and should
            // instead append to its operands.
            if (mine->isPhi() && mine->block() == this) {
                JS_ASSERT(predecessors_.length());
                if (!mine->toPhi()->addInputSlow(other))
                    return false;
            } else {
                // Otherwise, create a new phi node.
                MPhi *phi = MPhi::New(i);
                addPhi(phi);

                // Prime the phi for each predecessor, so input(x) comes from
                // predecessor(x).
                if (!phi->reserveLength(predecessors_.length() + 1))
                    return false;

                for (size_t j = 0; j < predecessors_.length(); j++) {
                    JS_ASSERT(predecessors_[j]->getSlot(i) == mine);
                    phi->addInput(mine);
                }
                phi->addInput(other);

                setSlot(i, phi);
                if (entryResumePoint())
                    entryResumePoint()->replaceOperand(i, phi);
            }
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
        JS_ASSERT_IF(use->consumer()->isDefinition(),
                     use->consumer()->toDefinition()->block()->id() < id());
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

AbortReason
MBasicBlock::setBackedge(MBasicBlock *pred)
{
    // Predecessors must be finished, and at the correct stack depth.
    JS_ASSERT(lastIns_);
    JS_ASSERT(pred->lastIns_);
    JS_ASSERT_IF(entryResumePoint(), pred->stackDepth() == entryResumePoint()->stackDepth());

    // We must be a pending loop header
    JS_ASSERT(kind_ == PENDING_LOOP_HEADER);

    bool hadTypeChange = false;

    // Add exit definitions to each corresponding phi at the entry.
    for (MPhiIterator phi = phisBegin(); phi != phisEnd(); phi++) {
        MPhi *entryDef = *phi;
        MDefinition *exitDef = pred->slots_[entryDef->slot()];

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

        bool typeChange = false;

        if (!entryDef->addInputSlow(exitDef, &typeChange))
            return AbortReason_Alloc;

        hadTypeChange |= typeChange;

        JS_ASSERT(entryDef->slot() < pred->stackDepth());
        setSlot(entryDef->slot(), entryDef);
    }

    if (hadTypeChange) {
        for (MPhiIterator phi = phisBegin(); phi != phisEnd(); phi++)
            phi->removeOperand(phi->numOperands() - 1);
        return AbortReason_Disable;
    }

    // We are now a loop header proper
    kind_ = LOOP_HEADER;

    if (!predecessors_.append(pred))
        return AbortReason_Alloc;

    return AbortReason_NoAbort;
}

void
MBasicBlock::clearLoopHeader()
{
    JS_ASSERT(isLoopHeader());
    kind_ = NORMAL;
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

size_t
MBasicBlock::getSuccessorIndex(MBasicBlock *block) const
{
    JS_ASSERT(lastIns());
    for (size_t i = 0; i < numSuccessors(); i++) {
        if (getSuccessor(i) == block)
            return i;
    }
    JS_NOT_REACHED("Invalid successor");
}

void
MBasicBlock::replaceSuccessor(size_t pos, MBasicBlock *split)
{
    JS_ASSERT(lastIns());

    // Note, during split-critical-edges, successors-with-phis is not yet set.
    // During PAA, this case is handled before we enter.
    JS_ASSERT_IF(successorWithPhis_, successorWithPhis_ != getSuccessor(pos));

    lastIns()->replaceSuccessor(pos, split);
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
MBasicBlock::clearDominatorInfo()
{
    setImmediateDominator(NULL);
    immediatelyDominated_.clear();
    numDominated_ = 0;
}

void
MBasicBlock::removePredecessor(MBasicBlock *pred)
{
    JS_ASSERT(numPredecessors() >= 2);

    for (size_t i = 0; i < numPredecessors(); i++) {
        if (getPredecessor(i) != pred)
            continue;

        // Adjust phis.  Note that this can leave redundant phis
        // behind.
        if (!phisEmpty()) {
            JS_ASSERT(pred->successorWithPhis());
            JS_ASSERT(pred->positionInPhiSuccessor() == i);
            for (MPhiIterator iter = phisBegin(); iter != phisEnd(); iter++)
                iter->removeOperand(i);
            for (size_t j = i+1; j < numPredecessors(); j++)
                getPredecessor(j)->setSuccessorWithPhis(this, j - 1);
        }

        // Remove from pred list.
        MBasicBlock **ptr = predecessors_.begin() + i;
        predecessors_.erase(ptr);
        return;
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
MBasicBlock::specializePhis()
{
    for (MPhiIterator iter = phisBegin(); iter != phisEnd(); iter++) {
        MPhi *phi = *iter;
        phi->specializeType();
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
