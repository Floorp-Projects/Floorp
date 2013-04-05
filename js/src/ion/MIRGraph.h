/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_mirgraph_h__
#define jsion_mirgraph_h__

// This file declares the data structures used to build a control-flow graph
// containing MIR.

#include "IonAllocPolicy.h"
#include "MIRGenerator.h"
#include "FixedList.h"

namespace js {
namespace ion {

class MBasicBlock;
class MIRGraph;
class MStart;

class MDefinitionIterator;

typedef InlineListIterator<MInstruction> MInstructionIterator;
typedef InlineListReverseIterator<MInstruction> MInstructionReverseIterator;
typedef InlineForwardListIterator<MPhi> MPhiIterator;

class LBlock;

class MBasicBlock : public TempObject, public InlineListNode<MBasicBlock>
{
  public:
    enum Kind {
        NORMAL,
        PENDING_LOOP_HEADER,
        LOOP_HEADER,
        SPLIT_EDGE
    };

  private:
    MBasicBlock(MIRGraph &graph, CompileInfo &info, jsbytecode *pc, Kind kind);
    bool init();
    void copySlots(MBasicBlock *from);
    bool inherit(MBasicBlock *pred, uint32_t popped);
    bool inheritResumePoint(MBasicBlock *pred);
    void assertUsesAreNotWithin(MUseIterator use, MUseIterator end);

    // Does this block do something that forces it to terminate early?
    bool earlyAbort_;

    // Pushes a copy of a local variable or argument.
    void pushVariable(uint32_t slot);

    // Sets a variable slot to the top of the stack, correctly creating copies
    // as needed.
    void setVariable(uint32_t slot);

  public:
    ///////////////////////////////////////////////////////
    ////////// BEGIN GRAPH BUILDING INSTRUCTIONS //////////
    ///////////////////////////////////////////////////////

    // Creates a new basic block for a MIR generator. If |pred| is not NULL,
    // its slots and stack depth are initialized from |pred|.
    static MBasicBlock *New(MIRGraph &graph, CompileInfo &info,
                            MBasicBlock *pred, jsbytecode *entryPc, Kind kind);
    static MBasicBlock *NewPopN(MIRGraph &graph, CompileInfo &info,
                                MBasicBlock *pred, jsbytecode *entryPc, Kind kind, uint32_t popn);
    static MBasicBlock *NewWithResumePoint(MIRGraph &graph, CompileInfo &info,
                                           MBasicBlock *pred, jsbytecode *entryPc,
                                           MResumePoint *resumePoint);
    static MBasicBlock *NewPendingLoopHeader(MIRGraph &graph, CompileInfo &info,
                                             MBasicBlock *pred, jsbytecode *entryPc);
    static MBasicBlock *NewSplitEdge(MIRGraph &graph, CompileInfo &info, MBasicBlock *pred);
    static MBasicBlock *NewParBailout(MIRGraph &graph, CompileInfo &info,
                                      MBasicBlock *pred, jsbytecode *entryPc);

    bool dominates(MBasicBlock *other);

    void setId(uint32_t id) {
        id_ = id;
    }
    void setEarlyAbort() {
        earlyAbort_ = true;
    }
    void clearEarlyAbort() {
        earlyAbort_ = false;
    }
    bool earlyAbort() {
        return earlyAbort_;
    }
    // Move the definition to the top of the stack.
    void pick(int32_t depth);

    // Exchange 2 stack slots at the defined depth
    void swapAt(int32_t depth);

    // Gets the instruction associated with various slot types.
    MDefinition *peek(int32_t depth);

    MDefinition *scopeChain();

    // Increase the number of slots available
    bool increaseSlots(size_t num);

    // Initializes a slot value; must not be called for normal stack
    // operations, as it will not create new SSA names for copies.
    void initSlot(uint32_t index, MDefinition *ins);

    // Discard the slot at the given depth, lowering all slots above.
    void shimmySlots(int discardDepth);

    // In an OSR block, set all MOsrValues to use the MResumePoint attached to
    // the MStart.
    void linkOsrValues(MStart *start);

    // Sets the instruction associated with various slot types. The
    // instruction must lie at the top of the stack.
    void setLocal(uint32_t local);
    void setArg(uint32_t arg);
    void setSlot(uint32_t slot);
    void setSlot(uint32_t slot, MDefinition *ins);

