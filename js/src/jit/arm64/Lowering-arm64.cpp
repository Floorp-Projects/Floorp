/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/arm64/Lowering-arm64.h"

#include "jit/arm64/Assembler-arm64.h"
#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::FloorLog2;

LBoxAllocation
LIRGeneratorARM64::useBoxFixed(MDefinition* mir, Register reg1, Register, bool useAtStart)
{
    MOZ_CRASH("useBoxFixed");
}

LAllocation
LIRGeneratorARM64::useByteOpRegister(MDefinition* mir)
{
    MOZ_CRASH("useByteOpRegister");
}

LAllocation
LIRGeneratorARM64::useByteOpRegisterAtStart(MDefinition* mir)
{
    MOZ_CRASH("useByteOpRegister");
}

LAllocation
LIRGeneratorARM64::useByteOpRegisterOrNonDoubleConstant(MDefinition* mir)
{
    MOZ_CRASH("useByteOpRegisterOrNonDoubleConstant");
}

void
LIRGenerator::visitBox(MBox* box)
{
    MDefinition* opd = box->getOperand(0);

    // If the operand is a constant, emit near its uses.
    if (opd->isConstant() && box->canEmitAtUses()) {
        emitAtUses(box);
        return;
    }

    if (opd->isConstant()) {
        define(new(alloc()) LValue(opd->toConstant()->toJSValue()), box, LDefinition(LDefinition::BOX));
    } else {
        LBox* ins = new(alloc()) LBox(useRegister(opd), opd->type());
        define(ins, box, LDefinition(LDefinition::BOX));
    }
}

void
LIRGenerator::visitUnbox(MUnbox* unbox)
{
    MDefinition* box = unbox->getOperand(0);

    if (box->type() == MIRType::ObjectOrNull) {
        LUnboxObjectOrNull* lir = new(alloc()) LUnboxObjectOrNull(useRegisterAtStart(box));
        if (unbox->fallible())
            assignSnapshot(lir, unbox->bailoutKind());
        defineReuseInput(lir, unbox, 0);
        return;
    }

    MOZ_ASSERT(box->type() == MIRType::Value);

    LUnboxBase* lir;
    if (IsFloatingPointType(unbox->type())) {
        lir = new(alloc()) LUnboxFloatingPoint(useRegisterAtStart(box), unbox->type());
    } else if (unbox->fallible()) {
        // If the unbox is fallible, load the Value in a register first to
        // avoid multiple loads.
        lir = new(alloc()) LUnbox(useRegisterAtStart(box));
    } else {
        lir = new(alloc()) LUnbox(useAtStart(box));
    }

    if (unbox->fallible())
        assignSnapshot(lir, unbox->bailoutKind());

    define(lir, unbox);
}

void
LIRGenerator::visitReturn(MReturn* ret)
{
    MDefinition* opd = ret->getOperand(0);
    MOZ_ASSERT(opd->type() == MIRType::Value);

    LReturn* ins = new(alloc()) LReturn;
    ins->setOperand(0, useFixed(opd, JSReturnReg));
    add(ins);
}

