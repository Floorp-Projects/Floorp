/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <danderson@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "ion/MIR.h"
#include "ion/Lowering.h"
#include "Assembler-x86.h"
#include "ion/shared/Lowering-shared-inl.h"
#include "Lowering-x86-inl.h"

using namespace js;
using namespace js::ion;

bool
LIRGeneratorX86::visitConstant(MConstant *ins)
{
    if (!ins->isEmittedAtUses() && ins->type() != MIRType_Double)
        return emitAtUses(ins);

    if (ins->type() == MIRType_Double)
        return define(new LDouble(ins->value().toDouble()), ins);

    return LIRGeneratorShared::visitConstant(ins);
}

bool
LIRGeneratorX86::visitBox(MBox *box)
{
    MDefinition *inner = box->getOperand(0);

    // If the box wrapped a double, it needs a new register.
    if (inner->type() == MIRType_Double)
        return defineBox(new LBoxDouble(use(inner)), box);

    if (!box->isEmittedAtUses())
        return emitAtUses(box);

    if (inner->isConstant())
        return defineBox(new LValue(inner->toConstant()->value()), box);

    LBox *lir = new LBox(use(inner), inner->type());

    // Otherwise, we should not define a new register for the payload portion
    // of the output, so bypass defineBox().
    uint32 vreg = getVirtualRegister();
    if (vreg >= MAX_VIRTUAL_REGISTERS)
        return false;

    lir->setDef(0, LDefinition(vreg, LDefinition::TYPE));
    lir->setDef(1, LDefinition(inner->id(), LDefinition::PAYLOAD, LDefinition::REDEFINED));
    box->setId(vreg);
    return add(lir);
}

bool
LIRGeneratorX86::visitUnbox(MUnbox *unbox)
{
    // An unbox on x86 reads in a type tag (either in memory or a register) and
    // a payload. Unlike most instructions conusming a box, we ask for the type
    // second, so that the result can re-use the first input.
    MDefinition *inner = unbox->getOperand(0);

    if (unbox->type() == MIRType_Double) {
        if (!ensureDefined(inner))
            return false;

        if (Assembler::HasSSE41()) {
            // With SSE4.1, we can emit a faster sequence of instructions that
            // does not need a temporary register.
            LUnboxDoubleSSE41 *lir = new LUnboxDoubleSSE41();
            lir->setOperand(0, useType(inner, LUse::ANY));
            lir->setOperand(1, usePayloadInRegister(inner));
            if (!assignSnapshot(lir))
                return false;
            return define(lir, unbox, LDefinition::DEFAULT);
        }

        // With SSE < 4.1, use a slower (but still very efficient) sequence of
        // instructions that needs a temporary register. Note that it also
        // needs to read the type twice so we keep it in a register.
        LUnboxDouble *lir = new LUnboxDouble();
        lir->setOperand(0, useType(inner, LUse::REGISTER));
        lir->setOperand(1, usePayloadInRegister(inner));
        if (!assignSnapshot(lir))
            return false;
        return define(lir, unbox, LDefinition::DEFAULT);
    }

    LUnbox *lir = new LUnbox(unbox->type());
    lir->setOperand(0, useType(inner, LUse::ANY));
    lir->setOperand(1, usePayloadInRegister(inner));

    // Re-use the inner payload's def, for better register allocation.
    LDefinition::Type type = LDefinition::TypeFrom(unbox->type());
    lir->setDef(0, LDefinition(VirtualRegisterOfPayload(inner), type, LDefinition::REDEFINED));
    unbox->setId(VirtualRegisterOfPayload(inner));

    return assignSnapshot(lir) && add(lir);
}

bool
LIRGeneratorX86::visitReturn(MReturn *ret)
{
    MDefinition *opd = ret->getOperand(0);
    JS_ASSERT(opd->type() == MIRType_Value);

    LReturn *ins = new LReturn;
    ins->setOperand(0, LUse(JSReturnReg_Type));
    ins->setOperand(1, LUse(JSReturnReg_Data));
    return fillBoxUses(ins, 0, opd) && add(ins);
}

