/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm_Lowering_arm_h
#define jit_arm_Lowering_arm_h

#include "jit/shared/Lowering-shared.h"

namespace js {
namespace ion {

class LIRGeneratorARM : public LIRGeneratorShared
{
  public:
    LIRGeneratorARM(MIRGenerator *gen, MIRGraph &graph, LIRGraph &lirGraph)
      : LIRGeneratorShared(gen, graph, lirGraph)
    { }

  protected:
    // Adds a box input to an instruction, setting operand |n| to the type and
    // |n+1| to the payload.
    bool useBox(LInstruction *lir, size_t n, MDefinition *mir,
                LUse::Policy policy = LUse::REGISTER, bool useAtStart = false);
    bool useBoxFixed(LInstruction *lir, size_t n, MDefinition *mir, Register reg1, Register reg2);

    inline LDefinition tempToUnbox() {
        return LDefinition::BogusTemp();
    }

    void lowerUntypedPhiInput(MPhi *phi, uint32_t inputPosition, LBlock *block, size_t lirIndex);
    bool defineUntypedPhi(MPhi *phi, size_t lirIndex);
    bool lowerForShift(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs,
                       MDefinition *rhs);
    bool lowerUrshD(MUrsh *mir);

    bool lowerForALU(LInstructionHelper<1, 1, 0> *ins, MDefinition *mir,
                     MDefinition *input);
    bool lowerForALU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir,
                     MDefinition *lhs, MDefinition *rhs);

    bool lowerForFPU(LInstructionHelper<1, 1, 0> *ins, MDefinition *mir,
                     MDefinition *src);
    bool lowerForFPU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir,
                     MDefinition *lhs, MDefinition *rhs);
    bool lowerForBitAndAndBranch(LBitAndAndBranch *baab, MInstruction *mir,
                                 MDefinition *lhs, MDefinition *rhs);

    bool lowerTruncateDToInt32(MTruncateToInt32 *ins);

    bool lowerConstantDouble(double d, MInstruction *ins);
    bool lowerDivI(MDiv *div);
    bool lowerModI(MMod *mod);
    bool lowerMulI(MMul *mul, MDefinition *lhs, MDefinition *rhs);
    bool lowerUDiv(MInstruction *div);
    bool lowerUMod(MInstruction *mod);
    bool visitPowHalf(MPowHalf *ins);
    bool visitAsmJSNeg(MAsmJSNeg *ins);
    bool visitAsmJSUDiv(MAsmJSUDiv *ins);
    bool visitAsmJSUMod(MAsmJSUMod *ins);

    LTableSwitch *newLTableSwitch(const LAllocation &in, const LDefinition &inputCopy,
                                  MTableSwitch *ins);
    LTableSwitchV *newLTableSwitchV(MTableSwitch *ins);
    LGetPropertyCacheT *newLGetPropertyCacheT(MGetPropertyCache *ins);
    LGetElementCacheT *newLGetElementCacheT(MGetElementCache *ins);

  public:
    bool visitConstant(MConstant *ins);
    bool visitBox(MBox *box);
    bool visitUnbox(MUnbox *unbox);
    bool visitReturn(MReturn *ret);
    bool lowerPhi(MPhi *phi);
    bool visitGuardShape(MGuardShape *ins);
    bool visitGuardObjectType(MGuardObjectType *ins);
    bool visitStoreTypedArrayElement(MStoreTypedArrayElement *ins);
    bool visitStoreTypedArrayElementHole(MStoreTypedArrayElementHole *ins);
    bool visitAsmJSUnsignedToDouble(MAsmJSUnsignedToDouble *ins);
    bool visitAsmJSStoreHeap(MAsmJSStoreHeap *ins);
    bool visitAsmJSLoadFuncPtr(MAsmJSLoadFuncPtr *ins);
    bool visitStoreTypedArrayElementStatic(MStoreTypedArrayElementStatic *ins);
};

typedef LIRGeneratorARM LIRGeneratorSpecific;

} // namespace ion
} // namespace js

#endif /* jit_arm_Lowering_arm_h */
