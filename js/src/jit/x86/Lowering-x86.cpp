/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/x86/Lowering-x86.h"

#include "jit/MIR.h"
#include "jit/x86/Assembler-x86.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::jit;

LDefinition
LIRGeneratorX86::tempForDispatchCache(MIRType outputType)
{
    // x86 doesn't have a scratch register and we need one for the
    // indirect jump for dispatch-style ICs.
    //
    // Note that currently we only install dispatch-style ICs for parallel
    // execution. If this assumption changes, please change it here.
    if (gen->info().executionMode() != ParallelExecution)
        return LDefinition::BogusTemp();

    // If we don't have an output register, we need a temp.
    if (outputType == MIRType_None)
        return temp();

    // If we have a double output register, we need a temp.
    if (outputType == MIRType_Double)
        return temp();

    // Otherwise we have a non-double output register and we can reuse it.
    return LDefinition::BogusTemp();
}

bool
LIRGeneratorX86::useBox(LInstruction *lir, size_t n, MDefinition *mir,
                        LUse::Policy policy, bool useAtStart)
{
    JS_ASSERT(mir->type() == MIRType_Value);

    if (!ensureDefined(mir))
        return false;
    lir->setOperand(n, LUse(mir->virtualRegister(), policy, useAtStart));
    lir->setOperand(n + 1, LUse(VirtualRegisterOfPayload(mir), policy, useAtStart));
    return true;
}

bool
LIRGeneratorX86::useBoxFixed(LInstruction *lir, size_t n, MDefinition *mir, Register reg1,
                             Register reg2)
{
    JS_ASSERT(mir->type() == MIRType_Value);
    JS_ASSERT(reg1 != reg2);

    if (!ensureDefined(mir))
        return false;
    lir->setOperand(n, LUse(reg1, mir->virtualRegister()));
    lir->setOperand(n + 1, LUse(reg2, VirtualRegisterOfPayload(mir)));
    return true;
}

LAllocation
LIRGeneratorX86::useByteOpRegister(MDefinition *mir)
{
    return useFixed(mir, eax);
}

LAllocation
LIRGeneratorX86::useByteOpRegisterOrNonDoubleConstant(MDefinition *mir)
{
    return useFixed(mir, eax);
}

bool
LIRGeneratorX86::visitBox(MBox *box)
{
    MDefinition *inner = box->getOperand(0);

    // If the box wrapped a double, it needs a new register.
    if (IsFloatingPointType(inner->type()))
        return defineBox(new(alloc()) LBoxFloatingPoint(useRegisterAtStart(inner), tempCopy(inner, 0),
                                                        inner->type()), box);

    if (box->canEmitAtUses())
        return emitAtUses(box);

    if (inner->isConstant())
        return defineBox(new(alloc()) LValue(inner->toConstant()->value()), box);

    LBox *lir = new(alloc()) LBox(use(inner), inner->type());

    // Otherwise, we should not define a new register for the payload portion
    // of the output, so bypass defineBox().
    uint32_t vreg = getVirtualRegister();
    if (vreg >= MAX_VIRTUAL_REGISTERS)
        return false;

    // Note that because we're using PASSTHROUGH, we do not change the type of
    // the definition. We also do not define the first output as "TYPE",
    // because it has no corresponding payload at (vreg + 1). Also note that
    // although we copy the input's original type for the payload half of the
    // definition, this is only for clarity. PASSTHROUGH definitions are
    // ignored.
    lir->setDef(0, LDefinition(vreg, LDefinition::GENERAL));
    lir->setDef(1, LDefinition(inner->virtualRegister(), LDefinition::TypeFrom(inner->type()),
                               LDefinition::PASSTHROUGH));
    box->setVirtualRegister(vreg);
    return add(lir);
}