bool
LIRGeneratorX86::preparePhi(MPhi *phi)
{
    uint32 first_vreg = getVirtualRegister();
    if (first_vreg >= MAX_VIRTUAL_REGISTERS)
        return false;

    phi->setId(first_vreg);

    if (phi->type() == MIRType_Value) {
        uint32 payload_vreg = getVirtualRegister();
        if (payload_vreg >= MAX_VIRTUAL_REGISTERS)
            return false;
        JS_ASSERT(first_vreg + VREG_DATA_OFFSET == payload_vreg);
    }

    return true;
}

bool
LIRGeneratorX86::lowerPhi(MPhi *ins)
{
    JS_ASSERT(ins->isInWorklist() && ins->id());

    // Typed phis can be handled much simpler.
    if (ins->type() != MIRType_Value)
        return LIRGeneratorShared::lowerPhi(ins);

    // Otherwise, we create two phis: one for the set of types and one for the
    // set of payloads. They form two separate instructions but their
    // definitions are paired such that they act as one. That is, the type phi
    // has vreg V and the data phi has vreg V++.
    LPhi *type = LPhi::New(gen, ins);
    LPhi *payload = LPhi::New(gen, ins);
    if (!type || !payload)
        return false;

    for (size_t i = 0; i < ins->numOperands(); i++) {
        MDefinition *opd = ins->getOperand(i);
        JS_ASSERT(opd->type() == MIRType_Value);
        JS_ASSERT(opd->id());
        JS_ASSERT(opd->isInWorklist());

        type->setOperand(i, LUse(opd->id() + VREG_TYPE_OFFSET, LUse::ANY));
        payload->setOperand(i, LUse(VirtualRegisterOfPayload(opd), LUse::ANY));
    }

    type->setDef(0, LDefinition(ins->id() + VREG_TYPE_OFFSET, LDefinition::TYPE));
    payload->setDef(0, LDefinition(ins->id() + VREG_DATA_OFFSET, LDefinition::PAYLOAD));
    return addPhi(type) && addPhi(payload);
}

bool
LIRGeneratorX86::assignSnapshot(LInstruction *ins)
{
    LSnapshot *snapshot = LSnapshot::New(gen, last_snapshot_);
    if (!snapshot)
        return false;

    MSnapshot *mir = snapshot->mir();
    for (size_t i = 0; i < mir->numOperands(); i++) {
        MDefinition *ins = mir->getOperand(i);
        LAllocation *type = snapshot->getEntry(i * 2);
        LAllocation *payload = snapshot->getEntry(i * 2 + 1);

        // The register allocation will fill these fields in with actual
        // register/stack assignments. During code generation, we can restore
        // interpreter state with the given information. Note that for
        // constants, including known types, we record a dummy placeholder,
        // since we can recover the same information, much cleaner, from MIR.
        if (ins->isConstant()) {
            *type = LConstantIndex(0);
            *payload = LConstantIndex(0);
        } else if (ins->type() != MIRType_Value) {
            *type = LConstantIndex(0);
            *payload = use(ins, LUse::KEEPALIVE);
        } else {
            *type = useType(ins, LUse::KEEPALIVE);
            *payload = usePayload(ins, LUse::KEEPALIVE);
        }
    }

    ins->assignSnapshot(snapshot);
    return true;
}

bool
LIRGeneratorX86::lowerForALU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs, MDefinition *rhs)
{
    ins->setOperand(0, useRegister(lhs));
    ins->setOperand(1, useOrConstant(rhs));
    return defineReuseInput(ins, mir);
}

bool
LIRGeneratorX86::lowerForFPU(LInstructionHelper<1, 2, 0> *ins, MDefinition *mir, MDefinition *lhs, MDefinition *rhs)
{
    ins->setOperand(0, useRegister(lhs));
    ins->setOperand(1, use(rhs));
    return defineReuseInput(ins, mir);
}