    // Rewrites a slot directly, bypassing the stack transition. This should
    // not be used under most circumstances.
    void rewriteSlot(uint32_t slot, MDefinition *ins);

    // Rewrites a slot based on its depth (same as argument to peek()).
    void rewriteAtDepth(int32_t depth, MDefinition *ins);

    // Tracks an instruction as being pushed onto the operand stack.
    void push(MDefinition *ins);
    void pushArg(uint32_t arg);
    void pushLocal(uint32_t local);
    void pushSlot(uint32_t slot);
    void setScopeChain(MDefinition *ins);

    // Returns the top of the stack, then decrements the virtual stack pointer.
    MDefinition *pop();
    void popn(uint32_t n);

    // Adds an instruction to this block's instruction list. |ins| may be NULL
    // to simplify OOM checking.
    void add(MInstruction *ins);

    // Marks the last instruction of the block; no further instructions
    // can be added.
    void end(MControlInstruction *ins);

    // Adds a phi instruction, but does not set successorWithPhis.
    void addPhi(MPhi *phi);

    // Adds a predecessor. Every predecessor must have the same exit stack
    // depth as the entry state to this block. Adding a predecessor
    // automatically creates phi nodes and rewrites uses as needed.
    bool addPredecessor(MBasicBlock *pred);
    bool addPredecessorPopN(MBasicBlock *pred, uint32_t popped);

    // Stranger utilities used for inlining.
    bool addPredecessorWithoutPhis(MBasicBlock *pred);
    void inheritSlots(MBasicBlock *parent);
    bool initEntrySlots();

    // Replaces an edge for a given block with a new block. This is
    // used for critical edge splitting and also for inserting
    // bailouts during ParallelArrayAnalysis.
    //
    // Note: If successorWithPhis is set, you must not be replacing it.
    void replacePredecessor(MBasicBlock *old, MBasicBlock *split);
    void replaceSuccessor(size_t pos, MBasicBlock *split);

    // Removes `pred` from the predecessor list.  `pred` should not be
    // the final predecessor. If this block defines phis, removes the
    // entry for `pred` and updates the indices of later entries.
    // This may introduce redundant phis if the new block has fewer
    // than two predecessors.
    void removePredecessor(MBasicBlock *pred);

    // Resets all the dominator info so that it can be recomputed.
    void clearDominatorInfo();

    // Sets a back edge. This places phi nodes and rewrites instructions within
    // the current loop as necessary.
    bool setBackedge(MBasicBlock *block);

    // Resets a LOOP_HEADER block to a NORMAL block.  This is needed when
    // optimizations remove the backedge.
    void clearLoopHeader();

    // Propagates phis placed in a loop header down to this successor block.
    void inheritPhis(MBasicBlock *header);

    void insertBefore(MInstruction *at, MInstruction *ins);
    void insertAfter(MInstruction *at, MInstruction *ins);

    // Add an instruction to this block, from elsewhere in the graph.
    void addFromElsewhere(MInstruction *ins);

    // Move an instruction. Movement may cross block boundaries.
    void moveBefore(MInstruction *at, MInstruction *ins);

    // Removes an instruction with the intention to discard it.
    void discard(MInstruction *ins);
    void discardLastIns();
    MInstructionIterator discardAt(MInstructionIterator &iter);
    MInstructionReverseIterator discardAt(MInstructionReverseIterator &iter);
    MDefinitionIterator discardDefAt(MDefinitionIterator &iter);

    // Discards a phi instruction and updates predecessor successorWithPhis.
    MPhiIterator discardPhiAt(MPhiIterator &at);

    ///////////////////////////////////////////////////////
    /////////// END GRAPH BUILDING INSTRUCTIONS ///////////
    ///////////////////////////////////////////////////////

    MIRGraph &graph() {
        return graph_;
    }
    CompileInfo &info() const {
        return info_;
    }
    jsbytecode *pc() const {
        return pc_;
    }
    uint32_t nslots() const {
        return slots_.length();
    }
    uint32_t id() const {
        return id_;
    }
    uint32_t numPredecessors() const {
        return predecessors_.length();
    }

    uint32_t domIndex() const {
        return domIndex_;
    }
    void setDomIndex(uint32_t d) {
        domIndex_ = d;
    }

