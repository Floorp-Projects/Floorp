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

bool
LIRGeneratorX64::useBox(LInstruction *lir, size_t n, MDefinition *mir,
                        LUse::Policy policy, bool useAtStart)
{
    JS_ASSERT(mir->type() == MIRType_Value);

    if (!ensureDefined(mir))
        return false;
    lir->setOperand(n, LUse(mir->virtualRegister(), policy, useAtStart));
    return true;
}

bool
LIRGeneratorX64::useBoxFixed(LInstruction *lir, size_t n, MDefinition *mir, Register reg1, Register)
{
    JS_ASSERT(mir->type() == MIRType_Value);

    if (!ensureDefined(mir))
        return false;
    lir->setOperand(n, LUse(reg1, mir->virtualRegister()));
    return true;
}

LAllocation
LIRGeneratorX64::useByteOpRegister(MDefinition *mir)
{
    return useRegister(mir);
}

LAllocation
LIRGeneratorX64::useByteOpRegisterOrNonDoubleConstant(MDefinition *mir)
{
    return useRegisterOrNonDoubleConstant(mir);
}

LDefinition
LIRGeneratorX64::tempToUnbox()
{
    return temp();
}

bool
LIRGeneratorX64::visitBox(MBox *box)
{
    MDefinition *opd = box->getOperand(0);

    // If the operand is a constant, emit near its uses.
    if (opd->isConstant() && box->canEmitAtUses())
        return emitAtUses(box);

    if (opd->isConstant())
        return define(new(alloc()) LValue(opd->toConstant()->value()), box, LDefinition(LDefinition::BOX));

    LBox *ins = new(alloc()) LBox(opd->type(), useRegister(opd));
    return define(ins, box, LDefinition(LDefinition::BOX));
}

bool
LIRGeneratorX64::visitUnbox(MUnbox *unbox)
{
    MDefinition *box = unbox->getOperand(0);
    LUnboxBase *lir;
    if (IsFloatingPointType(unbox->type()))
        lir = new(alloc()) LUnboxFloatingPoint(useRegisterAtStart(box), unbox->type());
    else
        lir = new(alloc()) LUnbox(useRegisterAtStart(box));

    if (unbox->fallible() && !assignSnapshot(lir, unbox->bailoutKind()))
        return false;

    return define(lir, unbox);
}

bool
LIRGeneratorX64::visitReturn(MReturn *ret)
{
    MDefinition *opd = ret->getOperand(0);
    JS_ASSERT(opd->type() == MIRType_Value);

    LReturn *ins = new(alloc()) LReturn;
    ins->setOperand(0, useFixed(opd, JSReturnReg));
    return add(ins);
}

bool
LIRGeneratorX64::defineUntypedPhi(MPhi *phi, size_t lirIndex)
{
    return defineTypedPhi(phi, lirIndex);
}

void
LIRGeneratorX64::lowerUntypedPhiInput(MPhi *phi, uint32_t inputPosition, LBlock *block, size_t lirIndex)
{
    lowerTypedPhiInput(phi, inputPosition, block, lirIndex);
}

bool
LIRGeneratorX64::visitAsmJSUnsignedToDouble(MAsmJSUnsignedToDouble *ins)
{
    JS_ASSERT(ins->input()->type() == MIRType_Int32);
    LAsmJSUInt32ToDouble *lir = new(alloc()) LAsmJSUInt32ToDouble(useRegisterAtStart(ins->input()));
    return define(lir, ins);
}

bool
LIRGeneratorX64::visitAsmJSUnsignedToFloat32(MAsmJSUnsignedToFloat32 *ins)
{
    JS_ASSERT(ins->input()->type() == MIRType_Int32);
    LAsmJSUInt32ToFloat32 *lir = new(alloc()) LAsmJSUInt32ToFloat32(useRegisterAtStart(ins->input()));
    return define(lir, ins);
}

bool
LIRGeneratorX64::visitAsmJSLoadHeap(MAsmJSLoadHeap *ins)
{
    MDefinition *ptr = ins->ptr();
    JS_ASSERT(ptr->type() == MIRType_Int32);

    // The X64 does not inline an explicit bounds check so has no need to keep the
    // index in a register, however only a positive index is accepted because a
    // negative offset encoded as an offset in the addressing mode would not wrap
    // back into the protected area reserved for the heap.
    if (ptr->isConstant() && ptr->toConstant()->value().toInt32() >= 0) {
        LAsmJSLoadHeap *lir = new(alloc()) LAsmJSLoadHeap(LAllocation(ptr->toConstant()->vp()));
        return define(lir, ins);
    }
    return define(new(alloc()) LAsmJSLoadHeap(useRegisterAtStart(ptr)), ins);
}

bool
LIRGeneratorX64::visitAsmJSStoreHeap(MAsmJSStoreHeap *ins)
{
    MDefinition *ptr = ins->ptr();
    JS_ASSERT(ptr->type() == MIRType_Int32);
    LAsmJSStoreHeap *lir;

    // Note only a positive constant index is accepted because a negative offset
    // encoded as an offset in the addressing mode would not wrap back into the
    // protected area reserved for the heap.
    LAllocation ptrAlloc = useRegisterOrNonNegativeConstantAtStart(ptr);
    switch (ins->viewType()) {
      case ArrayBufferView::TYPE_INT8: case ArrayBufferView::TYPE_UINT8:
      case ArrayBufferView::TYPE_INT16: case ArrayBufferView::TYPE_UINT16:
      case ArrayBufferView::TYPE_INT32: case ArrayBufferView::TYPE_UINT32:
        lir = new(alloc()) LAsmJSStoreHeap(ptrAlloc, useRegisterOrConstantAtStart(ins->value()));
        break;
      case ArrayBufferView::TYPE_FLOAT32: case ArrayBufferView::TYPE_FLOAT64:
        lir = new(alloc()) LAsmJSStoreHeap(ptrAlloc, useRegisterAtStart(ins->value()));
        break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected array type");
    }

    return add(lir, ins);
}

bool
LIRGeneratorX64::visitAsmJSLoadFuncPtr(MAsmJSLoadFuncPtr *ins)
{
    return define(new(alloc()) LAsmJSLoadFuncPtr(useRegister(ins->index()), temp()), ins);
}

bool
LIRGeneratorX64::visitStoreTypedArrayElementStatic(MStoreTypedArrayElementStatic *ins)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}
