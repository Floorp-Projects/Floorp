/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ion/x64/Lowering-x64.h"

#include "ion/MIR.h"
#include "ion/shared/Lowering-shared-inl.h"
#include "ion/x64/Assembler-x64.h"

using namespace js;
using namespace js::ion;

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
        return define(new LValue(opd->toConstant()->value()), box, LDefinition(LDefinition::BOX));

    LBox *ins = new LBox(opd->type(), useRegister(opd));
    return define(ins, box, LDefinition(LDefinition::BOX));
}

bool
LIRGeneratorX64::visitUnbox(MUnbox *unbox)
{
    MDefinition *box = unbox->getOperand(0);
    LUnboxBase *lir;
    if (unbox->type() == MIRType_Double)
        lir = new LUnboxDouble(useRegister(box));
    else
        lir = new LUnbox(useRegister(box));

    if (unbox->fallible() && !assignSnapshot(lir, unbox->bailoutKind()))
        return false;

    return define(lir, unbox);
}

bool
LIRGeneratorX64::visitReturn(MReturn *ret)
{
    MDefinition *opd = ret->getOperand(0);
    JS_ASSERT(opd->type() == MIRType_Value);

    LReturn *ins = new LReturn;
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
LIRGeneratorX64::visitStoreTypedArrayElement(MStoreTypedArrayElement *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);

    if (ins->isFloatArray())
        JS_ASSERT(ins->value()->type() == MIRType_Double);
    else
        JS_ASSERT(ins->value()->type() == MIRType_Int32);

    LUse elements = useRegister(ins->elements());
    LAllocation index = useRegisterOrConstant(ins->index());
    LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
    return add(new LStoreTypedArrayElement(elements, index, value), ins);
}

bool
LIRGeneratorX64::visitStoreTypedArrayElementHole(MStoreTypedArrayElementHole *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);
    JS_ASSERT(ins->length()->type() == MIRType_Int32);

    if (ins->isFloatArray())
        JS_ASSERT(ins->value()->type() == MIRType_Double);
    else
        JS_ASSERT(ins->value()->type() == MIRType_Int32);

    LUse elements = useRegister(ins->elements());
    LAllocation length = useAnyOrConstant(ins->length());
    LAllocation index = useRegisterOrConstant(ins->index());
    LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
    return add(new LStoreTypedArrayElementHole(elements, length, index, value), ins);
}

bool
LIRGeneratorX64::visitAsmJSUnsignedToDouble(MAsmJSUnsignedToDouble *ins)
{
    JS_ASSERT(ins->input()->type() == MIRType_Int32);
    LUInt32ToDouble *lir = new LUInt32ToDouble(useRegisterAtStart(ins->input()));
    return define(lir, ins);
}

bool
LIRGeneratorX64::visitAsmJSStoreHeap(MAsmJSStoreHeap *ins)
{
    LAsmJSStoreHeap *lir;
    switch (ins->viewType()) {
      case ArrayBufferView::TYPE_INT8: case ArrayBufferView::TYPE_UINT8:
      case ArrayBufferView::TYPE_INT16: case ArrayBufferView::TYPE_UINT16:
      case ArrayBufferView::TYPE_INT32: case ArrayBufferView::TYPE_UINT32:
        lir = new LAsmJSStoreHeap(useRegisterAtStart(ins->ptr()),
                                  useRegisterOrConstantAtStart(ins->value()));
        break;
      case ArrayBufferView::TYPE_FLOAT32:
      case ArrayBufferView::TYPE_FLOAT64:
        lir = new LAsmJSStoreHeap(useRegisterAtStart(ins->ptr()),
                                  useRegisterAtStart(ins->value()));
        break;
      default: MOZ_ASSUME_UNREACHABLE("unexpected array type");
    }

    return add(lir, ins);
}

bool
LIRGeneratorX64::visitAsmJSLoadFuncPtr(MAsmJSLoadFuncPtr *ins)
{
    return define(new LAsmJSLoadFuncPtr(useRegister(ins->index()), temp()), ins);
}

LGetPropertyCacheT *
LIRGeneratorX64::newLGetPropertyCacheT(MGetPropertyCache *ins)
{
    return new LGetPropertyCacheT(useRegister(ins->object()), LDefinition::BogusTemp());
}

LGetElementCacheT *
LIRGeneratorX64::newLGetElementCacheT(MGetElementCache *ins)
{
    return new LGetElementCacheT(useRegister(ins->object()),
                                 useRegister(ins->index()),
                                 LDefinition::BogusTemp());
}

bool
LIRGeneratorX64::visitStoreTypedArrayElementStatic(MStoreTypedArrayElementStatic *ins)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}