    MBasicBlock *getPredecessor(uint32_t i) const {
        return predecessors_[i];
    }
    MControlInstruction *lastIns() const {
        return lastIns_;
    }
    MPhiIterator phisBegin() const {
        return phis_.begin();
    }
    MPhiIterator phisEnd() const {
        return phis_.end();
    }
    bool phisEmpty() const {
        return phis_.empty();
    }
    MInstructionIterator begin() {
        return instructions_.begin();
    }
    MInstructionIterator begin(MInstruction *at) {
        JS_ASSERT(at->block() == this);
        return instructions_.begin(at);
    }
    MInstructionIterator end() {
        return instructions_.end();
    }
    MInstructionReverseIterator rbegin() {
        return instructions_.rbegin();
    }
    MInstructionReverseIterator rbegin(MInstruction *at) {
        JS_ASSERT(at->block() == this);
        return instructions_.rbegin(at);
    }
    MInstructionReverseIterator rend() {
        return instructions_.rend();
    }
    bool isLoopHeader() const {
        return kind_ == LOOP_HEADER;
    }
    MBasicBlock *backedge() const {
        JS_ASSERT(isLoopHeader());
        JS_ASSERT(numPredecessors() == 1 || numPredecessors() == 2);
        return getPredecessor(numPredecessors() - 1);
    }
    MBasicBlock *loopHeaderOfBackedge() const {
        JS_ASSERT(isLoopBackedge());
        return getSuccessor(numSuccessors() - 1);
    }
    MBasicBlock *loopPredecessor() const {
        JS_ASSERT(isLoopHeader());
        return getPredecessor(0);
    }
    bool isLoopBackedge() const {
        if (!numSuccessors())
            return false;
        MBasicBlock *lastSuccessor = getSuccessor(numSuccessors() - 1);
        return lastSuccessor->isLoopHeader() && lastSuccessor->backedge() == this;
    }
    bool isSplitEdge() const {
        return kind_ == SPLIT_EDGE;
    }

    uint32_t stackDepth() const {
        return stackPosition_;
    }
    void setStackDepth(uint32_t depth) {
        stackPosition_ = depth;
    }
    bool isMarked() const {
        return mark_;
    }
    void mark() {
        mark_ = true;
    }
    void unmark() {
        mark_ = false;
    }
    void makeStart(MStart *start) {
        add(start);
        start_ = start;
    }
    MStart *start() const {
        return start_;
    }

    MBasicBlock *immediateDominator() const {
        return immediateDominator_;
    }

    void setImmediateDominator(MBasicBlock *dom) {
        immediateDominator_ = dom;
    }

    MTest *immediateDominatorBranch(BranchDirection *pdirection);

    size_t numImmediatelyDominatedBlocks() const {
        return immediatelyDominated_.length();
    }

    MBasicBlock *getImmediatelyDominatedBlock(size_t i) const {
        return immediatelyDominated_[i];
    }

    size_t numDominated() const {
        return numDominated_;
    }

    void addNumDominated(size_t n) {
        numDominated_ += n;
    }

    bool addImmediatelyDominatedBlock(MBasicBlock *child);

    // This function retrieves the internal instruction associated with a
    // slot, and should not be used for normal stack operations. It is an
    // internal helper that is also used to enhance spew.
    MDefinition *getSlot(uint32_t index);

    MResumePoint *entryResumePoint() const {
        return entryResumePoint_;
    }
    MResumePoint *callerResumePoint() {
        return entryResumePoint()->caller();
    }
    void setCallerResumePoint(MResumePoint *caller) {
        entryResumePoint()->setCaller(caller);
    }
    size_t numEntrySlots() const {
        return entryResumePoint()->numOperands();
    }
    MDefinition *getEntrySlot(size_t i) const {
        JS_ASSERT(i < numEntrySlots());
        return entryResumePoint()->getOperand(i);
    }

    LBlock *lir() const {
        return lir_;
    }
    void assignLir(LBlock *lir) {
        JS_ASSERT(!lir_);
        lir_ = lir;
    }

