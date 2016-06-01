/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x64/Lowering-x64.h"

#include "jit/MIR.h"
#include "jit/x64/Assembler-x64.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

LBoxAllocation
LIRGeneratorX64::useBoxFixed(MDefinition* mir, Register reg1, Register, bool useAtStart)
{
    MOZ_ASSERT(mir->type() == MIRType::Value);

    ensureDefined(mir);
    return LBoxAllocation(LUse(reg1, mir->virtualRegister(), useAtStart));
}

LAllocation
LIRGeneratorX64::useByteOpRegister(MDefinition* mir)
{
    return useRegister(mir);
}

LAllocation
LIRGeneratorX64::useByteOpRegisterOrNonDoubleConstant(MDefinition* mir)
{
    return useRegisterOrNonDoubleConstant(mir);
}

LDefinition
LIRGeneratorX64::tempByteOpRegister()
{
    return temp();
}

LDefinition
LIRGeneratorX64::tempToUnbox()
{
    return temp();
}

void
LIRGeneratorX64::visitBox(MBox* box)
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
LIRGeneratorX64::visitUnbox(MUnbox* unbox)
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
LIRGeneratorX64::visitReturn(MReturn* ret)
{
    MDefinition* opd = ret->getOperand(0);
    MOZ_ASSERT(opd->type() == MIRType::Value);

    LReturn* ins = new(alloc()) LReturn;
    ins->setOperand(0, useFixed(opd, JSReturnReg));
    add(ins);
}

void
LIRGeneratorX64::defineUntypedPhi(MPhi* phi, size_t lirIndex)
{
    defineTypedPhi(phi, lirIndex);
}

void
LIRGeneratorX64::lowerUntypedPhiInput(MPhi* phi, uint32_t inputPosition, LBlock* block, size_t lirIndex)
{
    lowerTypedPhiInput(phi, inputPosition, block, lirIndex);
}

void
LIRGeneratorX64::visitCompareExchangeTypedArrayElement(MCompareExchangeTypedArrayElement* ins)
{
    lowerCompareExchangeTypedArrayElement(ins, /* useI386ByteRegisters = */ false);
}

void
LIRGeneratorX64::visitAtomicExchangeTypedArrayElement(MAtomicExchangeTypedArrayElement* ins)
{
    lowerAtomicExchangeTypedArrayElement(ins, /* useI386ByteRegisters = */ false);
}

void
LIRGeneratorX64::visitAtomicTypedArrayElementBinop(MAtomicTypedArrayElementBinop* ins)
{
    lowerAtomicTypedArrayElementBinop(ins, /* useI386ByteRegisters = */ false);
}

void
LIRGeneratorX64::visitAsmJSUnsignedToDouble(MAsmJSUnsignedToDouble* ins)
{
    MOZ_ASSERT(ins->input()->type() == MIRType::Int32);
    LAsmJSUInt32ToDouble* lir = new(alloc()) LAsmJSUInt32ToDouble(useRegisterAtStart(ins->input()));
    define(lir, ins);
}

void
LIRGeneratorX64::visitAsmJSUnsignedToFloat32(MAsmJSUnsignedToFloat32* ins)
{
    MOZ_ASSERT(ins->input()->type() == MIRType::Int32);
    LAsmJSUInt32ToFloat32* lir = new(alloc()) LAsmJSUInt32ToFloat32(useRegisterAtStart(ins->input()));
    define(lir, ins);
}

void
LIRGeneratorX64::visitAsmJSLoadHeap(MAsmJSLoadHeap* ins)
{
    MDefinition* base = ins->base();
    MOZ_ASSERT(base->type() == MIRType::Int32);

    // For simplicity, require a register if we're going to emit a bounds-check
    // branch, so that we don't have special cases for constants.
    LAllocation baseAlloc = gen->needsAsmJSBoundsCheckBranch(ins)
                            ? useRegisterAtStart(base)
                            : useRegisterOrZeroAtStart(base);

    define(new(alloc()) LAsmJSLoadHeap(baseAlloc), ins);
}

