/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

#include "jit/arm/Assembler-arm.h"
#include "jit/Lowering.h"
#include "jit/MIR.h"

#include "jit/shared/Lowering-shared-inl.h"

using namespace js;
using namespace js::ion;

using mozilla::FloorLog2;

bool
LIRGeneratorARM::useBox(LInstruction *lir, size_t n, MDefinition *mir,
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
LIRGeneratorARM::useBoxFixed(LInstruction *lir, size_t n, MDefinition *mir, Register reg1,
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

bool
LIRGeneratorARM::lowerConstantDouble(double d, MInstruction *mir)
{
    return define(new LDouble(d), mir);
}

bool
LIRGeneratorARM::visitConstant(MConstant *ins)
{
    if (ins->type() == MIRType_Double) {
        LDouble *lir = new LDouble(ins->value().toDouble());
        return define(lir, ins);
    }

    // Emit non-double constants at their uses.
    if (ins->canEmitAtUses())
        return emitAtUses(ins);

    return LIRGeneratorShared::visitConstant(ins);
}

bool
LIRGeneratorARM::visitBox(MBox *box)
{
    MDefinition *inner = box->getOperand(0);

    // If the box wrapped a double, it needs a new register.
    if (inner->type() == MIRType_Double)
        return defineBox(new LBoxDouble(useRegisterAtStart(inner), tempCopy(inner, 0)), box);

    if (box->canEmitAtUses())
        return emitAtUses(box);

    if (inner->isConstant())
        return defineBox(new LValue(inner->toConstant()->value()), box);

    LBox *lir = new LBox(use(inner), inner->type());

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
LIRGeneratorARM::visitUnbox(MUnbox *unbox)
{
    // An unbox on arm reads in a type tag (either in memory or a register) and
    // a payload. Unlike most instructions conusming a box, we ask for the type
    // second, so that the result can re-use the first input.
    MDefinition *inner = unbox->getOperand(0);

    if (!ensureDefined(inner))
        return false;

    if (unbox->type() == MIRType_Double) {
        LUnboxDouble *lir = new LUnboxDouble();
        if (unbox->fallible() && !assignSnapshot(lir, unbox->bailoutKind()))
            return false;
        if (!useBox(lir, LUnboxDouble::Input, inner))
            return false;
        return define(lir, unbox);
    }

    // Swap the order we use the box pieces so we can re-use the payload register.
    LUnbox *lir = new LUnbox;
    lir->setOperand(0, usePayloadInRegisterAtStart(inner));
    lir->setOperand(1, useType(inner, LUse::REGISTER));

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
LIRGeneratorARM::visitReturn(MReturn *ret)
{
    MDefinition *opd = ret->getOperand(0);
    JS_ASSERT(opd->type() == MIRType_Value);

    LReturn *ins = new LReturn;
    ins->setOperand(0, LUse(JSReturnReg_Type));
    ins->setOperand(1, LUse(JSReturnReg_Data));
    return fillBoxUses(ins, 0, opd) && add(ins);
}

// x = !y
bool
LIRGeneratorARM::lowerForALU(LInstructionHelper<1, 1, 0> *ins, MDefinition *mir, MDefinition *input)
{
    ins->setOperand(0, useRegister(input));
    return define(ins, mir,
                  LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::DEFAULT));
}

// z = x+y
bool
LIRGeneratorARM::lowerForALU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs, MDefinition *rhs)
{
    ins->setOperand(0, useRegister(lhs));
    ins->setOperand(1, useRegisterOrConstant(rhs));
    return define(ins, mir,
                  LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::DEFAULT));
}

bool
LIRGeneratorARM::lowerForFPU(LInstructionHelper<1, 1, 0> *ins, MDefinition *mir, MDefinition *input)
{
    ins->setOperand(0, useRegister(input));
    return define(ins, mir,
                  LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::DEFAULT));

}