    MBasicBlock *successorWithPhis() const {
        return successorWithPhis_;
    }
    uint32_t positionInPhiSuccessor() const {
        return positionInPhiSuccessor_;
    }
    void setSuccessorWithPhis(MBasicBlock *successor, uint32_t id) {
        successorWithPhis_ = successor;
        positionInPhiSuccessor_ = id;
    }
    size_t numSuccessors() const;
    MBasicBlock *getSuccessor(size_t index) const;
    size_t getSuccessorIndex(MBasicBlock *) const;

    // Specifies the closest loop header dominating this block.
    void setLoopHeader(MBasicBlock *loop) {
        JS_ASSERT(loop->isLoopHeader());
        loopHeader_ = loop;
    }
    MBasicBlock *loopHeader() const {
        return loopHeader_;
    }

    void setLoopDepth(uint32_t loopDepth) {
        loopDepth_ = loopDepth;
    }
    uint32_t loopDepth() const {
        return loopDepth_;
    }

    bool strict() const {
        return info_.script()->strict;
    }

    void dumpStack(FILE *fp);

    // Track bailouts by storing the current pc in MIR instruction added at this
    // cycle. This is also used for tracking calls when profiling.
    void updateTrackedPc(jsbytecode *pc) {
        trackedPc_ = pc;
    }

    jsbytecode *trackedPc() {
        return trackedPc_;
    }

  private:
    MIRGraph &graph_;
    CompileInfo &info_; // Each block originates from a particular script.
    InlineList<MInstruction> instructions_;
    Vector<MBasicBlock *, 1, IonAllocPolicy> predecessors_;
    InlineForwardList<MPhi> phis_;
    FixedList<MDefinition *> slots_;
    uint32_t stackPosition_;
    MControlInstruction *lastIns_;
    jsbytecode *pc_;
    uint32_t id_;
    uint32_t domIndex_; // Index in the dominator tree.
    LBlock *lir_;
    MStart *start_;
    MResumePoint *entryResumePoint_;
    MBasicBlock *successorWithPhis_;
    uint32_t positionInPhiSuccessor_;
    Kind kind_;
    uint32_t loopDepth_;

    // Utility mark for traversal algorithms.
    bool mark_;

    Vector<MBasicBlock *, 1, IonAllocPolicy> immediatelyDominated_;
    MBasicBlock *immediateDominator_;
    size_t numDominated_;
    MBasicBlock *loopHeader_;

    jsbytecode *trackedPc_;
};

typedef InlineListIterator<MBasicBlock> MBasicBlockIterator;
typedef InlineListIterator<MBasicBlock> ReversePostorderIterator;
typedef InlineListReverseIterator<MBasicBlock> PostorderIterator;

typedef Vector<MBasicBlock *, 1, IonAllocPolicy> MIRGraphExits;

class MIRGraph
{
    InlineList<MBasicBlock> blocks_;
    TempAllocator *alloc_;
    MIRGraphExits *exitAccumulator_;
    uint32_t blockIdGen_;
    uint32_t idGen_;
    MBasicBlock *osrBlock_;
    MStart *osrStart_;

    // List of compiled/inlined scripts.
    Vector<RawScript, 4, IonAllocPolicy> scripts_;

    size_t numBlocks_;

#ifdef DEBUG
    // Is the graph in Reverse Post Order
    bool inRPO_;
#endif

  public:
    MIRGraph(TempAllocator *alloc)
      : alloc_(alloc),
        exitAccumulator_(NULL),
        blockIdGen_(0),
        idGen_(0),
        osrBlock_(NULL),
        osrStart_(NULL),
        numBlocks_(0)
#ifdef DEBUG
        , inRPO_(false)
#endif
    { }

    template <typename T>
    T * allocate(size_t count = 1) {
        return reinterpret_cast<T *>(alloc_->allocate(sizeof(T) * count));
    }

    void addBlock(MBasicBlock *block);
    void insertBlockAfter(MBasicBlock *at, MBasicBlock *block);

    void unmarkBlocks();

    void setExitAccumulator(MIRGraphExits *accum) {
        exitAccumulator_ = accum;
    }
    MIRGraphExits *exitAccumulator() const {
        return exitAccumulator_;
    }

    bool addExit(MBasicBlock *exitBlock) {
        if (!exitAccumulator_)
            return true;

        return exitAccumulator_->append(exitBlock);
    }

    MBasicBlock *entryBlock() {
        return *blocks_.begin();
    }