// x = !y
void
LIRGeneratorARM64::lowerForALU(LInstructionHelper<1, 1, 0>* ins, MDefinition* mir, MDefinition* input)
{
    ins->setOperand(0, ins->snapshot() ? useRegister(input) : useRegisterAtStart(input));
    define(ins, mir, LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

// z = x+y
void
LIRGeneratorARM64::lowerForALU(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir,
                               MDefinition* lhs, MDefinition* rhs)
{
    ins->setOperand(0, ins->snapshot() ? useRegister(lhs) : useRegisterAtStart(lhs));
    ins->setOperand(1, ins->snapshot() ? useRegisterOrConstant(rhs) :
                                         useRegisterOrConstantAtStart(rhs));
    define(ins, mir, LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::REGISTER));
}

void
LIRGeneratorARM64::lowerForFPU(LInstructionHelper<1, 1, 0>* ins, MDefinition* mir, MDefinition* input)
{
    MOZ_CRASH("lowerForFPU");
}

template <size_t Temps>
void
LIRGeneratorARM64::lowerForFPU(LInstructionHelper<1, 2, Temps>* ins, MDefinition* mir,
                               MDefinition* lhs, MDefinition* rhs)
{
    MOZ_CRASH("lowerForFPU");
}

template void LIRGeneratorARM64::lowerForFPU(LInstructionHelper<1, 2, 0>* ins, MDefinition* mir,
                                             MDefinition* lhs, MDefinition* rhs);
template void LIRGeneratorARM64::lowerForFPU(LInstructionHelper<1, 2, 1>* ins, MDefinition* mir,
                                             MDefinition* lhs, MDefinition* rhs);

void
LIRGeneratorARM64::lowerForALUInt64(LInstructionHelper<INT64_PIECES, 2 * INT64_PIECES, 0>* ins,
                                    MDefinition* mir, MDefinition* lhs, MDefinition* rhs)
{
    MOZ_CRASH("NYI");
}

void
LIRGeneratorARM64::lowerForMulInt64(LMulI64* ins, MMul* mir, MDefinition* lhs, MDefinition* rhs)
{
    MOZ_CRASH("NYI");
}

template<size_t Temps>
void
LIRGeneratorARM64::lowerForShiftInt64(LInstructionHelper<INT64_PIECES, INT64_PIECES + 1, Temps>* ins,
                                      MDefinition* mir, MDefinition* lhs, MDefinition* rhs)
{
    MOZ_CRASH("NYI");
}

template void LIRGeneratorARM64::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES+1, 0>* ins, MDefinition* mir,
    MDefinition* lhs, MDefinition* rhs);
template void LIRGeneratorARM64::lowerForShiftInt64(
    LInstructionHelper<INT64_PIECES, INT64_PIECES+1, 1>* ins, MDefinition* mir,
    MDefinition* lhs, MDefinition* rhs);

void
LIRGeneratorARM64::lowerForBitAndAndBranch(LBitAndAndBranch* baab, MInstruction* mir,
                                         MDefinition* lhs, MDefinition* rhs)
{
    MOZ_CRASH("lowerForBitAndAndBranch");
}

void
LIRGeneratorARM64::defineUntypedPhi(MPhi* phi, size_t lirIndex)
{
    MOZ_CRASH("defineUntypedPhi");
}

void
LIRGeneratorARM64::lowerUntypedPhiInput(MPhi* phi, uint32_t inputPosition,
                                        LBlock* block, size_t lirIndex)
{
    MOZ_CRASH("lowerUntypedPhiInput");
}

void
LIRGeneratorARM64::lowerForShift(LInstructionHelper<1, 2, 0>* ins,
                                 MDefinition* mir, MDefinition* lhs, MDefinition* rhs)
{
    MOZ_CRASH("lowerForShift");
}

void
LIRGeneratorARM64::lowerDivI(MDiv* div)
{
    MOZ_CRASH("lowerDivI");
}

void
LIRGeneratorARM64::lowerMulI(MMul* mul, MDefinition* lhs, MDefinition* rhs)
{
    MOZ_CRASH("lowerMulI");
}

void
LIRGeneratorARM64::lowerModI(MMod* mod)
{
    MOZ_CRASH("lowerModI");
}

void
LIRGeneratorARM64::lowerDivI64(MDiv* div)
{
    MOZ_CRASH("NYI");
}

void
LIRGeneratorARM64::lowerModI64(MMod* mod)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitPowHalf(MPowHalf* ins)
{
    MOZ_CRASH("visitPowHalf");
}

LTableSwitch*
LIRGeneratorARM64::newLTableSwitch(const LAllocation& in, const LDefinition& inputCopy,
                                       MTableSwitch* tableswitch)
{
    MOZ_CRASH("newLTableSwitch");
}

LTableSwitchV*
LIRGeneratorARM64::newLTableSwitchV(MTableSwitch* tableswitch)
{
    MOZ_CRASH("newLTableSwitchV");
}