bool
LIRGeneratorARM::lowerForFPU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs, MDefinition *rhs)
{
    ins->setOperand(0, useRegister(lhs));
    ins->setOperand(1, useRegister(rhs));
    return define(ins, mir,
                  LDefinition(LDefinition::TypeFrom(mir->type()), LDefinition::DEFAULT));
}

bool
LIRGeneratorARM::lowerForBitAndAndBranch(LBitAndAndBranch *baab, MInstruction *mir,
                                         MDefinition *lhs, MDefinition *rhs)
{
    baab->setOperand(0, useRegister(lhs));
    baab->setOperand(1, useRegisterOrConstant(rhs));
    return add(baab, mir);
}

bool
LIRGeneratorARM::defineUntypedPhi(MPhi *phi, size_t lirIndex)
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
LIRGeneratorARM::lowerUntypedPhiInput(MPhi *phi, uint32_t inputPosition, LBlock *block, size_t lirIndex)
{
    // oh god, what is this code?
    MDefinition *operand = phi->getOperand(inputPosition);
    LPhi *type = block->getPhi(lirIndex + VREG_TYPE_OFFSET);
    LPhi *payload = block->getPhi(lirIndex + VREG_DATA_OFFSET);
    type->setOperand(inputPosition, LUse(operand->virtualRegister() + VREG_TYPE_OFFSET, LUse::ANY));
    payload->setOperand(inputPosition, LUse(VirtualRegisterOfPayload(operand), LUse::ANY));
}

bool
LIRGeneratorARM::lowerForShift(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs, MDefinition *rhs)
{

    ins->setOperand(0, useRegister(lhs));
    ins->setOperand(1, useRegisterOrConstant(rhs));
    return define(ins, mir);
}

bool
LIRGeneratorARM::lowerDivI(MDiv *div)
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
            LDivPowTwoI *lir = new LDivPowTwoI(useRegisterAtStart(div->lhs()), shift);
            if (div->fallible() && !assignSnapshot(lir))
                return false;
            return define(lir, div);
        }
    }

    if (hasIDIV()) {
        LDivI *lir = new LDivI(useRegister(div->lhs()), useRegister(div->rhs()), temp());
        if (div->fallible() && !assignSnapshot(lir))
            return false;
        return define(lir, div);
    } else {
        LSoftDivI *lir = new LSoftDivI(useFixed(div->lhs(), r0), use(div->rhs(), r1),
                                       tempFixed(r2), tempFixed(r3));
        if (div->fallible() && !assignSnapshot(lir))
            return false;
        return defineFixed(lir, div, LAllocation(AnyRegister(r0)));
    }
}

bool
LIRGeneratorARM::lowerMulI(MMul *mul, MDefinition *lhs, MDefinition *rhs)
{
    LMulI *lir = new LMulI;
    if (mul->fallible() && !assignSnapshot(lir))
        return false;
    return lowerForALU(lir, mul, lhs, rhs);
}

bool
LIRGeneratorARM::lowerModI(MMod *mod)
{
    if (mod->isUnsigned())
        return lowerUMod(mod);

    if (mod->rhs()->isConstant()) {
        int32_t rhs = mod->rhs()->toConstant()->value().toInt32();
        int32_t shift = FloorLog2(rhs);
        if (rhs > 0 && 1 << shift == rhs) {
            LModPowTwoI *lir = new LModPowTwoI(useRegister(mod->lhs()), shift);
            if (mod->fallible() && !assignSnapshot(lir))
                return false;
            return define(lir, mod);
        } else if (shift < 31 && (1 << (shift+1)) - 1 == rhs) {
            LModMaskI *lir = new LModMaskI(useRegister(mod->lhs()), temp(LDefinition::GENERAL), shift+1);
            if (mod->fallible() && !assignSnapshot(lir))
                return false;
            return define(lir, mod);
        }
    }

    if (hasIDIV()) {
        LModI *lir = new LModI(useRegister(mod->lhs()), useRegister(mod->rhs()), temp());
        if (mod->fallible() && !assignSnapshot(lir))
            return false;
        return define(lir, mod);
    } else {
        LSoftModI *lir = new LSoftModI(useFixed(mod->lhs(), r0), use(mod->rhs(), r1),
                                       tempFixed(r2), tempFixed(r3), temp(LDefinition::GENERAL));
        if (mod->fallible() && !assignSnapshot(lir))
            return false;
        return defineFixed(lir, mod, LAllocation(AnyRegister(r1)));
    }
}