    void clearBlockList() {
        blocks_.clear();
        blockIdGen_ = 0;
        numBlocks_ = 0;
    }
    void resetInstructionNumber() {
        idGen_ = 0;
    }
    MBasicBlockIterator begin() {
        return blocks_.begin();
    }
    MBasicBlockIterator begin(MBasicBlock *at) {
        return blocks_.begin(at);
    }
    MBasicBlockIterator end() {
        return blocks_.end();
    }
    PostorderIterator poBegin() {
        JS_ASSERT(inRPO_);
        return blocks_.rbegin();
    }
    PostorderIterator poEnd() {
        JS_ASSERT(inRPO_);
        return blocks_.rend();
    }
    ReversePostorderIterator rpoBegin() {
        JS_ASSERT(inRPO_);
        return blocks_.begin();
    }
    ReversePostorderIterator rpoEnd() {
        JS_ASSERT(inRPO_);
        return blocks_.end();
    }
    void removeBlock(MBasicBlock *block) {
        blocks_.remove(block);
        numBlocks_--;
    }
    void moveBlockToBegin(MBasicBlock *block) {
        blocks_.remove(block);
        blocks_.pushFront(block);
    }
    size_t numBlocks() const {
        return numBlocks_;
    }
    uint32_t numBlockIds() const {
        return blockIdGen_;
    }
    void allocDefinitionId(MDefinition *ins) {
        // This intentionally starts above 0. The id 0 is in places used to
        // indicate a failure to perform an operation on an instruction.
        idGen_ += 2;
        ins->setId(idGen_);
    }
    uint32_t getMaxInstructionId() {
        return idGen_;
    }
    MResumePoint *entryResumePoint() {
        return blocks_.begin()->entryResumePoint();
    }

    void copyIds(const MIRGraph &other) {
        idGen_ = other.idGen_;
        blockIdGen_ = other.blockIdGen_;
        numBlocks_ = other.numBlocks_;
    }

    void setOsrBlock(MBasicBlock *osrBlock) {
        JS_ASSERT(!osrBlock_);
        osrBlock_ = osrBlock;
    }
    MBasicBlock *osrBlock() {
        return osrBlock_;
    }
    void setOsrStart(MStart *osrStart) {
        osrStart_ = osrStart;
    }
    MStart *osrStart() {
        return osrStart_;
    }
    bool addScript(RawScript script) {
        // The same script may be inlined multiple times, add it only once.
        for (size_t i = 0; i < scripts_.length(); i++) {
            if (scripts_[i] == script)
                return true;
        }
        return scripts_.append(script);
    }
    size_t numScripts() const {
        return scripts_.length();
    }
    JSScript **scripts() {
        return scripts_.begin();
    }

#ifdef DEBUG
    void setInRPO() {
        inRPO_ = true;
    }
#endif

    // The ParSlice is an instance of ForkJoinSlice*, it carries
    // "per-helper-thread" information.  So as not to modify the
    // calling convention for parallel code, we obtain the current
    // slice from thread-local storage.  This helper method will
    // lazilly insert an MParSlice instruction in the entry block and
    // return the definition.
    MDefinition *parSlice();
};

class MDefinitionIterator
{

  friend class MBasicBlock;

  private:
    MBasicBlock *block_;
    MPhiIterator phiIter_;
    MInstructionIterator iter_;

    bool atPhi() const {
        return phiIter_ != block_->phisEnd();
    }

    MDefinition *getIns() {
        if (atPhi())
            return *phiIter_;
        return *iter_;
    }

    void next() {
        if (atPhi())
            phiIter_++;
        else
            iter_++;
    }

    bool more() const {
        return atPhi() || (*iter_) != block_->lastIns();
    }

  public:
    MDefinitionIterator(MBasicBlock *block)
      : block_(block),
        phiIter_(block->phisBegin()),
        iter_(block->begin())
    { }

    MDefinitionIterator operator ++(int) {
        MDefinitionIterator old(*this);
        if (more())
            next();
        return old;
    }

    operator bool() const {
        return more();
    }

    MDefinition *operator *() {
        return getIns();
    }

    MDefinition *operator ->() {
        return getIns();
    }

};

} // namespace ion
} // namespace js

#endif // jsion_mirgraph_h__

