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

#ifndef jsion_ion_lowering_h__
#define jsion_ion_lowering_h__

// This file declares the structures that are used for attaching LIR to a
// MIRGraph.

#include "IonAllocPolicy.h"
#include "IonLIR.h"
#include "MOpcodes.h"

namespace js {
namespace ion {

class MBasicBlock;
class MIRGenerator;
class MIRGraph;
class MInstruction;

class LBlock : public TempObject
{
    MBasicBlock *block_;
    InlineList<LInstruction> instructions_;

    LBlock(MBasicBlock *block)
      : block_(block)
    { }

  public:
    static LBlock *New(MBasicBlock *from) {
        return new LBlock(from);
    }

    void add(LInstruction *ins) {
        instructions_.insert(ins);
    }

    MBasicBlock *mir() const {
        return block_;
    }
};

class LIRGenerator : public MInstructionVisitor
{
    MIRGenerator *gen;
    MIRGraph &graph;
    LBlock *current;
    uint32 vregGen_;

  public:
    LIRGenerator(MIRGenerator *gen, MIRGraph &graph)
      : gen(gen),
        graph(graph),
        vregGen_(0)
    { }

    bool generate();
    uint32 nextVirtualRegister() {
        return ++vregGen_;
    }

  protected:
    // A backend can decide that an instruction should be emitted at its uses,
    // rather than at its definition. To communicate this, set the
    // instruction's virtual register set to 0. When using the instruction,
    // its virtual register is temporarily reassigned. To know to clear it
    // after constructing the use information, the worklist bit is temporarily
    // unset.
    //
    // The backend can use the worklist bit to determine whether or not a
    // definition should be created.
    bool emitAtUses(MInstruction *mir);

    // The lowest-level calls to use, those that do not wrap another call to
    // use(), must prefix grabbing virtual register IDs by these calls.
    inline void startUsing(MInstruction *mir);
    inline void stopUsing(MInstruction *mir);

    // These all create a use of a virtual register, with an optional
    // allocation policy.
    LUse use(MInstruction *mir, LUse policy);
    inline LUse use(MInstruction *mir);
    inline LUse useRegister(MInstruction *mir);
    inline LUse useFixed(MInstruction *mir, Register reg);
    inline LUse useFixed(MInstruction *mir, FloatRegister reg);
    inline LAllocation useOrConstant(MInstruction *mir);
    inline LAllocation useRegisterOrConstant(MInstruction *mir);

    // These create temporary register requests.
    inline LDefinition temp(LDefinition::Type type);

    template <size_t X, size_t Y>
    inline bool define(LInstructionHelper<1, X, Y> *lir, MInstruction *mir,
                        const LDefinition &def);

    template <size_t X, size_t Y>
    inline bool define(LInstructionHelper<1, X, Y> *lir, MInstruction *mir,
                       LDefinition::Policy policy = LDefinition::DEFAULT);

    template <size_t X, size_t Y>
    bool defineBox(LInstructionHelper<BOX_PIECES, X, Y> *lir, MInstruction *mir,
                   LDefinition::Policy policy = LDefinition::DEFAULT);

    bool add(LInstruction *ins) {
        current->add(ins);
        return true;
    }

  public:
    bool visitBlock(MBasicBlock *block);

#define VISITMIR(op) bool visit##op(M##op *ins);
    MIR_OPCODE_LIST(VISITMIR)
#undef VISITMIR
};

} // namespace js
} // namespace ion

#endif // jsion_ion_lowering_h__