bool
LIRGeneratorX86::visitUnbox(MUnbox *unbox)
{
    // An unbox on x86 reads in a type tag (either in memory or a register) and
    // a payload. Unlike most instructions conusming a box, we ask for the type
    // second, so that the result can re-use the first input.
    MDefinition *inner = unbox->getOperand(0);

    if (!ensureDefined(inner))
        return false;

    if (IsFloatingPointType(unbox->type())) {
        LUnboxFloatingPoint *lir = new(alloc()) LUnboxFloatingPoint(unbox->type());
        if (unbox->fallible() && !assignSnapshot(lir, unbox->bailoutKind()))
            return false;
        if (!useBox(lir, LUnboxFloatingPoint::Input, inner))
            return false;
        return define(lir, unbox);
    }

    // Swap the order we use the box pieces so we can re-use the payload register.
    LUnbox *lir = new(alloc()) LUnbox;
    lir->setOperand(0, usePayloadInRegisterAtStart(inner));
    lir->setOperand(1, useType(inner, LUse::ANY));

    if (unbox->fallible() && !assignSnapshot(lir, unbox->bailoutKind()))
        return false;

    // Note that PASSTHROUGH here is illegal, since types and payloads form two
    // separate intervals. If the type becomes dead before the payload, it
    // could be used as a Value without the type being recoverable. Unbox's
    // purpose is to eagerly kill the definition of a type tag, so keeping both
    // alive (for the purpose of gcmaps) is unappealing. Instead, we create a
    // new virtual register.
    return defineReuseInput(lir, unbox, 0);
}

bool
LIRGeneratorX86::visitReturn(MReturn *ret)
{
    MDefinition *opd = ret->getOperand(0);
    JS_ASSERT(opd->type() == MIRType_Value);

    LReturn *ins = new(alloc()) LReturn;
    ins->setOperand(0, LUse(JSReturnReg_Type));
    ins->setOperand(1, LUse(JSReturnReg_Data));
    return fillBoxUses(ins, 0, opd) && add(ins);
}

bool
LIRGeneratorX86::defineUntypedPhi(MPhi *phi, size_t lirIndex)
{
    LPhi *type = current->getPhi(lirIndex + VREG_TYPE_OFFSET);
    LPhi *payload = current->getPhi(lirIndex + VREG_DATA_OFFSET);

    uint32_t typeVreg = getVirtualRegister();
    if (typeVreg >= MAX_VIRTUAL_REGISTERS)
        return false;

    phi->setVirtualRegister(typeVreg);

    uint32_t payloadVreg = getVirtualRegister();
    if (payloadVreg >= MAX_VIRTUAL_REGISTERS)
        return false;
    JS_ASSERT(typeVreg + 1 == payloadVreg);

    type->setDef(0, LDefinition(typeVreg, LDefinition::TYPE));
    payload->setDef(0, LDefinition(payloadVreg, LDefinition::PAYLOAD));
    annotate(type);
    annotate(payload);
    return true;
}

void
LIRGeneratorX86::lowerUntypedPhiInput(MPhi *phi, uint32_t inputPosition, LBlock *block, size_t lirIndex)
{
    MDefinition *operand = phi->getOperand(inputPosition);
    LPhi *type = block->getPhi(lirIndex + VREG_TYPE_OFFSET);
    LPhi *payload = block->getPhi(lirIndex + VREG_DATA_OFFSET);
    type->setOperand(inputPosition, LUse(operand->virtualRegister() + VREG_TYPE_OFFSET, LUse::ANY));
    payload->setOperand(inputPosition, LUse(VirtualRegisterOfPayload(operand), LUse::ANY));
}

bool
LIRGeneratorX86::visitAsmJSUnsignedToDouble(MAsmJSUnsignedToDouble *ins)
{
    JS_ASSERT(ins->input()->type() == MIRType_Int32);
    LAsmJSUInt32ToDouble *lir = new(alloc()) LAsmJSUInt32ToDouble(useRegisterAtStart(ins->input()), temp());
    return define(lir, ins);
}

bool
LIRGeneratorX86::visitAsmJSUnsignedToFloat32(MAsmJSUnsignedToFloat32 *ins)
{
    JS_ASSERT(ins->input()->type() == MIRType_Int32);
    LAsmJSUInt32ToFloat32 *lir = new(alloc()) LAsmJSUInt32ToFloat32(useRegisterAtStart(ins->input()), temp());
    return define(lir, ins);
}

