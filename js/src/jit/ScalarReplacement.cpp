/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ScalarReplacement.h"

#include "mozilla/Vector.h"

#include "jit/IonAnalysis.h"
#include "jit/JitSpewer.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"

namespace js {
namespace jit {

// Scan resume point operands in search of a local variable which captures the
// current object, and replace it by the current object with its state.
static void
ReplaceResumePointOperands(MResumePoint *resumePoint, MDefinition *object, MDefinition *state)
{
    // Note: This function iterates over the caller as well, this is wrong
    // because if the object appears in one of the caller, we want to correctly
    // recover the object value from any block having the same caller.  In
    // practice, this is correct for 2 reasons:
    //
    // 1. We replace resume point operands in RPO, this implies that the caller
    // would first be updated when we update the resume point of entry block of
    // the inner function.  This implies that the object state would only hold
    // valid data for the caller resume point.
    //
    // 2. The caller resume point will have no reference of the new object
    // allocation if the object allocation is done within the callee.
    //
    // A side-effect of this implementation is that we would be restoring and
    // keeping tracks of the content of the object at the entry of the function,
    // in addition to the content of the object within the function.
    for (MResumePoint *rp = resumePoint; rp; rp = rp->caller()) {
        for (size_t op = 0; op < rp->numOperands(); op++) {
            if (rp->getOperand(op) == object) {
                rp->replaceOperand(op, state);

                // This assertion verifies the comment which is above still
                // holds.  Note, this is not true if rp == resumePoint, as the
                // object state can be a new one created at the beginning of the
                // block to keep track of the merge state.
                MOZ_ASSERT_IF(rp != resumePoint, state->block()->dominates(rp->block()));
            }
        }
    }
}

template <typename MemoryView>
class EmulateStateOf
{
  private:
    typedef typename MemoryView::BlockState BlockState;

    MIRGenerator *mir_;
    MIRGraph &graph_;

    // Block state at the entrance of all basic blocks.
    Vector<BlockState *, 8, SystemAllocPolicy> states_;

  public:
    EmulateStateOf(MIRGenerator *mir, MIRGraph &graph)
      : mir_(mir),
        graph_(graph)
    {
    }

