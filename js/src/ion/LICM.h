/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_licm_h__
#define jsion_licm_h__

#include "ion/IonAllocPolicy.h"
#include "ion/IonAnalysis.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"
// This file represents the Loop Invariant Code Motion optimization pass

namespace js {
namespace ion {

class MIRGraph;
class MBasicBlock;

typedef Vector<MBasicBlock*, 1, IonAllocPolicy> BlockQueue;
typedef Vector<MInstruction*, 1, IonAllocPolicy> InstructionQueue;

class LICM
{
    MIRGenerator *mir;
    MIRGraph &graph;

  public:
    LICM(MIRGenerator *mir, MIRGraph &graph);
    bool analyze();
};

class Loop
{
    MIRGenerator *mir;
    MIRGraph &graph;

  public:
    // Loop code may return three values:
    enum LoopReturn {
        LoopReturn_Success,
        LoopReturn_Error, // Compilation failure.
        LoopReturn_Skip   // The loop is not suitable for LICM, but there is no error.
    };

  public:
    // A loop is constructed on a backedge found in the control flow graph.
    Loop(MIRGenerator *mir, MBasicBlock *header, MBasicBlock *footer, MIRGraph &graph);

    // Initializes the loop, finds all blocks and instructions contained in the loop.
    LoopReturn init();

    // Identifies hoistable loop invariant instructions and moves them out of the loop.
    bool optimize();

  private:
    // These blocks define the loop.  header_ points to the loop header, and footer_
    // points to the basic block that has a backedge back to the loop header.
    MBasicBlock *footer_;
    MBasicBlock *header_;

    // The pre-loop block is the first predecessor of the loop header.  It is where
    // the loop is first entered and where hoisted instructions will be placed.
    MBasicBlock* preLoop_;

    // This method recursively traverses the graph from the loop footer back through
    // predecessor edges and stops when it reaches the loop header.
    // Along the way it adds instructions to the worklist for invariance testing.
    LoopReturn iterateLoopBlocks(MBasicBlock *current);

    bool hoistInstructions(InstructionQueue &toHoist);

    // Utility methods for invariance testing and instruction hoisting.
    bool isInLoop(MDefinition *ins);
    bool isLoopInvariant(MInstruction *ins);
    bool isLoopInvariant(MDefinition *ins);

    // This method determines if this block hot within a loop.  That is, if it's
    // always or usually run when the loop executes
    bool checkHotness(MBasicBlock *block);

    // Worklist and worklist usage methods
    InstructionQueue worklist_;
    bool insertInWorklist(MInstruction *ins);
    MInstruction* popFromWorklist();

    inline bool isHoistable(const MDefinition *ins) const {
        return ins->isMovable() && !ins->isEffectful() && !ins->neverHoist();
    }
};

} // namespace ion
} // namespace js

#endif // jsion_licm_h__
