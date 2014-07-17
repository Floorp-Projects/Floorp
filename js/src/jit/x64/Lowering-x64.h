/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_x64_Lowering_x64_h
#define jit_x64_Lowering_x64_h

#include "jit/shared/Lowering-x86-shared.h"

namespace js {
namespace jit {

class LIRGeneratorX64 : public LIRGeneratorX86Shared
{
  public:
    LIRGeneratorX64(MIRGenerator *gen, MIRGraph &graph, LIRGraph &lirGraph)
      : LIRGeneratorX86Shared(gen, graph, lirGraph)
    { }

  protected:
    void lowerUntypedPhiInput(MPhi *phi, uint32_t inputPosition, LBlock *block, size_t lirIndex);
    bool defineUntypedPhi(MPhi *phi, size_t lirIndex);

    // Adds a use at operand |n| of a value-typed insturction.
    bool useBox(LInstruction *lir, size_t n, MDefinition *mir,
                LUse::Policy policy = LUse::REGISTER, bool useAtStart = false);
    bool useBoxFixed(LInstruction *lir, size_t n, MDefinition *mir, Register reg1, Register);

    // x86 has constraints on what registers can be formatted for 1-byte
    // stores and loads; on x64 all registers are okay.
    LAllocation useByteOpRegister(MDefinition *mir);
    LAllocation useByteOpRegisterOrNonDoubleConstant(MDefinition *mir);

    LDefinition tempToUnbox();

    bool needTempForPostBarrier() { return false; }

    // x64 has a scratch register, so no need for another temp for dispatch
    // ICs.
    LDefinition tempForDispatchCache(MIRType outputType = MIRType_None) {
        return LDefinition::BogusTemp();
    }

  public:
    bool visitBox(MBox *box);
    bool visitUnbox(MUnbox *unbox);
    bool visitReturn(MReturn *ret);
    bool visitAsmJSUnsignedToDouble(MAsmJSUnsignedToDouble *ins);
    bool visitAsmJSUnsignedToFloat32(MAsmJSUnsignedToFloat32 *ins);
    bool visitAsmJSLoadHeap(MAsmJSLoadHeap *ins);
    bool visitAsmJSStoreHeap(MAsmJSStoreHeap *ins);
    bool visitAsmJSLoadFuncPtr(MAsmJSLoadFuncPtr *ins);
    bool visitStoreTypedArrayElementStatic(MStoreTypedArrayElementStatic *ins);

    static bool allowInlineForkJoinGetSlice() {
        return true;
    }
};

typedef LIRGeneratorX64 LIRGeneratorSpecific;

} // namespace jit
} // namespace js

#endif /* jit_x64_Lowering_x64_h */