void
LIRGeneratorX64::visitAsmJSStoreHeap(MAsmJSStoreHeap* ins)
{
    MDefinition* base = ins->base();
    MOZ_ASSERT(base->type() == MIRType::Int32);

    // For simplicity, require a register if we're going to emit a bounds-check
    // branch, so that we don't have special cases for constants.
    LAllocation baseAlloc = gen->needsAsmJSBoundsCheckBranch(ins)
                            ? useRegisterAtStart(base)
                            : useRegisterOrZeroAtStart(base);

    LAsmJSStoreHeap* lir = nullptr;  // initialize to silence GCC warning
    switch (ins->accessType()) {
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Int16:
      case Scalar::Uint16:
      case Scalar::Int32:
      case Scalar::Uint32:
        lir = new(alloc()) LAsmJSStoreHeap(baseAlloc, useRegisterOrConstantAtStart(ins->value()));
        break;
      case Scalar::Float32:
      case Scalar::Float64:
      case Scalar::Float32x4:
      case Scalar::Int8x16:
      case Scalar::Int16x8:
      case Scalar::Int32x4:
        lir = new(alloc()) LAsmJSStoreHeap(baseAlloc, useRegisterAtStart(ins->value()));
        break;
      case Scalar::Uint8Clamped:
      case Scalar::MaxTypedArrayViewType:
        MOZ_CRASH("unexpected array type");
    }
    add(lir, ins);
}

void
LIRGeneratorX64::visitAsmJSCompareExchangeHeap(MAsmJSCompareExchangeHeap* ins)
{
    MDefinition* base = ins->base();
    MOZ_ASSERT(base->type() == MIRType::Int32);

    // The output may not be used but will be clobbered regardless, so
    // pin the output to eax.
    //
    // The input values must both be in registers.

    const LAllocation oldval = useRegister(ins->oldValue());
    const LAllocation newval = useRegister(ins->newValue());

    LAsmJSCompareExchangeHeap* lir =
        new(alloc()) LAsmJSCompareExchangeHeap(useRegister(base), oldval, newval);

    defineFixed(lir, ins, LAllocation(AnyRegister(eax)));
}

void
LIRGeneratorX64::visitAsmJSAtomicExchangeHeap(MAsmJSAtomicExchangeHeap* ins)
{
    MOZ_ASSERT(ins->base()->type() == MIRType::Int32);

    const LAllocation base = useRegister(ins->base());
    const LAllocation value = useRegister(ins->value());

    // The output may not be used but will be clobbered regardless,
    // so ignore the case where we're not using the value and just
    // use the output register as a temp.

    LAsmJSAtomicExchangeHeap* lir =
        new(alloc()) LAsmJSAtomicExchangeHeap(base, value);
    define(lir, ins);
}

void
LIRGeneratorX64::visitAsmJSAtomicBinopHeap(MAsmJSAtomicBinopHeap* ins)
{
    MDefinition* base = ins->base();
    MOZ_ASSERT(base->type() == MIRType::Int32);

    // Case 1: the result of the operation is not used.
    //
    // We'll emit a single instruction: LOCK ADD, LOCK SUB, LOCK AND,
    // LOCK OR, or LOCK XOR.

    if (!ins->hasUses()) {
        LAsmJSAtomicBinopHeapForEffect* lir =
            new(alloc()) LAsmJSAtomicBinopHeapForEffect(useRegister(base),
                                                        useRegisterOrConstant(ins->value()));
        add(lir, ins);
        return;
    }

    // Case 2: the result of the operation is used.
    //
    // For ADD and SUB we'll use XADD with word and byte ops as
    // appropriate.  Any output register can be used and if value is a
    // register it's best if it's the same as output:
    //
    //    movl       value, output  ; if value != output
    //    lock xaddl output, mem
    //
    // For AND/OR/XOR we need to use a CMPXCHG loop, and the output is
    // always in rax:
    //
    //    movl          *mem, rax
    // L: mov           rax, temp
    //    andl          value, temp
    //    lock cmpxchg  temp, mem  ; reads rax also
    //    jnz           L
    //    ; result in rax
    //
    // Note the placement of L, cmpxchg will update rax with *mem if
    // *mem does not have the expected value, so reloading it at the
    // top of the loop would be redundant.

    bool bitOp = !(ins->operation() == AtomicFetchAddOp || ins->operation() == AtomicFetchSubOp);
    bool reuseInput = false;
    LAllocation value;

    if (bitOp || ins->value()->isConstant()) {
        value = useRegisterOrConstant(ins->value());
    } else {
        reuseInput = true;
        value = useRegisterAtStart(ins->value());
    }

    LAsmJSAtomicBinopHeap* lir =
        new(alloc()) LAsmJSAtomicBinopHeap(useRegister(base),
                                           value,
                                           bitOp ? temp() : LDefinition::BogusTemp());

    if (reuseInput)
        defineReuseInput(lir, ins, LAsmJSAtomicBinopHeap::valueOp);
    else if (bitOp)
        defineFixed(lir, ins, LAllocation(AnyRegister(rax)));
    else
        define(lir, ins);
}

