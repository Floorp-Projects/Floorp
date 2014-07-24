/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ScalarReplacement.h"

#include "mozilla/Vector.h"

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

// Returns False if the object is not escaped and if it is optimizable by
// ScalarReplacementOfObject.
//
// For the moment, this code is dumb as it only supports objects which are not
// changing shape, and which are known by TI at the object creation.
static bool
IsObjectEscaped(MInstruction *ins)
{
    MOZ_ASSERT(ins->type() == MIRType_Object);
    MOZ_ASSERT(ins->isNewObject() || ins->isGuardShape());

    // Check if the object is escaped. If the object is not the first argument
    // of either a known Store / Load, then we consider it as escaped. This is a
    // cheap and conservative escape analysis.
    for (MUseIterator i(ins->usesBegin()); i != ins->usesEnd(); i++) {
        MNode *consumer = (*i)->consumer();
        if (!consumer->isDefinition()) {
            // Cannot optimize if it is observable from fun.arguments or others.
            if (consumer->toResumePoint()->isObservableOperand(*i))
                return true;
            continue;
        }

        MDefinition *def = consumer->toDefinition();
        switch (def->op()) {
          case MDefinition::Op_StoreFixedSlot:
          case MDefinition::Op_LoadFixedSlot:
            // Not escaped if it is the first argument.
            if (def->indexOf(*i) == 0)
                break;
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
            if (ins->toNewObject()->templateObject()->lastProperty() != guard->shape())
                return true;
            if (IsObjectEscaped(def->toInstruction()))
                return true;
            break;
          }
          default:
            return true;
        }
    }

    return false;
}

typedef MObjectState BlockState;
typedef Vector<BlockState *, 8, SystemAllocPolicy> GraphState;

