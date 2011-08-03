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
 *   Andrew Scheff <ascheff@mozilla.com>
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

#ifndef jsion_licm_h__
#define jsion_licm_h__

// This file represents the Loop Invariant Code Motion optimization pass

namespace js {
namespace ion {

class MIRGraph;
class MBasicBlock;

typedef Vector<MBasicBlock*, 1, IonAllocPolicy> BlockQueue;
typedef Vector<MInstruction*, 1, IonAllocPolicy> InstructionQueue;

class LICM
{
    MIRGraph &graph;

  public:
    LICM(MIRGraph &graph);
    bool analyze();
};

class Loop
{
    MIRGraph &graph;

  public:
    // A loop is constructed on a backedge found in the control flow graph.
    Loop(MBasicBlock *header, MBasicBlock *footer, MIRGraph &graph);

    // Initializes the loop, finds all blocks and instructions contained in the loop.
    bool init();

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
    bool iterateLoopBlocks(MBasicBlock *current);

    bool hoistInstructions(InstructionQueue &toHoist);

    // Utility methods for invariance testing and instruction hoisting.
    bool isInLoop(MDefinition *ins);
    bool isLoopInvariant(MInstruction *ins);

    // This method determines if this block hot within a loop.  That is, if it's
    // always or usually run when the loop executes
    bool checkHotness(MBasicBlock *block);

    // Worklist and worklist usage methods
    InstructionQueue worklist_;
    bool insertInWorklist(MInstruction *ins);
    MInstruction* popFromWorklist();

    inline bool isHoistable(const MDefinition *ins) const {
        return ins->isIdempotent() && !ins->isNeverHoisted();
    }

};

} // namespace js
} // namespace ion

#endif // jsion_licm_h__