void
LIRGeneratorX64::visitAsmJSLoadFuncPtr(MAsmJSLoadFuncPtr* ins)
{
    define(new(alloc()) LAsmJSLoadFuncPtr(useRegister(ins->index()), temp()), ins);
}

void
LIRGeneratorX64::visitSubstr(MSubstr* ins)
{
    LSubstr* lir = new (alloc()) LSubstr(useRegister(ins->string()),
                                         useRegister(ins->begin()),
                                         useRegister(ins->length()),
                                         temp(),
                                         temp(),
                                         tempByteOpRegister());
    define(lir, ins);
    assignSafepoint(lir, ins);
}

void
LIRGeneratorX64::visitStoreTypedArrayElementStatic(MStoreTypedArrayElementStatic* ins)
{
    MOZ_CRASH("NYI");
}

void
LIRGeneratorX64::visitRandom(MRandom* ins)
{
    LRandom *lir = new(alloc()) LRandom(temp(),
                                        temp(),
                                        temp());
    defineFixed(lir, ins, LFloatReg(ReturnDoubleReg));
}

void
LIRGeneratorX64::lowerDivI64(MDiv* div)
{
    if (div->isUnsigned()) {
        lowerUDiv64(div);
        return;
    }

    LDivOrModI64* lir = new(alloc()) LDivOrModI64(useRegister(div->lhs()), useRegister(div->rhs()),
                                                  tempFixed(rdx));
    defineInt64Fixed(lir, div, LInt64Allocation(LAllocation(AnyRegister(rax))));
}

void
LIRGeneratorX64::lowerModI64(MMod* mod)
{
    if (mod->isUnsigned()) {
        lowerUMod64(mod);
        return;
    }

    LDivOrModI64* lir = new(alloc()) LDivOrModI64(useRegister(mod->lhs()), useRegister(mod->rhs()),
                                                  tempFixed(rax));
    defineInt64Fixed(lir, mod, LInt64Allocation(LAllocation(AnyRegister(rdx))));
}

void
LIRGeneratorX64::lowerUDiv64(MDiv* div)
{
    LUDivOrMod64* lir = new(alloc()) LUDivOrMod64(useRegister(div->lhs()),
                                                  useRegister(div->rhs()),
                                                  tempFixed(rdx));
    defineInt64Fixed(lir, div, LInt64Allocation(LAllocation(AnyRegister(rax))));
}

void
LIRGeneratorX64::lowerUMod64(MMod* mod)
{
    LUDivOrMod64* lir = new(alloc()) LUDivOrMod64(useRegister(mod->lhs()),
                                                  useRegister(mod->rhs()),
                                                  tempFixed(rax));
    defineInt64Fixed(lir, mod, LInt64Allocation(LAllocation(AnyRegister(rdx))));
}

void
LIRGeneratorX64::visitWasmTruncateToInt64(MWasmTruncateToInt64* ins)
{
    MDefinition* opd = ins->input();
    MOZ_ASSERT(opd->type() == MIRType::Double || opd->type() == MIRType::Float32);

    LDefinition maybeTemp = ins->isUnsigned() ? tempDouble() : LDefinition::BogusTemp();
    defineInt64(new(alloc()) LWasmTruncateToInt64(useRegister(opd), maybeTemp), ins);
}

void
LIRGeneratorX64::visitInt64ToFloatingPoint(MInt64ToFloatingPoint* ins)
{
    MDefinition* opd = ins->input();
    MOZ_ASSERT(opd->type() == MIRType::Int64);
    MOZ_ASSERT(IsFloatingPointType(ins->type()));

    define(new(alloc()) LInt64ToFloatingPoint(useInt64Register(opd)), ins);
}
