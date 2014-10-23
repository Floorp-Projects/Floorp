/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/Lowering-x86-shared.h"

#include "mozilla/MathAlgorithms.h"

#include "jit/MIR.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::Abs;
using mozilla::FloorLog2;
using mozilla::Swap;

LTableSwitch *
LIRGeneratorX86Shared::newLTableSwitch(const LAllocation &in, const LDefinition &inputCopy,
                                       MTableSwitch *tableswitch)
{
    return new(alloc()) LTableSwitch(in, inputCopy, temp(), tableswitch);
}

LTableSwitchV *
LIRGeneratorX86Shared::newLTableSwitchV(MTableSwitch *tableswitch)
{
    return new(alloc()) LTableSwitchV(temp(), tempDouble(), temp(), tableswitch);
}

bool
LIRGeneratorX86Shared::visitGuardShape(MGuardShape *ins)
{
    MOZ_ASSERT(ins->obj()->type() == MIRType_Object);

    LGuardShape *guard = new(alloc()) LGuardShape(useRegisterAtStart(ins->obj()));
    if (!assignSnapshot(guard, ins->bailoutKind()))
        return false;
    if (!add(guard, ins))
        return false;
    return redefine(ins, ins->obj());
}

bool
LIRGeneratorX86Shared::visitGuardObjectType(MGuardObjectType *ins)
{
    MOZ_ASSERT(ins->obj()->type() == MIRType_Object);

    LGuardObjectType *guard = new(alloc()) LGuardObjectType(useRegisterAtStart(ins->obj()));
    if (!assignSnapshot(guard, Bailout_ObjectIdentityOrTypeGuard))
        return false;
    if (!add(guard, ins))
        return false;
    return redefine(ins, ins->obj());
}

bool
LIRGeneratorX86Shared::visitPowHalf(MPowHalf *ins)
{
    MDefinition *input = ins->input();
    MOZ_ASSERT(input->type() == MIRType_Double);
    LPowHalfD *lir = new(alloc()) LPowHalfD(useRegisterAtStart(input));
    return defineReuseInput(lir, ins, 0);
}

bool
LIRGeneratorX86Shared::lowerForShift(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir,
                                     MDefinition *lhs, MDefinition *rhs)
{
    ins->setOperand(0, useRegisterAtStart(lhs));

    // shift operator should be constant or in register ecx
    // x86 can't shift a non-ecx register
    if (rhs->isConstant())
        ins->setOperand(1, useOrConstantAtStart(rhs));
    else
        ins->setOperand(1, lhs != rhs ? useFixed(rhs, ecx) : useFixedAtStart(rhs, ecx));

    return defineReuseInput(ins, mir, 0);
}

bool
LIRGeneratorX86Shared::lowerForALU(LInstructionHelper<1, 1, 0> *ins, MDefinition *mir,
                                   MDefinition *input)
{
    ins->setOperand(0, useRegisterAtStart(input));
    return defineReuseInput(ins, mir, 0);
}

bool
LIRGeneratorX86Shared::lowerForALU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir,
                                   MDefinition *lhs, MDefinition *rhs)
{
    ins->setOperand(0, useRegisterAtStart(lhs));
    ins->setOperand(1, lhs != rhs ? useOrConstant(rhs) : useOrConstantAtStart(rhs));
    return defineReuseInput(ins, mir, 0);
}

bool
LIRGeneratorX86Shared::lowerForFPU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs, MDefinition *rhs)
{
    ins->setOperand(0, useRegisterAtStart(lhs));
    ins->setOperand(1, lhs != rhs ? use(rhs) : useAtStart(rhs));
    return defineReuseInput(ins, mir, 0);
}

bool
LIRGeneratorX86Shared::lowerForCompIx4(LSimdBinaryCompIx4 *ins, MSimdBinaryComp *mir, MDefinition *lhs, MDefinition *rhs)
{
    return lowerForALU(ins, mir, lhs, rhs);
}

bool
LIRGeneratorX86Shared::lowerForCompFx4(LSimdBinaryCompFx4 *ins, MSimdBinaryComp *mir, MDefinition *lhs, MDefinition *rhs)
{
    // Swap the operands around to fit the instructions that x86 actually has.
    // We do this here, before register allocation, so that we don't need
    // temporaries and copying afterwards.
    switch (mir->operation()) {
      case MSimdBinaryComp::greaterThan:
      case MSimdBinaryComp::greaterThanOrEqual:
        mir->reverse();
        Swap(lhs, rhs);
        break;
      default:
        break;
    }

    return lowerForFPU(ins, mir, lhs, rhs);
}