    bool run(MemoryView &view);
};

template <typename MemoryView>
bool
EmulateStateOf<MemoryView>::run(MemoryView &view)
{
    // Initialize the current block state of each block to an unknown state.
    if (!states_.appendN(nullptr, graph_.numBlocks()))
        return false;

    // Initialize the first block which needs to be traversed in RPO.
    MBasicBlock *startBlock = view.startingBlock();
    if (!view.initStartingState(&states_[startBlock->id()]))
        return false;

    // Iterate over each basic block which has a valid entry state, and merge
    // the state in the successor blocks.
    for (ReversePostorderIterator block = graph_.rpoBegin(startBlock); block != graph_.rpoEnd(); block++) {
        if (mir_->shouldCancel(MemoryView::phaseName))
            return false;

        // Get the block state as the result of the merge of all predecessors
        // which have already been visited in RPO.  This means that backedges
        // are not yet merged into the loop.
        BlockState *state = states_[block->id()];
        if (!state)
            continue;
        view.setEntryBlockState(state);

        // Iterates over resume points, phi and instructions.
        for (MNodeIterator iter(*block); iter; ) {
            // Increment the iterator before visiting the instruction, as the
            // visit function might discard itself from the basic block.
            MNode *ins = *iter++;
            if (ins->isDefinition()) {
                if (!ins->toDefinition()->accept(&view))
                    return false;
            } else if (!view.visitResumePoint(ins->toResumePoint())) {
                return false;
            }
        }

        // For each successor, merge the current state into the state of the
        // successors.
        for (size_t s = 0; s < block->numSuccessors(); s++) {
            MBasicBlock *succ = block->getSuccessor(s);
            if (!view.mergeIntoSuccessorState(*block, succ, &states_[succ->id()]))
                return false;
        }
    }

    states_.clear();
    return true;
}

// Returns False if the object is not escaped and if it is optimizable by
// ScalarReplacementOfObject.
//
// For the moment, this code is dumb as it only supports objects which are not
// changing shape, and which are known by TI at the object creation.
static bool
IsObjectEscaped(MInstruction *ins)
{
    MOZ_ASSERT(ins->type() == MIRType_Object);
    MOZ_ASSERT(ins->isNewObject() || ins->isGuardShape() || ins->isCreateThisWithTemplate());

    JSObject *obj = nullptr;
    if (ins->isNewObject())
        obj = ins->toNewObject()->templateObject();
    else if (ins->isCreateThisWithTemplate())
        obj = ins->toCreateThisWithTemplate()->templateObject();

    // Check if the object is escaped. If the object is not the first argument
    // of either a known Store / Load, then we consider it as escaped. This is a
    // cheap and conservative escape analysis.
    for (MUseIterator i(ins->usesBegin()); i != ins->usesEnd(); i++) {
        MNode *consumer = (*i)->consumer();
        if (!consumer->isDefinition()) {
            // Cannot optimize if it is observable from fun.arguments or others.
            if (!consumer->toResumePoint()->isRecoverableOperand(*i)) {
                JitSpewDef(JitSpew_Escape, "Observable object cannot be recovered\n", ins);
                return true;
            }
            continue;
        }

        MDefinition *def = consumer->toDefinition();
        switch (def->op()) {
          case MDefinition::Op_StoreFixedSlot:
          case MDefinition::Op_LoadFixedSlot:
            // Not escaped if it is the first argument.
            if (def->indexOf(*i) == 0)
                break;

            JitSpewDef(JitSpew_Escape, "Object ", ins);
            JitSpewDef(JitSpew_Escape, "  is escaped by\n", def);
            return true;

          case MDefinition::Op_Slots: {
#ifdef DEBUG
            // Assert that MSlots are only used by MStoreSlot and MLoadSlot.
            MSlots *ins = def->toSlots();
            MOZ_ASSERT(ins->object() != 0);
            for (MUseIterator i(ins->usesBegin()); i != ins->usesEnd(); i++) {
                // toDefinition should normally never fail, since they don't get
                // captured by resume points.
                MDefinition *def = (*i)->consumer()->toDefinition();
                MOZ_ASSERT(def->op() == MDefinition::Op_StoreSlot ||
                           def->op() == MDefinition::Op_LoadSlot);
            }
#endif
            break;
          }

          case MDefinition::Op_GuardShape: {
            MGuardShape *guard = def->toGuardShape();
            MOZ_ASSERT(!ins->isGuardShape());
            if (obj->lastProperty() != guard->shape()) {
                JitSpewDef(JitSpew_Escape, "Object ", ins);
                JitSpewDef(JitSpew_Escape, "  has a non-matching guard shape\n", guard);
                return true;
            }
            if (IsObjectEscaped(def->toInstruction()))
                return true;
            break;
          }
          default:
            JitSpewDef(JitSpew_Escape, "Object ", ins);
            JitSpewDef(JitSpew_Escape, "  is escaped by\n", def);
            return true;
        }
    }

    JitSpewDef(JitSpew_Escape, "Object is not escaped\n", ins);
    return false;
}

class ObjectMemoryView : public MDefinitionVisitorDefaultNoop
{
  public:
    typedef MObjectState BlockState;
    static const char *phaseName;

  private:
    TempAllocator &alloc_;
    MConstant *undefinedVal_;
    MInstruction *obj_;
    MBasicBlock *startBlock_;
    BlockState *state_;

  public:
    ObjectMemoryView(TempAllocator &alloc, MInstruction *obj);

    MBasicBlock *startingBlock();
    bool initStartingState(BlockState **pState);

    void setEntryBlockState(BlockState *state);
    bool mergeIntoSuccessorState(MBasicBlock *curr, MBasicBlock *succ, BlockState **pSuccState);

#ifdef DEBUG
    void assertSuccess();
#else
    void assertSuccess() {}
#endif

