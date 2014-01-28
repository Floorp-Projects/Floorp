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

using mozilla::FloorLog2;

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
    JS_ASSERT(ins->obj()->type() == MIRType_Object);

    LGuardShape *guard = new(alloc()) LGuardShape(useRegister(ins->obj()));
    if (!assignSnapshot(guard, ins->bailoutKind()))
        return false;
    if (!add(guard, ins))
        return false;
    return redefine(ins, ins->obj());
}

bool
LIRGeneratorX86Shared::visitGuardObjectType(MGuardObjectType *ins)
{
    JS_ASSERT(ins->obj()->type() == MIRType_Object);

    LGuardObjectType *guard = new(alloc()) LGuardObjectType(useRegister(ins->obj()));
    if (!assignSnapshot(guard))
        return false;
    if (!add(guard, ins))
        return false;
    return redefine(ins, ins->obj());
}

bool
LIRGeneratorX86Shared::visitPowHalf(MPowHalf *ins)
{
    MDefinition *input = ins->input();
    JS_ASSERT(input->type() == MIRType_Double);
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
        ins->setOperand(1, useOrConstant(rhs));
    else
        ins->setOperand(1, useFixed(rhs, ecx));

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
    ins->setOperand(1, useOrConstant(rhs));
    return defineReuseInput(ins, mir, 0);
}

bool
LIRGeneratorX86Shared::lowerForFPU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs, MDefinition *rhs)
{
    ins->setOperand(0, useRegisterAtStart(lhs));
    ins->setOperand(1, use(rhs));
    return defineReuseInput(ins, mir, 0);
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
    if (mul->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
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

        // Check for division by a positive power of two, which is an easy and
        // important case to optimize. Note that other optimizations are also
        // possible; division by negative powers of two can be optimized in a
        // similar manner as positive powers of two, and division by other
        // constants can be optimized by a reciprocal multiplication technique.
        int32_t shift = FloorLog2(rhs);
        if (rhs > 0 && 1 << shift == rhs) {
            LAllocation lhs = useRegisterAtStart(div->lhs());
            LDivPowTwoI *lir;
            if (!div->canBeNegativeDividend()) {
                // Numerator is unsigned, so does not need adjusting.
                lir = new(alloc()) LDivPowTwoI(lhs, lhs, shift);
            } else {
                // Numerator is signed, and needs adjusting, and an extra
                // lhs copy register is needed.
                lir = new(alloc()) LDivPowTwoI(lhs, useRegister(div->lhs()), shift);
            }
            if (div->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
                return false;
            return defineReuseInput(lir, div, 0);
        }
    }

    // Optimize x/x. This is quaint, but it also protects the LDivI code below.
    // Since LDivI requires lhs to be in %eax, and since the register allocator
    // can't put a virtual register in two physical registers at the same time,
    // this puts rhs in %eax too, and since rhs isn't marked usedAtStart, it
    // would conflict with the %eax output register. (rhs could be marked
    // usedAtStart but for the fact that LDivI clobbers %edx early and rhs could
    // happen to be in %edx).
    if (div->lhs() == div->rhs()) {
        if (!div->canBeDivideByZero())
            return define(new(alloc()) LInteger(1), div);

        LDivSelfI *lir = new(alloc()) LDivSelfI(useRegisterAtStart(div->lhs()));
        if (div->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
            return false;
        return define(lir, div);
    }

    LDivI *lir = new(alloc()) LDivI(useFixed(div->lhs(), eax), useRegister(div->rhs()), tempFixed(edx));
    if (div->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
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
        int32_t shift = FloorLog2(rhs);
        if (rhs > 0 && 1 << shift == rhs) {
            LModPowTwoI *lir = new(alloc()) LModPowTwoI(useRegisterAtStart(mod->lhs()), shift);
            if (mod->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
                return false;
            return defineReuseInput(lir, mod, 0);
        }
    }

    // Optimize x%x. The comments in lowerDivI apply here as well, except
    // that we return 0 for all cases except when x is 0 and we're not
    // truncated.
    if (mod->rhs() == mod->lhs()) {
        if (mod->isTruncated())
            return define(new(alloc()) LInteger(0), mod);

        LModSelfI *lir = new(alloc()) LModSelfI(useRegisterAtStart(mod->lhs()));
        if (mod->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
            return false;
        return define(lir, mod);
    }

    LModI *lir = new(alloc()) LModI(useFixedAtStart(mod->lhs(), eax),
                                    useRegister(mod->rhs()),
                                    tempFixed(eax));
    if (mod->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
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

    JS_ASSERT(ins->type() == MIRType_Double);
    return defineReuseInput(new(alloc()) LNegD(useRegisterAtStart(ins->input())), ins, 0);
}

bool
LIRGeneratorX86Shared::lowerUDiv(MDiv *div)
{
    // Optimize x/x. The comments in lowerDivI apply here as well.
    if (div->lhs() == div->rhs()) {
        if (!div->canBeDivideByZero())
            return define(new(alloc()) LInteger(1), div);

        LDivSelfI *lir = new(alloc()) LDivSelfI(useRegisterAtStart(div->lhs()));
        if (div->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
            return false;
        return define(lir, div);
    }

    LUDivOrMod *lir = new(alloc()) LUDivOrMod(useFixedAtStart(div->lhs(), eax),
                                              useRegister(div->rhs()),
                                              tempFixed(edx));
    if (div->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
        return false;
    return defineFixed(lir, div, LAllocation(AnyRegister(eax)));
}

bool
LIRGeneratorX86Shared::lowerUMod(MMod *mod)
{
    // Optimize x%x. The comments in lowerModI apply here as well.
    if (mod->lhs() == mod->rhs()) {
        if (mod->isTruncated() || (mod->isUnsigned() && !mod->canBeDivideByZero()))
            return define(new(alloc()) LInteger(0), mod);

        LModSelfI *lir = new(alloc()) LModSelfI(useRegisterAtStart(mod->lhs()));
        if (mod->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
            return false;
        return define(lir, mod);
    }

    LUDivOrMod *lir = new(alloc()) LUDivOrMod(useFixedAtStart(mod->lhs(), eax),
                                              useRegister(mod->rhs()),
                                              tempFixed(eax));
    if (mod->fallible() && !assignSnapshot(lir, Bailout_BaselineInfo))
        return false;
    return defineFixed(lir, mod, LAllocation(AnyRegister(edx)));
}

bool
LIRGeneratorX86Shared::lowerUrshD(MUrsh *mir)
{
    MDefinition *lhs = mir->lhs();
    MDefinition *rhs = mir->rhs();

    JS_ASSERT(lhs->type() == MIRType_Int32);
    JS_ASSERT(rhs->type() == MIRType_Int32);
    JS_ASSERT(mir->type() == MIRType_Double);

#ifdef JS_CODEGEN_X64
    JS_ASSERT(ecx == rcx);
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
    JS_ASSERT(opd->type() == MIRType_Double);

    LDefinition maybeTemp = Assembler::HasSSE3() ? LDefinition::BogusTemp() : tempDouble();
    return define(new(alloc()) LTruncateDToInt32(useRegister(opd), maybeTemp), ins);
}

bool
LIRGeneratorX86Shared::lowerTruncateFToInt32(MTruncateToInt32 *ins)
{
    MDefinition *opd = ins->input();
    JS_ASSERT(opd->type() == MIRType_Float32);

    LDefinition maybeTemp = Assembler::HasSSE3() ? LDefinition::BogusTemp() : tempFloat32();
    return define(new(alloc()) LTruncateFToInt32(useRegister(opd), maybeTemp), ins);
}