bool
LIRGeneratorX86Shared::lowerForBitAndAndBranch(LBitAndAndBranch *baab, MInstruction *mir,
                                               MDefinition *lhs, MDefinition *rhs)
{
    baab->setOperand(0, useRegisterAtStart(lhs));
    baab->setOperand(1, useRegisterOrConstantAtStart(rhs));
    return add(baab, mir);
}

bool
LIRGeneratorX86Shared::lowerMulI(MMul *mul, MDefinition *lhs, MDefinition *rhs)
{
    // Note: lhs is used twice, so that we can restore the original value for the
    // negative zero check.
    LMulI *lir = new(alloc()) LMulI(useRegisterAtStart(lhs), useOrConstant(rhs), use(lhs));
    if (mul->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
        return false;
    return defineReuseInput(lir, mul, 0);
}

bool
LIRGeneratorX86Shared::lowerDivI(MDiv *div)
{
    if (div->isUnsigned())
        return lowerUDiv(div);

    // Division instructions are slow. Division by constant denominators can be
    // rewritten to use other instructions.
    if (div->rhs()->isConstant()) {
        int32_t rhs = div->rhs()->toConstant()->value().toInt32();

        // Division by powers of two can be done by shifting, and division by
        // other numbers can be done by a reciprocal multiplication technique.
        int32_t shift = FloorLog2(Abs(rhs));
        if (rhs != 0 && uint32_t(1) << shift == Abs(rhs)) {
            LAllocation lhs = useRegisterAtStart(div->lhs());
            LDivPowTwoI *lir;
            if (!div->canBeNegativeDividend()) {
                // Numerator is unsigned, so does not need adjusting.
                lir = new(alloc()) LDivPowTwoI(lhs, lhs, shift, rhs < 0);
            } else {
                // Numerator is signed, and needs adjusting, and an extra
                // lhs copy register is needed.
                lir = new(alloc()) LDivPowTwoI(lhs, useRegister(div->lhs()), shift, rhs < 0);
            }
            if (div->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
                return false;
            return defineReuseInput(lir, div, 0);
        } else if (rhs != 0 &&
                   gen->optimizationInfo().registerAllocator() != RegisterAllocator_LSRA)
        {
            LDivOrModConstantI *lir;
            lir = new(alloc()) LDivOrModConstantI(useRegister(div->lhs()), rhs, tempFixed(eax));
            if (div->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
                return false;
            return defineFixed(lir, div, LAllocation(AnyRegister(edx)));
        }
    }

    LDivI *lir = new(alloc()) LDivI(useRegister(div->lhs()), useRegister(div->rhs()),
                                    tempFixed(edx));
    if (div->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
        return false;
    return defineFixed(lir, div, LAllocation(AnyRegister(eax)));
}

bool
LIRGeneratorX86Shared::lowerModI(MMod *mod)
{
    if (mod->isUnsigned())
        return lowerUMod(mod);

    if (mod->rhs()->isConstant()) {
        int32_t rhs = mod->rhs()->toConstant()->value().toInt32();
        int32_t shift = FloorLog2(Abs(rhs));
        if (rhs != 0 && uint32_t(1) << shift == Abs(rhs)) {
            LModPowTwoI *lir = new(alloc()) LModPowTwoI(useRegisterAtStart(mod->lhs()), shift);
            if (mod->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
                return false;
            return defineReuseInput(lir, mod, 0);
        } else if (rhs != 0 &&
                   gen->optimizationInfo().registerAllocator() != RegisterAllocator_LSRA)
        {
            LDivOrModConstantI *lir;
            lir = new(alloc()) LDivOrModConstantI(useRegister(mod->lhs()), rhs, tempFixed(edx));
            if (mod->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
                return false;
            return defineFixed(lir, mod, LAllocation(AnyRegister(eax)));
        }
    }

    LModI *lir = new(alloc()) LModI(useRegister(mod->lhs()),
                                    useRegister(mod->rhs()),
                                    tempFixed(eax));
    if (mod->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
        return false;
    return defineFixed(lir, mod, LAllocation(AnyRegister(edx)));
}

bool
LIRGeneratorX86Shared::visitAsmJSNeg(MAsmJSNeg *ins)
{
    if (ins->type() == MIRType_Int32)
        return defineReuseInput(new(alloc()) LNegI(useRegisterAtStart(ins->input())), ins, 0);

    if (ins->type() == MIRType_Float32)
        return defineReuseInput(new(alloc()) LNegF(useRegisterAtStart(ins->input())), ins, 0);

    MOZ_ASSERT(ins->type() == MIRType_Double);
    return defineReuseInput(new(alloc()) LNegD(useRegisterAtStart(ins->input())), ins, 0);
}

bool
LIRGeneratorX86Shared::lowerUDiv(MDiv *div)
{
    LUDivOrMod *lir = new(alloc()) LUDivOrMod(useRegister(div->lhs()),
                                              useRegister(div->rhs()),
                                              tempFixed(edx));
    if (div->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
        return false;
    return defineFixed(lir, div, LAllocation(AnyRegister(eax)));
}

bool
LIRGeneratorX86Shared::lowerUMod(MMod *mod)
{
    LUDivOrMod *lir = new(alloc()) LUDivOrMod(useRegister(mod->lhs()),
                                              useRegister(mod->rhs()),
                                              tempFixed(eax));
    if (mod->fallible() && !assignSnapshot(lir, Bailout_DoubleOutput))
        return false;
    return defineFixed(lir, mod, LAllocation(AnyRegister(edx)));
}

bool
LIRGeneratorX86Shared::lowerUrshD(MUrsh *mir)
{
    MDefinition *lhs = mir->lhs();
    MDefinition *rhs = mir->rhs();

    MOZ_ASSERT(lhs->type() == MIRType_Int32);
    MOZ_ASSERT(rhs->type() == MIRType_Int32);
    MOZ_ASSERT(mir->type() == MIRType_Double);

#ifdef JS_CODEGEN_X64
    MOZ_ASSERT(ecx == rcx);
#endif

    LUse lhsUse = useRegisterAtStart(lhs);
    LAllocation rhsAlloc = rhs->isConstant() ? useOrConstant(rhs) : useFixed(rhs, ecx);

    LUrshD *lir = new(alloc()) LUrshD(lhsUse, rhsAlloc, tempCopy(lhs, 0));
    return define(lir, mir);
}

bool
LIRGeneratorX86Shared::lowerConstantDouble(double d, MInstruction *mir)
{
    return define(new(alloc()) LDouble(d), mir);
}

bool
LIRGeneratorX86Shared::lowerConstantFloat32(float f, MInstruction *mir)
{
    return define(new(alloc()) LFloat32(f), mir);
}

bool
LIRGeneratorX86Shared::visitConstant(MConstant *ins)
{
    if (ins->type() == MIRType_Double)
        return lowerConstantDouble(ins->value().toDouble(), ins);

    if (ins->type() == MIRType_Float32)
        return lowerConstantFloat32(ins->value().toDouble(), ins);

    // Emit non-double constants at their uses.
    if (ins->canEmitAtUses())
        return emitAtUses(ins);

    return LIRGeneratorShared::visitConstant(ins);
}

bool
LIRGeneratorX86Shared::lowerTruncateDToInt32(MTruncateToInt32 *ins)
{
    MDefinition *opd = ins->input();
    MOZ_ASSERT(opd->type() == MIRType_Double);

    LDefinition maybeTemp = Assembler::HasSSE3() ? LDefinition::BogusTemp() : tempDouble();
    return define(new(alloc()) LTruncateDToInt32(useRegister(opd), maybeTemp), ins);
}

bool
LIRGeneratorX86Shared::lowerTruncateFToInt32(MTruncateToInt32 *ins)
{
    MDefinition *opd = ins->input();
    MOZ_ASSERT(opd->type() == MIRType_Float32);

    LDefinition maybeTemp = Assembler::HasSSE3() ? LDefinition::BogusTemp() : tempFloat32();
    return define(new(alloc()) LTruncateFToInt32(useRegister(opd), maybeTemp), ins);
}

bool
LIRGeneratorX86Shared::visitForkJoinGetSlice(MForkJoinGetSlice *ins)
{
    // We fix eax and edx for cmpxchg and div.
    LForkJoinGetSlice *lir = new(alloc())
        LForkJoinGetSlice(useFixed(ins->forkJoinContext(), ForkJoinGetSliceReg_cx),
                          tempFixed(eax),
                          tempFixed(edx),
                          tempFixed(ForkJoinGetSliceReg_temp0),
                          tempFixed(ForkJoinGetSliceReg_temp1));
    return defineFixed(lir, ins, LAllocation(AnyRegister(ForkJoinGetSliceReg_output)));
}

bool
LIRGeneratorX86Shared::visitCompareExchangeTypedArrayElement(MCompareExchangeTypedArrayElement *ins)
{
    MOZ_ASSERT(ins->arrayType() != Scalar::Float32);
    MOZ_ASSERT(ins->arrayType() != Scalar::Float64);

    MOZ_ASSERT(ins->elements()->type() == MIRType_Elements);
    MOZ_ASSERT(ins->index()->type() == MIRType_Int32);

    const LUse elements = useRegister(ins->elements());
    const LAllocation index = useRegisterOrConstant(ins->index());

    // Register allocation:
    //
    // If the target is an integer register then the target must be
    // eax.
    //
    // If the target is a floating register then we need a temp at the
    // lower level; that temp must be eax.
    //
    // oldval must be in a register.
    //
    // newval will need to be in a register.  If the source is a byte
    // array then the newval must be a register that has a byte size:
    // ebx, ecx, or edx, since eax is taken for the output in this
    // case.
    //
    // Bug #1077036 describes some optimization opportunities.

    bool fixedOutput = false;
    LDefinition tempDef = LDefinition::BogusTemp();
    LAllocation newval;
    if (ins->arrayType() == Scalar::Uint32 && IsFloatingPointType(ins->type())) {
        tempDef = tempFixed(eax);
        newval = useRegister(ins->newval());
    } else {
        fixedOutput = true;
        if (ins->isByteArray())
            newval = useFixed(ins->newval(), ebx);
        else
            newval = useRegister(ins->newval());
    }

    // A register allocator limitation precludes 'useRegisterAtStart()' here.
    const LAllocation oldval = useRegister(ins->oldval());

    LCompareExchangeTypedArrayElement *lir =
        new(alloc()) LCompareExchangeTypedArrayElement(elements, index, oldval, newval, tempDef);

    return fixedOutput ? defineFixed(lir, ins, LAllocation(AnyRegister(eax))) : define(lir, ins);
}

bool
LIRGeneratorX86Shared::visitAtomicTypedArrayElementBinop(MAtomicTypedArrayElementBinop *ins)
{
    MOZ_ASSERT(ins->arrayType() != Scalar::Uint8Clamped);
    MOZ_ASSERT(ins->arrayType() != Scalar::Float32);
    MOZ_ASSERT(ins->arrayType() != Scalar::Float64);

    MOZ_ASSERT(ins->elements()->type() == MIRType_Elements);
    MOZ_ASSERT(ins->index()->type() == MIRType_Int32);

    const LUse elements = useRegister(ins->elements());
    const LAllocation index = useRegisterOrConstant(ins->index());

    // Register allocation:
    //
    // For ADD and SUB we'll use XADD:
    //
    //    movl       src, output
    //    lock xaddl output, mem
    //
    // For the 8-bit variants XADD needs a byte register for the
    // output only.
    //
    // For AND/OR/XOR we need to use a CMPXCHG loop:
    //
    //    movl          *mem, eax
    // L: mov           eax, temp
    //    andl          src, temp
    //    lock cmpxchg  temp, mem  ; reads eax also
    //    jnz           L
    //    ; result in eax
    //
    // Note the placement of L, cmpxchg will update eax with *mem if
    // *mem does not have the expected value, so reloading it at the
    // top of the loop is redundant.
    //
    // If the array is not a uint32 array then:
    //  - eax should be the output (one result of the cmpxchg)
    //  - there is a temp, which must have a byte register if
    //    the array has 1-byte elements elements
    //
    // If the array is a uint32 array then:
    //  - eax is the first temp
    //  - we also need a second temp
    //
    // For simplicity we force the 'value' into a byte register if the
    // array has 1-byte elements, though that could be worked around.
    //
    // For simplicity we also choose fixed byte registers even when
    // any available byte register would have been OK.
    //
    // There are optimization opportunities:
    //  - when the result is unused, Bug #1077014.
    //  - better register allocation and instruction selection, Bug #1077036.

    bool bitOp = !(ins->operation() == AtomicFetchAddOp || ins->operation() == AtomicFetchSubOp);
    bool fixedOutput = true;
    LDefinition tempDef1 = LDefinition::BogusTemp();
    LDefinition tempDef2 = LDefinition::BogusTemp();
    LAllocation value;

    if (ins->arrayType() == Scalar::Uint32 && IsFloatingPointType(ins->type())) {
        value = useRegister(ins->value());
        fixedOutput = false;
        if (bitOp) {
            tempDef1 = tempFixed(eax);
            tempDef2 = temp();
        } else {
            tempDef1 = temp();
        }
    } else if (ins->isByteArray()) {
        value = useFixed(ins->value(), ebx);
        if (bitOp)
            tempDef1 = tempFixed(ecx);
    }
    else {
        value = useRegister(ins->value());
        if (bitOp)
            tempDef1 = temp();
    }

    LAtomicTypedArrayElementBinop *lir =
        new(alloc()) LAtomicTypedArrayElementBinop(elements, index, value, tempDef1, tempDef2);

    return fixedOutput ? defineFixed(lir, ins, LAllocation(AnyRegister(eax))) : define(lir, ins);
}

bool
LIRGeneratorX86Shared::visitSimdTernaryBitwise(MSimdTernaryBitwise *ins)
{
    MOZ_ASSERT(IsSimdType(ins->type()));

    if (ins->type() == MIRType_Int32x4 || ins->type() == MIRType_Float32x4) {
        LSimdSelect *lins = new(alloc()) LSimdSelect;

        // This must be useRegisterAtStart() because it is destroyed.
        lins->setOperand(0, useRegisterAtStart(ins->getOperand(0)));
        // This must be useRegisterAtStart() because it is destroyed.
        lins->setOperand(1, useRegisterAtStart(ins->getOperand(1)));
        // This could be useRegister(), but combining it with
        // useRegisterAtStart() is broken see bug 772830.
        lins->setOperand(2, useRegisterAtStart(ins->getOperand(2)));
        // The output is constrained to be in the same register as the second
        // argument to avoid redundantly copying the result into place. The
        // register allocator will move the result if necessary.
        return defineReuseInput(lins, ins, 1);
    }

    MOZ_CRASH("Unknown SIMD kind when doing bitwise operations");
    return false;
}

bool
LIRGeneratorX86Shared::visitSimdSplatX4(MSimdSplatX4 *ins)
{
    LAllocation x = useRegisterAtStart(ins->getOperand(0));
    LSimdSplatX4 *lir = new(alloc()) LSimdSplatX4(x);

    switch (ins->type()) {
      case MIRType_Int32x4:
        return define(lir, ins);
      case MIRType_Float32x4:
        return defineReuseInput(lir, ins, 0);
      default:
        MOZ_CRASH("Unknown SIMD kind");
    }
}


bool
LIRGeneratorX86Shared::visitSimdValueX4(MSimdValueX4 *ins)
{
    if (ins->type() == MIRType_Float32x4) {
        // As x is used at start and reused for the output, other inputs can't
        // be used at start.
        LAllocation x = useRegisterAtStart(ins->getOperand(0));
        LAllocation y = useRegister(ins->getOperand(1));
        LAllocation z = useRegister(ins->getOperand(2));
        LAllocation w = useRegister(ins->getOperand(3));
        LDefinition copyY = tempCopy(ins->getOperand(1), 1);
        return defineReuseInput(new (alloc()) LSimdValueFloat32x4(x, y, z, w, copyY), ins, 0);
    }

    // No defineReuseInput => useAtStart for everyone.
    LAllocation x = useRegisterAtStart(ins->getOperand(0));
    LAllocation y = useRegisterAtStart(ins->getOperand(1));
    LAllocation z = useRegisterAtStart(ins->getOperand(2));
    LAllocation w = useRegisterAtStart(ins->getOperand(3));

    MOZ_ASSERT(ins->type() == MIRType_Int32x4);
    return define(new(alloc()) LSimdValueInt32x4(x, y, z, w), ins);
}