// This function replaces every MStoreFixedSlot / MStoreSlot by an MObjectState
// which emulates the content of the object. Every MLoadFixedSlot and MLoadSlot
// is replaced by the corresponding value.
//
// In order to restore the value of the object correctly in case of bailouts, we
// replace all references of the allocation by the MObjectState definitions.
static bool
ScalarReplacementOfObject(MIRGenerator *mir, MIRGraph &graph, GraphState &states,
                          MInstruction *obj)
{
    // For each basic block, we record the last/first state of the object in
    // each of the basic blocks.
    if (!states.appendN(nullptr, graph.numBlocks()))
        return false;

    // Uninitialized slots have an "undefined" value.
    MBasicBlock *objBlock = obj->block();
    MConstant *undefinedVal = MConstant::New(graph.alloc(), UndefinedValue());
    objBlock->insertBefore(obj, undefinedVal);
    states[objBlock->id()] = BlockState::New(graph.alloc(), obj, undefinedVal);

    // Iterate over each basic block and save the object's layout.
    for (ReversePostorderIterator block = graph.rpoBegin(obj->block()); block != graph.rpoEnd(); block++) {
        if (mir->shouldCancel("Scalar Replacement of Object"))
            return false;

        BlockState *state = states[block->id()];
        if (!state) {
            MOZ_ASSERT(!objBlock->dominates(*block));
            continue;
        }

        // Insert the state either at the location of the new object, or after
        // all the phi nodes if the block has multiple predecessors.
        if (*block == objBlock)
            objBlock->insertAfter(obj, state);
        else if (block->numPredecessors() > 1)
            block->insertBefore(*block->begin(), state);
        else
            MOZ_ASSERT(state->block()->dominates(*block));

        // Replace the local variable references by references to the object state.
        ReplaceResumePointOperands(block->entryResumePoint(), obj, state);

        for (MDefinitionIterator ins(*block); ins; ) {
            switch (ins->op()) {
              case MDefinition::Op_ObjectState: {
                ins++;
                continue;
              }

              case MDefinition::Op_LoadFixedSlot: {
                MLoadFixedSlot *def = ins->toLoadFixedSlot();

                // Skip loads made on other objects.
                if (def->object() != obj)
                    break;

                // Replace load by the slot value.
                ins->replaceAllUsesWith(state->getFixedSlot(def->slot()));

                // Remove original instruction.
                ins = block->discardDefAt(ins);
                continue;
              }

              case MDefinition::Op_StoreFixedSlot: {
                MStoreFixedSlot *def = ins->toStoreFixedSlot();

                // Skip stores made on other objects.
                if (def->object() != obj)
                    break;

                // Clone the state and update the slot value.
                state = BlockState::Copy(graph.alloc(), state);
                state->setFixedSlot(def->slot(), def->value());
                block->insertBefore(ins->toInstruction(), state);

                // Remove original instruction.
                ins = block->discardDefAt(ins);
                continue;
              }

              case MDefinition::Op_GuardShape: {
                MGuardShape *def = ins->toGuardShape();

                // Skip loads made on other objects.
                if (def->obj() != obj)
                    break;

                // Replace the shape guard by its object.
                ins->replaceAllUsesWith(obj);

                // Remove original instruction.
                ins = block->discardDefAt(ins);
                continue;
              }

              case MDefinition::Op_LoadSlot: {
                MLoadSlot *def = ins->toLoadSlot();

                // Skip loads made on other objects.
                MSlots *slots = def->slots()->toSlots();
                if (slots->object() != obj) {
                    // Guard objects are replaced when they are visited.
                    MOZ_ASSERT(!slots->object()->isGuardShape() || slots->object()->toGuardShape()->obj() != obj);
                    break;
                }

                // Replace load by the slot value.
                ins->replaceAllUsesWith(state->getDynamicSlot(def->slot()));

                // Remove original instruction.
                ins = block->discardDefAt(ins);
                if (!slots->hasLiveDefUses())
                    slots->block()->discard(slots);
                continue;
              }

              case MDefinition::Op_StoreSlot: {
                MStoreSlot *def = ins->toStoreSlot();

                // Skip stores made on other objects.
                MSlots *slots = def->slots()->toSlots();
                if (slots->object() != obj) {
                    // Guard objects are replaced when they are visited.
                    MOZ_ASSERT(!slots->object()->isGuardShape() || slots->object()->toGuardShape()->obj() != obj);
                    break;
                }

                // Clone the state and update the slot value.
                state = BlockState::Copy(graph.alloc(), state);
                state->setDynamicSlot(def->slot(), def->value());
                block->insertBefore(ins->toInstruction(), state);

                // Remove original instruction.
                ins = block->discardDefAt(ins);
                if (!slots->hasLiveDefUses())
                    slots->block()->discard(slots);
                continue;
              }

              default:
                break;
            }

            // Replace the local variable references by references to the object state.
            if (ins->isInstruction())
                ReplaceResumePointOperands(ins->toInstruction()->resumePoint(), obj, state);

            ins++;
        }

        // For each successor, copy/merge the current state as being the initial
        // state of the successor block.
        for (size_t s = 0; s < block->numSuccessors(); s++) {
            MBasicBlock *succ = block->getSuccessor(s);
            BlockState *succState = states[succ->id()];

            // When a block has no state yet, create an empty one for the
            // successor.
            if (!succState) {
                // If the successor is not dominated then the object cannot flow
                // in this basic block without a Phi.  We know that no Phi exist
                // in non-dominated successors as the conservative escaped
                // analysis fails otherwise.  Such condition can succeed if the
                // successor is a join at the end of a if-block and the object
                // only exists within the branch.
                if (!objBlock->dominates(succ))
                    continue;

                if (succ->numPredecessors() > 1) {
                    succState = states[succ->id()] = BlockState::Copy(graph.alloc(), state);
                    size_t numPreds = succ->numPredecessors();
                    for (size_t slot = 0; slot < state->numSlots(); slot++) {
                        MPhi *phi = MPhi::New(graph.alloc());
                        if (!phi->reserveLength(numPreds))
                            return false;

                        // Fill the input of the successors Phi with undefined
                        // values, and each block later fills the Phi inputs.
                        for (size_t p = 0; p < numPreds; p++)
                            phi->addInput(undefinedVal);

                        // Add Phi in the list of Phis of the basic block.
                        succ->addPhi(phi);
                        succState->setSlot(slot, phi);
                    }
                } else {
                    succState = states[succ->id()] = state;
                }
            }

            if (succ->numPredecessors() > 1) {
                // The current block might appear multiple times among the
                // predecessors. As we need to replace all the inputs, we need
                // to check all predecessors against the current block to
                // replace the Phi node operands.
                size_t numPreds = succ->numPredecessors();
                for (size_t p = 0; p < numPreds; p++) {
                    if (succ->getPredecessor(p) != *block)
                        continue;

                    // Copy the current slot state to the predecessor index of
                    // each Phi of the same slot.
                    for (size_t slot = 0; slot < state->numSlots(); slot++) {
                        MPhi *phi = succState->getSlot(slot)->toPhi();
                        phi->replaceOperand(p, state->getSlot(slot));
                    }
                }
            }
        }
    }

    MOZ_ASSERT(!obj->hasLiveDefUses());
    obj->setRecoveredOnBailout();
    states.clear();
    return true;
}

bool
ScalarReplacement(MIRGenerator *mir, MIRGraph &graph)
{
    GraphState objectStates;
    for (ReversePostorderIterator block = graph.rpoBegin(); block != graph.rpoEnd(); block++) {
        if (mir->shouldCancel("Scalar Replacement (main loop)"))
            return false;

        for (MInstructionIterator ins = block->begin(); ins != block->end(); ins++) {
            if (ins->isNewObject() && !IsObjectEscaped(*ins)) {
                if (!ScalarReplacementOfObject(mir, graph, objectStates, *ins))
                    return false;
            }
        }
    }

    return true;
}

} /* namespace jit */
} /* namespace js */
