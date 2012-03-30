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
 *   David Anderson <dvander@alliedmods.net>
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

#ifndef jsion_lowering_shared_h__
#define jsion_lowering_shared_h__

// This file declares the structures that are used for attaching LIR to a
// MIRGraph.

#include "ion/IonAllocPolicy.h"
#include "ion/LIR.h"

namespace js {
namespace ion {

class MBasicBlock;
class MTableSwitch;
class MIRGenerator;
class MIRGraph;
class MDefinition;
class MInstruction;
class LOsiPoint;

class LIRGeneratorShared : public MInstructionVisitor
{
  protected:
    MIRGenerator *gen;
    MIRGraph &graph;
    LIRGraph &lirGraph_;
    LBlock *current;
    MResumePoint *lastResumePoint_;
    LOsiPoint *osiPoint_;

  public:
    LIRGeneratorShared(MIRGenerator *gen, MIRGraph &graph, LIRGraph &lirGraph)
      : gen(gen),
        graph(graph),
        lirGraph_(lirGraph),
        lastResumePoint_(NULL),
        osiPoint_(NULL)
    { }

    MIRGenerator *mir() {
        return gen;
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
    inline bool emitAtUses(MInstruction *mir);

    // The lowest-level calls to use, those that do not wrap another call to
    // use(), must prefix grabbing virtual register IDs by these calls.
    inline bool ensureDefined(MDefinition *mir);

    // These all create a use of a virtual register, with an optional
    // allocation policy.
    inline LUse use(MDefinition *mir, LUse policy);
    inline LUse use(MDefinition *mir);
    inline LUse useAtStart(MDefinition *mir);
    inline LUse useRegister(MDefinition *mir);
    inline LUse useRegisterAtStart(MDefinition *mir);
    inline LUse useFixed(MDefinition *mir, Register reg);
    inline LUse useFixed(MDefinition *mir, FloatRegister reg);
    inline LAllocation useOrConstant(MDefinition *mir);
    // "Any" is architecture dependent, and will include registers and stack slots on X86,
    // and only registers on ARM.
    inline LAllocation useAnyOrConstant(MDefinition *mir);
    inline LAllocation useKeepaliveOrConstant(MDefinition *mir);
    inline LAllocation useRegisterOrConstant(MDefinition *mir);
    inline LAllocation useRegisterOrNonDoubleConstant(MDefinition *mir);

#ifdef JS_NUNBOX32
    inline LUse useType(MDefinition *mir, LUse::Policy policy);
    inline LUse usePayload(MDefinition *mir, LUse::Policy policy);
    inline LUse usePayloadAtStart(MDefinition *mir, LUse::Policy policy);
    inline LUse usePayloadInRegisterAtStart(MDefinition *mir);

    // Adds a box input to an instruction, setting operand |n| to the type and
    // |n+1| to the payload. Does not modify the operands, instead expecting a
    // policy to already be set.
    inline bool fillBoxUses(LInstruction *lir, size_t n, MDefinition *mir);
#endif

    // These create temporary register requests.
    inline LDefinition temp(LDefinition::Type type = LDefinition::GENERAL,
                            LDefinition::Policy policy = LDefinition::DEFAULT);
    inline LDefinition tempFloat();
    inline LDefinition tempCopy(MDefinition *input, uint32 reusedInput);

    // Note that the fixed register has a GENERAL type.
    inline LDefinition tempFixed(Register reg);

    template <size_t Ops, size_t Temps>
    inline bool defineFixed(LInstructionHelper<1, Ops, Temps> *lir, MDefinition *mir,
                            const LAllocation &output);

    template <size_t Ops, size_t Temps>
    inline bool defineBox(LInstructionHelper<BOX_PIECES, Ops, Temps> *lir, MDefinition *mir,
                          LDefinition::Policy policy = LDefinition::DEFAULT);

    template <size_t Ops, size_t Temps>
    inline bool defineReturn(LInstructionHelper<BOX_PIECES, Ops, Temps> *lir, MDefinition *mir);

    template <size_t Defs, size_t Ops, size_t Temps>
    inline bool defineVMReturn(LInstructionHelper<Defs, Ops, Temps> *lir, MDefinition *mir);

    template <size_t Ops, size_t Temps>
    inline bool define(LInstructionHelper<1, Ops, Temps> *lir, MDefinition *mir,
                        const LDefinition &def);

    template <size_t Ops, size_t Temps>
    inline bool define(LInstructionHelper<1, Ops, Temps> *lir, MDefinition *mir,
                       LDefinition::Policy policy = LDefinition::DEFAULT);

    template <size_t Ops, size_t Temps>
    inline bool defineReuseInput(LInstructionHelper<1, Ops, Temps> *lir, MDefinition *mir, uint32 operand);

    // Rather than defining a new virtual register, sets |ins| to have the same
    // virtual register as |as|.
    inline bool redefine(MDefinition *ins, MDefinition *as);

    // Defines an IR's output as the same as another IR. This is similar to
    // redefine(), but used when creating new LIR.
    inline bool defineAs(LInstruction *outLir, MDefinition *outMir, MDefinition *inMir);

    uint32 getVirtualRegister() {
        return lirGraph_.getVirtualRegister();
    }

    template <typename T> void annotate(T *ins);
    template <typename T> bool add(T *ins, MInstruction *mir = NULL);

    void lowerTypedPhiInput(MPhi *phi, uint32 inputPosition, LBlock *block, size_t lirIndex);
    bool defineTypedPhi(MPhi *phi, size_t lirIndex);

    LOsiPoint *popOsiPoint() {
        LOsiPoint *tmp = osiPoint_;
        osiPoint_ = NULL;
        return tmp;
    }

    LSnapshot *buildSnapshot(LInstruction *ins, MResumePoint *rp, BailoutKind kind);
    bool assignPostSnapshot(MInstruction *mir, LInstruction *ins);

    // Marks this instruction as fallible, meaning that before it performs
    // effects (if any), it may check pre-conditions and bailout if they do not
    // hold. This function informs the register allocator that it will need to
    // capture appropriate state.
    bool assignSnapshot(LInstruction *ins, BailoutKind kind = Bailout_Normal);

    // Marks this instruction as needing to call into either the VM or GC. This
    // function may build a snapshot that captures the result of its own
    // instruction, and as such, should generally be called after define*().
    bool assignSafepoint(LInstruction *ins, MInstruction *mir);

  public:
    bool visitConstant(MConstant *ins);
};

} // namespace ion
} // namespace js

#endif // jsion_lowering_shared_h__