bool
LIRGeneratorARM::visitPowHalf(MPowHalf *ins)
{
    MDefinition *input = ins->input();
    JS_ASSERT(input->type() == MIRType_Double);
    LPowHalfD *lir = new LPowHalfD(useRegisterAtStart(input));
    return defineReuseInput(lir, ins, 0);
}

LTableSwitch *
LIRGeneratorARM::newLTableSwitch(const LAllocation &in, const LDefinition &inputCopy,
                                       MTableSwitch *tableswitch)
{
    return new LTableSwitch(in, inputCopy, tableswitch);
}

LTableSwitchV *
LIRGeneratorARM::newLTableSwitchV(MTableSwitch *tableswitch)
{
    return new LTableSwitchV(temp(), tempFloat(), tableswitch);
}

LGetPropertyCacheT *
LIRGeneratorARM::newLGetPropertyCacheT(MGetPropertyCache *ins)
{
    return new LGetPropertyCacheT(useRegister(ins->object()), LDefinition::BogusTemp());
}

LGetElementCacheT *
LIRGeneratorARM::newLGetElementCacheT(MGetElementCache *ins)
{
    return new LGetElementCacheT(useRegister(ins->object()),
                                 useRegister(ins->index()),
                                 LDefinition::BogusTemp());
}

bool
LIRGeneratorARM::visitGuardShape(MGuardShape *ins)
{
    JS_ASSERT(ins->obj()->type() == MIRType_Object);

    LDefinition tempObj = temp(LDefinition::OBJECT);
    LGuardShape *guard = new LGuardShape(useRegister(ins->obj()), tempObj);
    if (!assignSnapshot(guard, ins->bailoutKind()))
        return false;
    if (!add(guard, ins))
        return false;
    return redefine(ins, ins->obj());
}

bool
LIRGeneratorARM::visitGuardObjectType(MGuardObjectType *ins)
{
    JS_ASSERT(ins->obj()->type() == MIRType_Object);

    LDefinition tempObj = temp(LDefinition::OBJECT);
    LGuardObjectType *guard = new LGuardObjectType(useRegister(ins->obj()), tempObj);
    if (!assignSnapshot(guard))
        return false;
    if (!add(guard, ins))
        return false;
    return redefine(ins, ins->obj());
}

bool
LIRGeneratorARM::visitStoreTypedArrayElement(MStoreTypedArrayElement *ins)
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
LIRGeneratorARM::visitStoreTypedArrayElementHole(MStoreTypedArrayElementHole *ins)
{
    JS_ASSERT(ins->elements()->type() == MIRType_Elements);
    JS_ASSERT(ins->index()->type() == MIRType_Int32);
    JS_ASSERT(ins->length()->type() == MIRType_Int32);

    if (ins->isFloatArray())
        JS_ASSERT(ins->value()->type() == MIRType_Double);
    else
        JS_ASSERT(ins->value()->type() == MIRType_Int32);

    LUse elements = useRegister(ins->elements());
    LAllocation length = useRegisterOrConstant(ins->length());
    LAllocation index = useRegisterOrConstant(ins->index());
    LAllocation value = useRegisterOrNonDoubleConstant(ins->value());
    return add(new LStoreTypedArrayElementHole(elements, length, index, value), ins);
}

bool
LIRGeneratorARM::lowerUrshD(MUrsh *mir)
{
    MDefinition *lhs = mir->lhs();
    MDefinition *rhs = mir->rhs();

    JS_ASSERT(lhs->type() == MIRType_Int32);
    JS_ASSERT(rhs->type() == MIRType_Int32);

    LUrshD *lir = new LUrshD(useRegister(lhs), useRegisterOrConstant(rhs), temp());
    return define(lir, mir);
}