  public:
    bool visitResumePoint(MResumePoint *rp);
    bool visitStoreFixedSlot(MStoreFixedSlot *ins);
    bool visitLoadFixedSlot(MLoadFixedSlot *ins);
    bool visitStoreSlot(MStoreSlot *ins);
    bool visitLoadSlot(MLoadSlot *ins);
    bool visitGuardShape(MGuardShape *ins);
};

const char *ObjectMemoryView::phaseName = "Scalar Replacement of Object";

ObjectMemoryView::ObjectMemoryView(TempAllocator &alloc, MInstruction *obj)
  : alloc_(alloc),
    obj_(obj),
    startBlock_(obj->block())
{
}

MBasicBlock *
ObjectMemoryView::startingBlock()
{
    return startBlock_;
}

bool
ObjectMemoryView::initStartingState(BlockState **pState)
{
    // Uninitialized slots have an "undefined" value.
    undefinedVal_ = MConstant::New(alloc_, UndefinedValue());
    startBlock_->insertBefore(obj_, undefinedVal_);

    // Create a new block state and insert at it at the location of the new object.
    BlockState *state = BlockState::New(alloc_, obj_, undefinedVal_);
    startBlock_->insertAfter(obj_, state);

    *pState = state;
    return true;
}

void
ObjectMemoryView::setEntryBlockState(BlockState *state)
{
    state_ = state;
}

bool
ObjectMemoryView::mergeIntoSuccessorState(MBasicBlock *curr, MBasicBlock *succ,
                                          BlockState **pSuccState)
{
    BlockState *succState = *pSuccState;

    // When a block has no state yet, create an empty one for the
    // successor.
    if (!succState) {
        // If the successor is not dominated then the object cannot flow
        // in this basic block without a Phi.  We know that no Phi exist
        // in non-dominated successors as the conservative escaped
        // analysis fails otherwise.  Such condition can succeed if the
        // successor is a join at the end of a if-block and the object
        // only exists within the branch.
        if (!startBlock_->dominates(succ))
            return true;

        // If there is only one predecessor, carry over the last state of the
        // block to the successor.  As the block state is immutable, if the
        // current block has multiple successors, they will share the same entry
        // state.
        if (succ->numPredecessors() <= 1 || !state_->numSlots()) {
            *pSuccState = state_;
            return true;
        }

        // If we have multiple predecessors, then we allocate one Phi node for
        // each predecessor, and create a new block state which only has phi
        // nodes.  These would later be removed by the removal of redundant phi
        // nodes.
        succState = BlockState::Copy(alloc_, state_);
        size_t numPreds = succ->numPredecessors();
        for (size_t slot = 0; slot < state_->numSlots(); slot++) {
            MPhi *phi = MPhi::New(alloc_);
            if (!phi->reserveLength(numPreds))
                return false;

            // Fill the input of the successors Phi with undefined
            // values, and each block later fills the Phi inputs.
            for (size_t p = 0; p < numPreds; p++)
                phi->addInput(undefinedVal_);

            // Add Phi in the list of Phis of the basic block.
            succ->addPhi(phi);
            succState->setSlot(slot, phi);
        }

        // Insert the newly created block state instruction at the beginning
        // of the successor block, after all the phi nodes.  Note that it
        // would be captured by the entry resume point of the successor
        // block.
        succ->insertBefore(*succ->begin(), succState);
        *pSuccState = succState;
    }

    MOZ_ASSERT_IF(succ == startBlock_, startBlock_->isLoopHeader());
    if (succ->numPredecessors() > 1 && succState->numSlots() && succ != startBlock_) {
        // We need to re-compute successorWithPhis as the previous EliminatePhis
        // phase might have removed all the Phis from the successor block.
        size_t currIndex;
        MOZ_ASSERT(!succ->phisEmpty());
        if (curr->successorWithPhis()) {
            MOZ_ASSERT(curr->successorWithPhis() == succ);
            currIndex = curr->positionInPhiSuccessor();
        } else {
            currIndex = succ->indexForPredecessor(curr);
            curr->setSuccessorWithPhis(succ, currIndex);
        }
        MOZ_ASSERT(succ->getPredecessor(currIndex) == curr);

        // Copy the current slot states to the index of current block in all the
        // Phi created during the first visit of the successor.
        for (size_t slot = 0; slot < state_->numSlots(); slot++) {
            MPhi *phi = succState->getSlot(slot)->toPhi();
            phi->replaceOperand(currIndex, state_->getSlot(slot));
        }
    }

    return true;
}

#ifdef DEBUG
void
ObjectMemoryView::assertSuccess()
{
    for (MUseIterator i(obj_->usesBegin()); i != obj_->usesEnd(); i++) {
        MNode *ins = (*i)->consumer();

        // Resume points have been replaced by the object state.
        MOZ_ASSERT(!ins->isResumePoint());

        MDefinition *def = ins->toDefinition();

        if (def->isRecoveredOnBailout())
            continue;

        // The only remaining uses would be removed by DCE, which will also
        // recover the object on bailouts.
        MOZ_ASSERT(def->isSlots());
        MOZ_ASSERT(!def->hasOneUse());
    }
}
#endif

bool
ObjectMemoryView::visitResumePoint(MResumePoint *rp)
{
    ReplaceResumePointOperands(rp, obj_, state_);
    return true;
}

bool
ObjectMemoryView::visitStoreFixedSlot(MStoreFixedSlot *ins)
{
    // Skip stores made on other objects.
    if (ins->object() != obj_)
        return true;

    // Clone the state and update the slot value.
    state_ = BlockState::Copy(alloc_, state_);
    state_->setFixedSlot(ins->slot(), ins->value());
    ins->block()->insertBefore(ins->toInstruction(), state_);

    // Remove original instruction.
    ins->block()->discard(ins);
    return true;
}

bool
ObjectMemoryView::visitLoadFixedSlot(MLoadFixedSlot *ins)
{
    // Skip loads made on other objects.
    if (ins->object() != obj_)
        return true;

    // Replace load by the slot value.
    ins->replaceAllUsesWith(state_->getFixedSlot(ins->slot()));

    // Remove original instruction.
    ins->block()->discard(ins);
    return true;
}

bool
ObjectMemoryView::visitStoreSlot(MStoreSlot *ins)
{
    // Skip stores made on other objects.
    MSlots *slots = ins->slots()->toSlots();
    if (slots->object() != obj_) {
        // Guard objects are replaced when they are visited.
        MOZ_ASSERT(!slots->object()->isGuardShape() || slots->object()->toGuardShape()->obj() != obj_);
        return true;
    }

    // Clone the state and update the slot value.
    state_ = BlockState::Copy(alloc_, state_);
    state_->setDynamicSlot(ins->slot(), ins->value());
    ins->block()->insertBefore(ins->toInstruction(), state_);

    // Remove original instruction.
    ins->block()->discard(ins);
    return true;
}

bool
ObjectMemoryView::visitLoadSlot(MLoadSlot *ins)
{
    // Skip loads made on other objects.
    MSlots *slots = ins->slots()->toSlots();
    if (slots->object() != obj_) {
        // Guard objects are replaced when they are visited.
        MOZ_ASSERT(!slots->object()->isGuardShape() || slots->object()->toGuardShape()->obj() != obj_);
        return true;
    }

    // Replace load by the slot value.
    ins->replaceAllUsesWith(state_->getDynamicSlot(ins->slot()));

    // Remove original instruction.
    ins->block()->discard(ins);
    return true;
}

bool
ObjectMemoryView::visitGuardShape(MGuardShape *ins)
{
    // Skip loads made on other objects.
    if (ins->obj() != obj_)
        return true;

    // Replace the shape guard by its object.
    ins->replaceAllUsesWith(obj_);

    // Remove original instruction.
    ins->block()->discard(ins);
    return true;
}

static bool
IndexOf(MDefinition *ins, int32_t *res)
{
    MOZ_ASSERT(ins->isLoadElement() || ins->isStoreElement());
    MDefinition *indexDef = ins->getOperand(1); // ins->index();
    if (indexDef->isBoundsCheck())
        indexDef = indexDef->toBoundsCheck()->index();
    if (indexDef->isToInt32())
        indexDef = indexDef->toToInt32()->getOperand(0);
    if (!indexDef->isConstant())
        return false;

    Value index = indexDef->toConstant()->value();
    if (!index.isInt32())
        return false;
    *res = index.toInt32();
    return true;
}

// Returns False if the array is not escaped and if it is optimizable by
// ScalarReplacementOfArray.
//
// For the moment, this code is dumb as it only supports arrays which are not
// changing length, with only access with known constants.
static bool
IsArrayEscaped(MInstruction *ins)
{
    MOZ_ASSERT(ins->type() == MIRType_Object);
    MOZ_ASSERT(ins->isNewArray());
    uint32_t count = ins->toNewArray()->count();

    // The array is probably too large to be represented efficiently with
    // MArrayState, and we do not want to make huge allocations during bailouts.
    if (ins->toNewArray()->allocatingBehaviour() == NewArray_Unallocating) {
        JitSpewDef(JitSpew_Escape, "Array is not allocated\n", ins);
        return true;
    }

    if (count >= 16) {
        JitSpewDef(JitSpew_Escape, "Array has too many elements\n", ins);
        return true;
    }

    // Check if the object is escaped. If the object is not the first argument
    // of either a known Store / Load, then we consider it as escaped. This is a
    // cheap and conservative escape analysis.
    for (MUseIterator i(ins->usesBegin()); i != ins->usesEnd(); i++) {
        MNode *consumer = (*i)->consumer();
        if (!consumer->isDefinition()) {
            // Cannot optimize if it is observable from fun.arguments or others.
            if (!consumer->toResumePoint()->isRecoverableOperand(*i)) {
                JitSpewDef(JitSpew_Escape, "Observable array cannot be recovered\n", ins);
                return true;
            }
            continue;
        }

        MDefinition *def = consumer->toDefinition();
        switch (def->op()) {
          case MDefinition::Op_Elements: {
            MOZ_ASSERT(def->toElements()->object() == ins);
            for (MUseIterator i(def->usesBegin()); i != def->usesEnd(); i++) {
                // The MIRType_Elements cannot be captured in a resume point as
                // it does not represent a value allocation.
                MDefinition *access = (*i)->consumer()->toDefinition();

                switch (access->op()) {
                  case MDefinition::Op_LoadElement: {
                    MOZ_ASSERT(access->toLoadElement()->elements() == def);

                    // If we need hole checks, then the array cannot be escaped
                    // as the array might refer to the prototype chain to look
                    // for properties, thus it might do additional side-effects
                    // which are not reflected by the alias set, is we are
                    // bailing on holes.
                    if (access->toLoadElement()->needsHoleCheck()) {
                        JitSpewDef(JitSpew_Escape, "Array ", ins);
                        JitSpewDef(JitSpew_Escape,
                                   "  has a load element which needs hole check\n", access);
                        return true;
                    }

                    // If the index is not a constant then this index can alias
                    // all others. We do not handle this case.
                    int32_t index;
                    if (!IndexOf(access, &index)) {
                        JitSpewDef(JitSpew_Escape, "Array ", ins);
                        JitSpewDef(JitSpew_Escape,
                                   "  has a load element with a non-trivial index\n", access);
                        return true;
                    }
                    if (index < 0 || count <= uint32_t(index)) {
                        JitSpewDef(JitSpew_Escape, "Array ", ins);
                        JitSpewDef(JitSpew_Escape,
                                   "  has a load element with an out-of-bound index\n", access);
                        return true;
                    }
                    break;
                  }

                  case MDefinition::Op_StoreElement: {
                    MOZ_ASSERT(access->toStoreElement()->elements() == def);

                    // If we need hole checks, then the array cannot be escaped
                    // as the array might refer to the prototype chain to look
                    // for properties, thus it might do additional side-effects
                    // which are not reflected by the alias set, is we are
                    // bailing on holes.
                    if (access->toStoreElement()->needsHoleCheck())
                        return true;

                    // If the index is not a constant then this index can alias
                    // all others. We do not handle this case.
                    int32_t index;
                    if (!IndexOf(access, &index)) {
                        JitSpewDef(JitSpew_Escape, "Array ", ins);
                        JitSpewDef(JitSpew_Escape, "  has a store element with a non-trivial index\n", access);
                        return true;
                    }
                    if (index < 0 || count <= uint32_t(index)) {
                        JitSpewDef(JitSpew_Escape, "Array ", ins);
                        JitSpewDef(JitSpew_Escape, "  has a store element with an out-of-bound index\n", access);
                        return true;
                    }

                    // We are not yet encoding magic hole constants in resume points.
                    if (access->toStoreElement()->value()->type() == MIRType_MagicHole) {
                        JitSpewDef(JitSpew_Escape, "Array ", ins);
                        JitSpewDef(JitSpew_Escape, "  has a store element with an magic-hole constant\n", access);
                        return true;
                    }
                    break;
                  }

                  case MDefinition::Op_SetInitializedLength:
                    MOZ_ASSERT(access->toSetInitializedLength()->elements() == def);
                    break;

                  case MDefinition::Op_InitializedLength:
                    MOZ_ASSERT(access->toInitializedLength()->elements() == def);
                    break;

                  case MDefinition::Op_ArrayLength:
                    MOZ_ASSERT(access->toArrayLength()->elements() == def);
                    break;

                  default:
                    JitSpewDef(JitSpew_Escape, "Array's element ", ins);
                    JitSpewDef(JitSpew_Escape, "  is escaped by\n", def);
                    return true;
                }
            }

            break;
          }

          default:
            JitSpewDef(JitSpew_Escape, "Array ", ins);
            JitSpewDef(JitSpew_Escape, "  is escaped by\n", def);
            return true;
        }
    }

    JitSpewDef(JitSpew_Escape, "Array is not escaped\n", ins);
    return false;
}

// This class replaces every MStoreElement and MSetInitializedLength by an
// MArrayState which emulates the content of the array. All MLoadElement,
// MInitializedLength and MArrayLength are replaced by the corresponding value.
//
// In order to restore the value of the array correctly in case of bailouts, we
// replace all reference of the allocation by the MArrayState definition.
class ArrayMemoryView : public MDefinitionVisitorDefaultNoop
{
  public:
    typedef MArrayState BlockState;
    static const char *phaseName;