void
LIRGeneratorARM64::lowerUrshD(MUrsh* mir)
{
    MOZ_CRASH("lowerUrshD");
}

void
LIRGenerator::visitWasmNeg(MWasmNeg* ins)
{
    MOZ_CRASH("visitWasmNeg");
}

void
LIRGenerator::visitWasmSelect(MWasmSelect* ins)
{
    MOZ_CRASH("visitWasmSelect");
}

void
LIRGeneratorARM64::lowerUDiv(MDiv* div)
{
    MOZ_CRASH("lowerUDiv");
}

void
LIRGeneratorARM64::lowerUMod(MMod* mod)
{
    MOZ_CRASH("lowerUMod");
}

void
LIRGenerator::visitWasmUnsignedToDouble(MWasmUnsignedToDouble* ins)
{
    MOZ_CRASH("visitWasmUnsignedToDouble");
}

void
LIRGenerator::visitWasmUnsignedToFloat32(MWasmUnsignedToFloat32* ins)
{
    MOZ_CRASH("visitWasmUnsignedToFloat32");
}

void
LIRGenerator::visitAsmJSLoadHeap(MAsmJSLoadHeap* ins)
{
    MOZ_CRASH("visitAsmJSLoadHeap");
}

void
LIRGenerator::visitAsmJSStoreHeap(MAsmJSStoreHeap* ins)
{
    MOZ_CRASH("visitAsmJSStoreHeap");
}

void
LIRGenerator::visitWasmCompareExchangeHeap(MWasmCompareExchangeHeap* ins)
{
    MOZ_CRASH("visitWasmCompareExchangeHeap");
}

void
LIRGenerator::visitWasmAtomicExchangeHeap(MWasmAtomicExchangeHeap* ins)
{
    MOZ_CRASH("visitWasmAtomicExchangeHeap");
}

void
LIRGenerator::visitWasmAtomicBinopHeap(MWasmAtomicBinopHeap* ins)
{
    MOZ_CRASH("visitWasmAtomicBinopHeap");
}

void
LIRGeneratorARM64::lowerTruncateDToInt32(MTruncateToInt32* ins)
{
    MOZ_CRASH("lowerTruncateDToInt32");
}

void
LIRGeneratorARM64::lowerTruncateFToInt32(MTruncateToInt32* ins)
{
    MOZ_CRASH("lowerTruncateFToInt32");
}

void
LIRGenerator::visitAtomicTypedArrayElementBinop(MAtomicTypedArrayElementBinop* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitCompareExchangeTypedArrayElement(MCompareExchangeTypedArrayElement* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitAtomicExchangeTypedArrayElement(MAtomicExchangeTypedArrayElement* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSubstr(MSubstr* ins)
{
    MOZ_CRASH("visitSubstr");
}

void
LIRGenerator::visitRandom(MRandom* ins)
{
    LRandom *lir = new(alloc()) LRandom(temp(),
                                        temp(),
                                        temp());
    defineFixed(lir, ins, LFloatReg(ReturnDoubleReg));
}

void
LIRGenerator::visitWasmTruncateToInt64(MWasmTruncateToInt64* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitWasmLoad(MWasmLoad* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitWasmStore(MWasmStore* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitInt64ToFloatingPoint(MInt64ToFloatingPoint* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitCopySign(MCopySign* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitExtendInt32ToInt64(MExtendInt32ToInt64* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSignExtendInt64(MSignExtendInt64* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdInsertElement(MSimdInsertElement*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdExtractElement(MSimdExtractElement*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdBinaryArith(MSimdBinaryArith*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdSelect(MSimdSelect*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdSplat(MSimdSplat*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdValueX4(MSimdValueX4*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdBinarySaturating(MSimdBinarySaturating*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdSwizzle(MSimdSwizzle*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdShuffle(MSimdShuffle*)
{
    MOZ_CRASH("NYI");
}

void
LIRGenerator::visitSimdGeneralShuffle(MSimdGeneralShuffle*)
{
    MOZ_CRASH("NYI");
}