bool
LIRGeneratorX86::visitAsmJSLoadHeap(MAsmJSLoadHeap *ins)
{
    MDefinition *ptr = ins->ptr();
    LAllocation ptrAlloc;
    JS_ASSERT(ptr->type() == MIRType_Int32);

    // For the x86 it is best to keep the 'ptr' in a register if a bounds check is needed.
    if (ptr->isConstant() && ins->skipBoundsCheck()) {
        int32_t ptrValue = ptr->toConstant()->value().toInt32();
        // A bounds check is only skipped for a positive index.
        JS_ASSERT(ptrValue >= 0);
        ptrAlloc = LAllocation(ptr->toConstant()->vp());
    } else {
        ptrAlloc = useRegisterAtStart(ptr);
    }
    LAsmJSLoadHeap *lir = new(alloc()) LAsmJSLoadHeap(ptrAlloc);
    return define(lir, ins);
}

bool
LIRGeneratorX86::visitAsmJSStoreHeap(MAsmJSStoreHeap *ins)
{
    MDefinition *ptr = ins->ptr();
    LAsmJSStoreHeap *lir;
    JS_ASSERT(ptr->type() == MIRType_Int32);

    if (ptr->isConstant() && ins->skipBoundsCheck()) {
        int32_t ptrValue = ptr->toConstant()->value().toInt32();
        JS_ASSERT(ptrValue >= 0);
        LAllocation ptrAlloc = LAllocation(ptr->toConstant()->vp());
        switch (ins->viewType()) {
          case Scalar::Int8: case Scalar::Uint8:
            // See comment below.
            lir = new(alloc()) LAsmJSStoreHeap(ptrAlloc, useFixed(ins->value(), eax));
            break;
          case Scalar::Int16: case Scalar::Uint16:
          case Scalar::Int32: case Scalar::Uint32:
          case Scalar::Float32: case Scalar::Float64:
            // See comment below.
            lir = new(alloc()) LAsmJSStoreHeap(ptrAlloc, useRegisterAtStart(ins->value()));
            break;
          default: MOZ_CRASH("unexpected array type");
        }
        return add(lir, ins);
    }

    switch (ins->viewType()) {
      case Scalar::Int8: case Scalar::Uint8:
        // See comment for LIRGeneratorX86::useByteOpRegister.
        lir = new(alloc()) LAsmJSStoreHeap(useRegister(ins->ptr()), useFixed(ins->value(), eax));
        break;
      case Scalar::Int16: case Scalar::Uint16:
      case Scalar::Int32: case Scalar::Uint32:
      case Scalar::Float32: case Scalar::Float64:
        // For now, don't allow constant values. The immediate operand
        // affects instruction layout which affects patching.
        lir = new(alloc()) LAsmJSStoreHeap(useRegisterAtStart(ptr), useRegisterAtStart(ins->value()));
        break;
      default: MOZ_CRASH("unexpected array type");
    }

    return add(lir, ins);
}

bool
LIRGeneratorX86::visitStoreTypedArrayElementStatic(MStoreTypedArrayElementStatic *ins)
{
    // The code generated for StoreTypedArrayElementStatic is identical to that
    // for AsmJSStoreHeap, and the same concerns apply.
    LStoreTypedArrayElementStatic *lir;
    switch (ins->viewType()) {
      case Scalar::Int8: case Scalar::Uint8:
      case Scalar::Uint8Clamped:
        lir = new(alloc()) LStoreTypedArrayElementStatic(useRegister(ins->ptr()),
                                                         useFixed(ins->value(), eax));
        break;
      case Scalar::Int16: case Scalar::Uint16:
      case Scalar::Int32: case Scalar::Uint32:
      case Scalar::Float32: case Scalar::Float64:
        lir = new(alloc()) LStoreTypedArrayElementStatic(useRegisterAtStart(ins->ptr()),
                                                         useRegisterAtStart(ins->value()));
        break;
      default: MOZ_CRASH("unexpected array type");
    }

    return add(lir, ins);
}

bool
LIRGeneratorX86::visitAsmJSLoadFuncPtr(MAsmJSLoadFuncPtr *ins)
{
    return define(new(alloc()) LAsmJSLoadFuncPtr(useRegisterAtStart(ins->index())), ins);
}