  private:
    TempAllocator &alloc_;
    MConstant *undefinedVal_;
    MConstant *length_;
    MInstruction *arr_;
    MBasicBlock *startBlock_;
    BlockState *state_;

  public:
    ArrayMemoryView(TempAllocator &alloc, MInstruction *arr);

    MBasicBlock *startingBlock();
    bool initStartingState(BlockState **pState);

    void setEntryBlockState(BlockState *state);
    bool mergeIntoSuccessorState(MBasicBlock *curr, MBasicBlock *succ, BlockState **pSuccState);

#ifdef DEBUG
    void assertSuccess();
#else
    void assertSuccess() {}
#endif

  private:
    bool isArrayStateElements(MDefinition *elements);
    void discardInstruction(MInstruction *ins, MDefinition *elements);

  public:
    bool visitResumePoint(MResumePoint *rp);
    bool visitStoreElement(MStoreElement *ins);
    bool visitLoadElement(MLoadElement *ins);
    bool visitSetInitializedLength(MSetInitializedLength *ins);
    bool visitInitializedLength(MInitializedLength *ins);
    bool visitArrayLength(MArrayLength *ins);
};

const char *ArrayMemoryView::phaseName = "Scalar Replacement of Array";

ArrayMemoryView::ArrayMemoryView(TempAllocator &alloc, MInstruction *arr)
  : alloc_(alloc),
    undefinedVal_(nullptr),
    length_(nullptr),
    arr_(arr),
    startBlock_(arr->block()),
    state_(nullptr)
{
}

MBasicBlock *
ArrayMemoryView::startingBlock()
{
    return startBlock_;
}

bool
ArrayMemoryView::initStartingState(BlockState **pState)
{
    // Uninitialized elements have an "undefined" value.
    undefinedVal_ = MConstant::New(alloc_, UndefinedValue());
    MConstant *initLength = MConstant::New(alloc_, Int32Value(0));
    arr_->block()->insertBefore(arr_, undefinedVal_);
    arr_->block()->insertBefore(arr_, initLength);

    // Create a new block state and insert at it at the location of the new array.
    BlockState *state = BlockState::New(alloc_, arr_, undefinedVal_, initLength);
    startBlock_->insertAfter(arr_, state);

    *pState = state;
    return true;
}

void
ArrayMemoryView::setEntryBlockState(BlockState *state)
{
    state_ = state;
}

bool
ArrayMemoryView::mergeIntoSuccessorState(MBasicBlock *curr, MBasicBlock *succ,
                                          BlockState **pSuccState)
{
    BlockState *succState = *pSuccState;

    // When a block has no state yet, create an empty one for the
    // successor.
    if (!succState) {
        // If the successor is not dominated then the array cannot flow
        // in this basic block without a Phi.  We know that no Phi exist
        // in non-dominated successors as the conservative escaped
        // analysis fails otherwise.  Such condition can succeed if the
        // successor is a join at the end of a if-block and the array
        // only exists within the branch.
        if (!startBlock_->dominates(succ))
            return true;

        // If there is only one predecessor, carry over the last state of the
        // block to the successor.  As the block state is immutable, if the
        // current block has multiple successors, they will share the same entry
        // state.
        if (succ->numPredecessors() <= 1 || !state_->numElements()) {
            *pSuccState = state_;
            return true;
        }

        // If we have multiple predecessors, then we allocate one Phi node for
        // each predecessor, and create a new block state which only has phi
        // nodes.  These would later be removed by the removal of redundant phi
        // nodes.
        succState = BlockState::Copy(alloc_, state_);
        size_t numPreds = succ->numPredecessors();
        for (size_t index = 0; index < state_->numElements(); index++) {
            MPhi *phi = MPhi::New(alloc_);
            if (!phi->reserveLength(numPreds))
                return false;

            // Fill the input of the successors Phi with undefined
            // values, and each block later fills the Phi inputs.
            for (size_t p = 0; p < numPreds; p++)
                phi->addInput(undefinedVal_);

            // Add Phi in the list of Phis of the basic block.
            succ->addPhi(phi);
            succState->setElement(index, phi);
        }

        // Insert the newly created block state instruction at the beginning
        // of the successor block, after all the phi nodes.  Note that it
        // would be captured by the entry resume point of the successor
        // block.
        succ->insertBefore(*succ->begin(), succState);
        *pSuccState = succState;
    }

    MOZ_ASSERT_IF(succ == startBlock_, startBlock_->isLoopHeader());
    if (succ->numPredecessors() > 1 && succState->numElements() && succ != startBlock_) {
        // We need to re-compute successorWithPhis as the previous EliminatePhis
        // phase might have removed all the Phis from the successor block.
        size_t currIndex;
        MOZ_ASSERT(!succ->phisEmpty());
        if (curr->successorWithPhis()) {
            MOZ_ASSERT(curr->successorWithPhis() == succ);
            currIndex = curr->positionInPhiSuccessor();
        } else {
            currIndex = succ->indexForPredecessor(curr);
            curr->setSuccessorWithPhis(succ, currIndex);
        }
        MOZ_ASSERT(succ->getPredecessor(currIndex) == curr);

        // Copy the current element states to the index of current block in all
        // the Phi created during the first visit of the successor.
        for (size_t index = 0; index < state_->numElements(); index++) {
            MPhi *phi = succState->getElement(index)->toPhi();
            phi->replaceOperand(currIndex, state_->getElement(index));
        }
    }

    return true;
}

#ifdef DEBUG
void
ArrayMemoryView::assertSuccess()
{
    MOZ_ASSERT(!arr_->hasLiveDefUses());
}
#endif

bool
ArrayMemoryView::visitResumePoint(MResumePoint *rp)
{
    ReplaceResumePointOperands(rp, arr_, state_);
    return true;
}

bool
ArrayMemoryView::isArrayStateElements(MDefinition *elements)
{
    return elements->isElements() && elements->toElements()->object() == arr_;
}

void
ArrayMemoryView::discardInstruction(MInstruction *ins, MDefinition *elements)
{
    MOZ_ASSERT(elements->isElements());
    ins->block()->discard(ins);
    if (!elements->hasLiveDefUses())
        elements->block()->discard(elements->toInstruction());
}

bool
ArrayMemoryView::visitStoreElement(MStoreElement *ins)
{
    // Skip other array objects.
    MDefinition *elements = ins->elements();
    if (!isArrayStateElements(elements))
        return true;

    // Register value of the setter in the state.
    int32_t index;
    MOZ_ALWAYS_TRUE(IndexOf(ins, &index));
    state_ = BlockState::Copy(alloc_, state_);
    state_->setElement(index, ins->value());
    ins->block()->insertBefore(ins, state_);

    // Remove original instruction.
    discardInstruction(ins, elements);
    return true;
}

bool
ArrayMemoryView::visitLoadElement(MLoadElement *ins)
{
    // Skip other array objects.
    MDefinition *elements = ins->elements();
    if (!isArrayStateElements(elements))
        return true;

    // Replace by the value contained at the index.
    int32_t index;
    MOZ_ALWAYS_TRUE(IndexOf(ins, &index));
    ins->replaceAllUsesWith(state_->getElement(index));

    // Remove original instruction.
    discardInstruction(ins, elements);
    return true;
}

bool
ArrayMemoryView::visitSetInitializedLength(MSetInitializedLength *ins)
{
    // Skip other array objects.
    MDefinition *elements = ins->elements();
    if (!isArrayStateElements(elements))
        return true;

    // Replace by the new initialized length.  Note that the argument of
    // MSetInitalizedLength is the last index and not the initialized length.
    // To obtain the length, we need to add 1 to it, and thus we need to create
    // a new constant that we register in the ArrayState.
    state_ = BlockState::Copy(alloc_, state_);
    int32_t initLengthValue = ins->index()->toConstant()->value().toInt32() + 1;
    MConstant *initLength = MConstant::New(alloc_, Int32Value(initLengthValue));
    ins->block()->insertBefore(ins, initLength);
    ins->block()->insertBefore(ins, state_);
    state_->setInitializedLength(initLength);

    // Remove original instruction.
    discardInstruction(ins, elements);
    return true;
}

bool
ArrayMemoryView::visitInitializedLength(MInitializedLength *ins)
{
    // Skip other array objects.
    MDefinition *elements = ins->elements();
    if (!isArrayStateElements(elements))
        return true;

    // Replace by the value of the length.
    ins->replaceAllUsesWith(state_->initializedLength());

    // Remove original instruction.
    discardInstruction(ins, elements);
    return true;
}

bool
ArrayMemoryView::visitArrayLength(MArrayLength *ins)
{
    // Skip other array objects.
    MDefinition *elements = ins->elements();
    if (!isArrayStateElements(elements))
        return true;

    // Replace by the value of the length.
    if (!length_) {
        length_ = MConstant::New(alloc_, Int32Value(state_->numElements()));
        arr_->block()->insertBefore(arr_, length_);
    }
    ins->replaceAllUsesWith(length_);

    // Remove original instruction.
    discardInstruction(ins, elements);
    return true;
}

bool
ScalarReplacement(MIRGenerator *mir, MIRGraph &graph)
{
    EmulateStateOf<ObjectMemoryView> replaceObject(mir, graph);
    EmulateStateOf<ArrayMemoryView> replaceArray(mir, graph);
    bool addedPhi = false;

    for (ReversePostorderIterator block = graph.rpoBegin(); block != graph.rpoEnd(); block++) {
        if (mir->shouldCancel("Scalar Replacement (main loop)"))
            return false;

        for (MInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
            if ((ins->isNewObject() || ins->isCreateThisWithTemplate()) && !IsObjectEscaped(*ins)) {
                ObjectMemoryView view(graph.alloc(), *ins);
                if (!replaceObject.run(view))
                    return false;
                view.assertSuccess();
                addedPhi = true;
                continue;
            }

            if (ins->isNewArray() && !IsArrayEscaped(*ins)) {
                ArrayMemoryView view(graph.alloc(), *ins);
                if (!replaceArray.run(view))
                    return false;
                view.assertSuccess();
                addedPhi = true;
                continue;
            }
        }
    }

    if (addedPhi) {
        // Phis added by Scalar Replacement are only redundant Phis which are
        // not directly captured by any resume point but only by the MDefinition
        // state. The conservative observability only focuses on Phis which are
        // not used as resume points operands.
        AssertExtendedGraphCoherency(graph);
        if (!EliminatePhis(mir, graph, ConservativeObservability))
            return false;
    }

    return true;
}

} /* namespace jit */
} /* namespace js */