bool
LIRGeneratorARM::visitAsmJSNeg(MAsmJSNeg *ins)
{
    if (ins->type() == MIRType_Int32)
        return define(new LNegI(useRegisterAtStart(ins->input())), ins);

    JS_ASSERT(ins->type() == MIRType_Double);
    return define(new LNegD(useRegisterAtStart(ins->input())), ins);
}

bool
LIRGeneratorARM::lowerUDiv(MInstruction *div)
{
    MDefinition *lhs = div->getOperand(0);
    MDefinition *rhs = div->getOperand(1);

    if (hasIDIV()) {
        LUDiv *lir = new LUDiv;
        lir->setOperand(0, useRegister(lhs));
        lir->setOperand(1, useRegister(rhs));
        return define(lir, div);
    } else {
        LSoftUDivOrMod *lir = new LSoftUDivOrMod(useFixed(lhs, r0), useFixed(rhs, r1),
                                                 tempFixed(r2), tempFixed(r3));
        return defineFixed(lir, div, LAllocation(AnyRegister(r0)));
    }
}

bool
LIRGeneratorARM::visitAsmJSUDiv(MAsmJSUDiv *div)
{
    return lowerUDiv(div);
}

bool
LIRGeneratorARM::lowerUMod(MInstruction *mod)
{
    MDefinition *lhs = mod->getOperand(0);
    MDefinition *rhs = mod->getOperand(1);

    if (hasIDIV()) {
        LUMod *lir = new LUMod;
        lir->setOperand(0, useRegister(lhs));
        lir->setOperand(1, useRegister(rhs));
        return define(lir, mod);
    } else {
        LSoftUDivOrMod *lir = new LSoftUDivOrMod(useFixed(lhs, r0), useFixed(rhs, r1),
                                                 tempFixed(r2), tempFixed(r3));
        return defineFixed(lir, mod, LAllocation(AnyRegister(r1)));
    }
}

bool
LIRGeneratorARM::visitAsmJSUMod(MAsmJSUMod *mod)
{
    return lowerUMod(mod);
}

bool
LIRGeneratorARM::visitAsmJSUnsignedToDouble(MAsmJSUnsignedToDouble *ins)
{
    JS_ASSERT(ins->input()->type() == MIRType_Int32);
    LUInt32ToDouble *lir = new LUInt32ToDouble(useRegisterAtStart(ins->input()));
    return define(lir, ins);
}

bool
LIRGeneratorARM::visitAsmJSStoreHeap(MAsmJSStoreHeap *ins)
{
    LAsmJSStoreHeap *lir;
    switch (ins->viewType()) {
      case ArrayBufferView::TYPE_INT8: case ArrayBufferView::TYPE_UINT8:
      case ArrayBufferView::TYPE_INT16: case ArrayBufferView::TYPE_UINT16:
      case ArrayBufferView::TYPE_INT32: case ArrayBufferView::TYPE_UINT32:
        lir = new LAsmJSStoreHeap(useRegisterAtStart(ins->ptr()),
                                  useRegisterAtStart(ins->value()));
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
LIRGeneratorARM::visitAsmJSLoadFuncPtr(MAsmJSLoadFuncPtr *ins)
{
    return define(new LAsmJSLoadFuncPtr(useRegister(ins->index()), temp()), ins);
}

bool
LIRGeneratorARM::lowerTruncateDToInt32(MTruncateToInt32 *ins)
{
    MDefinition *opd = ins->input();
    JS_ASSERT(opd->type() == MIRType_Double);

    return define(new LTruncateDToInt32(useRegister(opd), LDefinition::BogusTemp()), ins);
}

bool
LIRGeneratorARM::visitStoreTypedArrayElementStatic(MStoreTypedArrayElementStatic *ins)
{
    MOZ_ASSUME_UNREACHABLE("NYI");
}

//__aeabi_uidiv
