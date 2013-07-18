/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_Lowering_x86_shared_h
#define jit_shared_Lowering_x86_shared_h

#include "jit/shared/Lowering-shared.h"

namespace js {
namespace jit {

class LIRGeneratorX86Shared : public LIRGeneratorShared
{
  protected:
    LIRGeneratorX86Shared(MIRGenerator *gen, MIRGraph &graph, LIRGraph &lirGraph)
      : LIRGeneratorShared(gen, graph, lirGraph)
    {}

    LTableSwitch *newLTableSwitch(const LAllocation &in, const LDefinition &inputCopy,
                                  MTableSwitch *ins);
    LTableSwitchV *newLTableSwitchV(MTableSwitch *ins);

    bool visitGuardShape(MGuardShape *ins);
    bool visitGuardObjectType(MGuardObjectType *ins);
    bool visitPowHalf(MPowHalf *ins);
    bool lowerForShift(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs,
                       MDefinition *rhs);
    bool lowerForALU(LInstructionHelper<1, 1, 0> *ins, MDefinition *mir, MDefinition *input);
    bool lowerForALU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs,
                     MDefinition *rhs);
    bool lowerForFPU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs,
                     MDefinition *rhs);
    bool lowerForBitAndAndBranch(LBitAndAndBranch *baab, MInstruction *mir,
                                 MDefinition *lhs, MDefinition *rhs);
    bool visitConstant(MConstant *ins);
    bool visitAsmJSNeg(MAsmJSNeg *ins);
    bool visitAsmJSUDiv(MAsmJSUDiv *ins);
    bool visitAsmJSUMod(MAsmJSUMod *ins);
    bool lowerMulI(MMul *mul, MDefinition *lhs, MDefinition *rhs);
    bool lowerDivI(MDiv *div);
    bool lowerModI(MMod *mod);
    bool lowerUDiv(MInstruction *div);
    bool lowerUMod(MInstruction *mod);
    bool lowerUrshD(MUrsh *mir);
    bool lowerConstantDouble(double d, MInstruction *ins);
    bool lowerConstantFloat32(float d, MInstruction *ins);
    bool lowerTruncateDToInt32(MTruncateToInt32 *ins);
};

} // namespace jit
} // namespace js

#endif /* jit_shared_Lowering_x86_shared_h */
