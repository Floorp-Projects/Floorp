/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_none_Lowering_none_h
#define jit_none_Lowering_none_h

#include "jit/shared/Lowering-shared.h"

namespace js {
namespace jit {

class LIRGeneratorNone : public LIRGeneratorShared
{
  public:
    LIRGeneratorNone(MIRGenerator *gen, MIRGraph &graph, LIRGraph &lirGraph)
      : LIRGeneratorShared(gen, graph, lirGraph)
    {
        MOZ_CRASH();
    }

    bool useBox(LInstruction *, size_t, MDefinition *,
                LUse::Policy a = LUse::REGISTER, bool b = false) {
        MOZ_CRASH();
    }
    bool useBoxFixed(LInstruction *, size_t, MDefinition *, Register, Register) { MOZ_CRASH(); }

    LAllocation useByteOpRegister(MDefinition *) { MOZ_CRASH(); }
    LAllocation useByteOpRegisterOrNonDoubleConstant(MDefinition *) { MOZ_CRASH(); }
    LDefinition tempToUnbox() { MOZ_CRASH(); }
    bool needTempForPostBarrier() { MOZ_CRASH(); }
    LDefinition tempForDispatchCache(MIRType v = MIRType_None) { MOZ_CRASH(); }
    void lowerUntypedPhiInput(MPhi *, uint32_t, LBlock *, size_t) { MOZ_CRASH(); }
    bool defineUntypedPhi(MPhi *, size_t) { MOZ_CRASH(); }
    bool lowerForShift(LInstructionHelper<1, 2, 0> *, MDefinition *, MDefinition *, MDefinition *) {
        MOZ_CRASH();
    }
    bool lowerUrshD(MUrsh *) { MOZ_CRASH(); }
    template <typename T>
    bool lowerForALU(T, MDefinition *, MDefinition *, MDefinition *v = nullptr) { MOZ_CRASH(); }
    template <typename T>
    bool lowerForFPU(T, MDefinition *, MDefinition *, MDefinition *v = nullptr) { MOZ_CRASH(); }
    bool lowerForBitAndAndBranch(LBitAndAndBranch *, MInstruction *,
                                 MDefinition *, MDefinition *) {
        MOZ_CRASH();
    }

    bool lowerConstantDouble(double, MInstruction *) { MOZ_CRASH(); }
    bool lowerConstantFloat32(float, MInstruction *) { MOZ_CRASH(); }
    bool lowerTruncateDToInt32(MTruncateToInt32 *) { MOZ_CRASH(); }
    bool lowerTruncateFToInt32(MTruncateToInt32 *) { MOZ_CRASH(); }
    bool lowerDivI(MDiv *) { MOZ_CRASH(); }
    bool lowerModI(MMod *) { MOZ_CRASH(); }
    bool lowerMulI(MMul *, MDefinition *, MDefinition *) { MOZ_CRASH(); }
    bool lowerUDiv(MDiv *) { MOZ_CRASH(); }
    bool lowerUMod(MMod *) { MOZ_CRASH(); }
    bool visitBox(MBox *box) { MOZ_CRASH(); }
    bool visitUnbox(MUnbox *unbox) { MOZ_CRASH(); }
    bool visitReturn(MReturn *ret) { MOZ_CRASH(); }
    bool visitPowHalf(MPowHalf *) { MOZ_CRASH(); }
    bool visitAsmJSNeg(MAsmJSNeg *) { MOZ_CRASH(); }
    bool visitGuardShape(MGuardShape *ins) { MOZ_CRASH(); }
    bool visitGuardObjectType(MGuardObjectType *ins) { MOZ_CRASH(); }
    bool visitAsmJSUnsignedToDouble(MAsmJSUnsignedToDouble *ins) { MOZ_CRASH(); }
    bool visitAsmJSUnsignedToFloat32(MAsmJSUnsignedToFloat32 *ins) { MOZ_CRASH(); }
    bool visitAsmJSLoadHeap(MAsmJSLoadHeap *ins) { MOZ_CRASH(); }
    bool visitAsmJSStoreHeap(MAsmJSStoreHeap *ins) { MOZ_CRASH(); }
    bool visitAsmJSLoadFuncPtr(MAsmJSLoadFuncPtr *ins) { MOZ_CRASH(); }
    bool visitStoreTypedArrayElementStatic(MStoreTypedArrayElementStatic *ins) { MOZ_CRASH(); }
    bool visitForkJoinGetSlice(MForkJoinGetSlice *ins) { MOZ_CRASH(); }

    LTableSwitch *newLTableSwitch(LAllocation, LDefinition, MTableSwitch *) { MOZ_CRASH(); }
    LTableSwitchV *newLTableSwitchV(MTableSwitch *) { MOZ_CRASH(); }
    bool visitSimdSplatX4(MSimdSplatX4 *ins) { MOZ_CRASH(); }
};

typedef LIRGeneratorNone LIRGeneratorSpecific;

} // namespace jit
} // namespace js

#endif /* jit_none_Lowering_none_h */
