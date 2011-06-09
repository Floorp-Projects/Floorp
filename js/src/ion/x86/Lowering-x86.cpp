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
#include "ion/IonLowering.h"
#include "ion/IonLowering-inl.h"
#include "Lowering-x86.h"
#include "Lowering-x86-inl.h"

using namespace js;
using namespace js::ion;

bool
LIRGeneratorX86::visitConstant(MConstant *ins)
{
    // On x86, moving a double is non-trivial so only do it once.
    if (!ins->inWorklist() && !ins->value().isDouble())
        return emitAtUses(ins);

    return LIRGenerator::visitConstant(ins);
}

bool
LIRGeneratorX86::visitBox(MBox *box)
{
    if (!box->inWorklist())
        return emitAtUses(box);

    // If something really, really wants a box - and it's not snooping for
    // constants, then we go ahead and create two defs. Creating two defs
    // manually is almost never needed so this is a special case. The first is
    // a constant index containing the type, and the second is the payload.
    MInstruction *inner = box->getInput(0);

    uint32 type_vreg = nextVirtualRegister();
    uint32 data_vreg = nextVirtualRegister();
    if (data_vreg >= MAX_VIRTUAL_REGISTERS)
        return false;

    box->setId(type_vreg);

    LDefinition out_type(LDefinition::TYPE, LDefinition::PRESET);
    out_type.setVirtualRegister(type_vreg);
    out_type.setOutput(LConstantIndex(inner->type()));

    LDefinition out_payload;
    if (inner->isConstant()) {
        out_payload = LDefinition(LDefinition::PAYLOAD, LDefinition::PRESET);
        out_payload.setVirtualRegister(data_vreg);
        out_payload.setOutput(LAllocation(inner->toConstant()->vp()));
    } else {
        out_payload = LDefinition(data_vreg, LDefinition::PAYLOAD);
    }

    return add(new LBox(useOrConstant(inner), out_type, out_payload));
}

bool
LIRGeneratorX86::visitUnbox(MUnbox *unbox)
{
    // An unbox on x86 reads in a type tag (either in memory or a register) and
    // a payload. Unlike most instructions conusming a box, we ask for the type
    // second, so that the result can re-use the first input.
    MInstruction *inner = unbox->getInput(0);

    if (unbox->type() == MIRType_Double) {
        LBoxToDouble *lir = new LBoxToDouble;
        startUsing(inner);
        lir->setOperand(0, usePayloadInRegister(inner));
        lir->setOperand(1, useType(inner));
        lir->setTemp(0, temp(LDefinition::DOUBLE));
        stopUsing(inner);
        return define(lir, unbox, LDefinition::DEFAULT);
    }

    LUnbox *lir = new LUnbox(unbox->type());
    lir->setOperand(0, usePayloadInRegister(inner));
    lir->setOperand(1, useType(inner));
    return define(lir, unbox, LDefinition::CAN_REUSE_INPUT);
}

bool
LIRGeneratorX86::visitReturn(MReturn *ret)
{
    MInstruction *opd = ret->getInput(0);
    JS_ASSERT(opd->type() == MIRType_Value);

    LReturn *ins = new LReturn;
    ins->setOperand(0, LUse(JSReturnReg_Type));
    ins->setOperand(1, LUse(JSReturnReg_Data));
    fillBoxUses(ins, 0, opd);
    return add(ins);
}

