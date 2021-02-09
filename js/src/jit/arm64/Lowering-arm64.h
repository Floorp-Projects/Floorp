/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_arm64_Lowering_arm64_h
#define jit_arm64_Lowering_arm64_h

#include "jit/shared/Lowering-shared.h"

namespace js {
namespace jit {

class LIRGeneratorARM64 : public LIRGeneratorShared {
 protected:
  LIRGeneratorARM64(MIRGenerator* gen, MIRGraph& graph, LIRGraph& lirGraph)
      : LIRGeneratorShared(gen, graph, lirGraph) {}

  // Returns a box allocation. reg2 is ignored on 64-bit platforms.
  LBoxAllocation useBoxFixed(MDefinition* mir, Register reg1, Register reg2,
                             bool useAtStart = false);

  LAllocation useByteOpRegister(MDefinition* mir);
  LAllocation useByteOpRegisterAtStart(MDefinition* mir);
  LAllocation useByteOpRegisterOrNonDoubleConstant(MDefinition* mir);

  inline LDefinition tempToUnbox() { return temp(); }

  bool needTempForPostBarrier() { return true; }

  // ARM64 has a scratch register, so no need for another temp for dispatch ICs.
  LDefinition tempForDispatchCache(MIRType outputType = MIRType::None) {
    return LDefinition::BogusTemp();
  }

  void lowerUntypedPhiInput(MPhi* phi, uint32_t inputPosition, LBlock* block,
                            size_t lirIndex);
  void lowerInt64PhiInput(MPhi*, uint32_t, LBlock*, size_t) {
    MOZ_CRASH("NYI");
  }
  void defineInt64Phi(MPhi*, size_t) { MOZ_CRASH("NYI"); }
  void lowerForShift(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir,
                     MDefinition* lhs, MDefinition* rhs);
  void lowerUrshD(MUrsh* mir);

  void lowerPowOfTwoI(MPow* mir);

  void lowerForALU(LInstructionHelper<1, 1, 0>* ins, MDefinition* mir,
                   MDefinition* input);
  void lowerForALU(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir,
                   MDefinition* lhs, MDefinition* rhs);

  void lowerForALUInt64(
      LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0>* ins,
      MDefinition* mir, MDefinition* lhs, MDefinition* rhs);
  void lowerForMulInt64(LMulI64* ins, MMul* mir, MDefinition* lhs,
                        MDefinition* rhs);
  template <size_t Temps>
  void lowerForShiftInt64(
      LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, Temps>* ins,
      MDefinition* mir, MDefinition* lhs, MDefinition* rhs);

  void lowerForFPU(LInstructionHelper<1, 1, 0>* ins, MDefinition* mir,
                   MDefinition* input);

  template <size_t Temps>
  void lowerForFPU(LInstructionHelper<1, 2, Temps>* ins, MDefinition* mir,
                   MDefinition* lhs, MDefinition* rhs);

  void lowerBuiltinInt64ToFloatingPoint(MBuiltinInt64ToFloatingPoint* ins);
  void lowerWasmBuiltinTruncateToInt64(MWasmBuiltinTruncateToInt64* ins);
  void lowerForBitAndAndBranch(LBitAndAndBranch* baab, MInstruction* mir,
                               MDefinition* lhs, MDefinition* rhs);
  void lowerWasmBuiltinTruncateToInt32(MWasmBuiltinTruncateToInt32* ins);
  void lowerTruncateDToInt32(MTruncateToInt32* ins);
  void lowerTruncateFToInt32(MTruncateToInt32* ins);
  void lowerDivI(MDiv* div);
  void lowerModI(MMod* mod);
  void lowerDivI64(MDiv* div);
  void lowerWasmBuiltinDivI64(MWasmBuiltinDivI64* div);
  void lowerModI64(MMod* mod);
  void lowerWasmBuiltinModI64(MWasmBuiltinModI64* mod);
  void lowerMulI(MMul* mul, MDefinition* lhs, MDefinition* rhs);
  void lowerUDiv(MDiv* div);
  void lowerUMod(MMod* mod);

  void lowerBigIntLsh(MBigIntLsh* ins);
  void lowerBigIntRsh(MBigIntRsh* ins);
  void lowerBigIntDiv(MBigIntDiv* ins);
  void lowerBigIntMod(MBigIntMod* ins);

  void lowerAtomicLoad64(MLoadUnboxedScalar* ins);
  void lowerAtomicStore64(MStoreUnboxedScalar* ins);

  LTableSwitchV* newLTableSwitchV(MTableSwitch* ins);
  LTableSwitch* newLTableSwitch(const LAllocation& in,
                                const LDefinition& inputCopy,
                                MTableSwitch* ins);

  void lowerPhi(MPhi* phi);
};

typedef LIRGeneratorARM64 LIRGeneratorSpecific;

}  // namespace jit
}  // namespace js

#endif /* jit_arm64_Lowering_arm64_h */
