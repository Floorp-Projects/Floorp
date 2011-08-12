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

#ifndef jsion_ion_lowering_inl_h__
#define jsion_ion_lowering_inl_h__

#include "ion/MIR.h"
#include "ion/MIRGraph.h"

namespace js {
namespace ion {

bool
LIRGeneratorShared::emitAtUses(MInstruction *mir)
{
    mir->setEmittedAtUses();
    mir->setVirtualRegister(0);
    return true;
}

LUse
LIRGeneratorShared::use(MDefinition *mir, LUse policy)
{
    // It is illegal to call use() on an instruction with two defs.
#if BOX_PIECES > 1
    JS_ASSERT(mir->type() != MIRType_Value);
#endif
    if (!ensureDefined(mir))
        return policy;
    policy.setVirtualRegister(mir->virtualRegister());
    return policy;
}

template <size_t X, size_t Y> bool
LIRGeneratorShared::define(LInstructionHelper<1, X, Y> *lir, MDefinition *mir, const LDefinition &def)
{
    uint32 vreg = getVirtualRegister();
    if (vreg >= MAX_VIRTUAL_REGISTERS)
        return false;

    // Assign the definition and a virtual register. Then, propagate this
    // virtual register to the MIR, so we can map MIR to LIR during lowering.
    lir->setDef(0, def);
    lir->getDef(0)->setVirtualRegister(vreg);
    mir->setVirtualRegister(vreg);
    return add(lir);
}

template <size_t X, size_t Y> bool
LIRGeneratorShared::define(LInstructionHelper<1, X, Y> *lir, MDefinition *mir, LDefinition::Policy policy)
{
    LDefinition::Type type = LDefinition::TypeFrom(mir->type());
    return define(lir, mir, LDefinition(type, policy));
}

template <size_t Ops, size_t Temps> bool
LIRGeneratorShared::defineReuseInput(LInstructionHelper<1, Ops, Temps> *lir, MDefinition *mir)
{
    return define(lir, mir, LDefinition::MUST_REUSE_INPUT);
}

template <size_t Ops, size_t Temps> bool
LIRGeneratorShared::defineBox(LInstructionHelper<BOX_PIECES, Ops, Temps> *lir, MDefinition *mir,
                              LDefinition::Policy policy)
{
    uint32 vreg = getVirtualRegister();
    if (vreg >= MAX_VIRTUAL_REGISTERS)
        return false;

#if defined(JS_NUNBOX32)
    lir->setDef(0, LDefinition(vreg + VREG_TYPE_OFFSET, LDefinition::TYPE, policy));
    lir->setDef(1, LDefinition(vreg + VREG_DATA_OFFSET, LDefinition::PAYLOAD, policy));
    if (getVirtualRegister() >= MAX_VIRTUAL_REGISTERS)
        return false;
#elif defined(JS_PUNBOX64)
    lir->setDef(0, LDefinition(vreg, LDefinition::BOX, policy));
#endif

    mir->setVirtualRegister(vreg);
    return add(lir);
}

template <size_t Ops, size_t Temps> bool
LIRGeneratorShared::defineReturn(LInstructionHelper<BOX_PIECES, Ops, Temps> *lir, MDefinition *mir)
{
    defineBox(lir, mir, LDefinition::PRESET);

#if defined(JS_NUNBOX32)
    lir->getDef(TYPE_INDEX)->setOutput(LGeneralReg(JSReturnReg_Type));
    lir->getDef(PAYLOAD_INDEX)->setOutput(LGeneralReg(JSReturnReg_Data));
#elif defined(JS_PUNBOX64)
    lir->getDef(0)->setOutput(LGeneralReg(JSReturnReg));
#endif

    return true;
}

// In LIR, we treat booleans and integers as the same low-level type (INTEGER).
// When snapshotting, we recover the actual JS type from MIR. This function
// checks that when making redefinitions, we don't accidentally coerce two
// incompatible types.
static inline bool
IsCompatibleLIRCoercion(MIRType to, MIRType from)
{
    if (to == from)
        return true;
    if ((to == MIRType_Int32 || to == MIRType_Boolean) &&
        (from == MIRType_Int32 || from == MIRType_Boolean)) {
        return true;
    }
    return false;
}

bool
LIRGeneratorShared::redefine(MDefinition *def, MDefinition *as)
{
    JS_ASSERT(IsCompatibleLIRCoercion(def->type(), as->type()));
    if (!ensureDefined(as))
        return false;
    def->setVirtualRegister(as->virtualRegister());
    return true;
}

bool
LIRGeneratorShared::ensureDefined(MDefinition *mir)
{
    if (mir->isEmittedAtUses()) {
        if (!mir->toInstruction()->accept(this))
            return false;
        JS_ASSERT(mir->isLowered());
    }
    return true;
}

LUse
LIRGeneratorShared::useCopy(MDefinition *mir)
{
    return use(mir, LUse(LUse::COPY));
}

LUse
LIRGeneratorShared::useRegister(MDefinition *mir)
{
    return use(mir, LUse(LUse::REGISTER));
}

LUse
LIRGeneratorShared::use(MDefinition *mir)
{
    return use(mir, LUse(LUse::ANY));
}

LAllocation
LIRGeneratorShared::useOrConstant(MDefinition *mir)
{
    if (mir->isConstant())
        return LAllocation(mir->toConstant()->vp());
    return use(mir);
}

LAllocation
LIRGeneratorShared::useRegisterOrConstant(MDefinition *mir)
{
    if (mir->isConstant())
        return LAllocation(mir->toConstant()->vp());
    return use(mir, LUse(LUse::REGISTER));
}

LAllocation
LIRGeneratorShared::useKeepaliveOrConstant(MDefinition *mir)
{
    if (mir->isConstant())
        return LAllocation(mir->toConstant()->vp());
    return use(mir, LUse(LUse::KEEPALIVE));
}

LUse
LIRGeneratorShared::useFixed(MDefinition *mir, Register reg)
{
    return use(mir, LUse(reg));
}

LUse
LIRGeneratorShared::useFixed(MDefinition *mir, FloatRegister reg)
{
    return use(mir, LUse(reg));
}

LDefinition
LIRGeneratorShared::temp(LDefinition::Type type)
{
    uint32 vreg = getVirtualRegister();
    if (vreg >= MAX_VIRTUAL_REGISTERS) {
        gen->abort("max virtual registers");
        return LDefinition();
    }
    return LDefinition(vreg, type);
}

LDefinition
LIRGeneratorShared::tempFloat()
{
    return temp(LDefinition::DOUBLE);
}

template <typename T> bool
LIRGeneratorShared::annotate(T *ins)
{
    for (size_t i = 0; i < ins->numDefs(); i++) {
        if (ins->getDef(i)->policy() != LDefinition::REDEFINED) {
            ins->setId(ins->getDef(i)->virtualRegister());
            return true;
        }
    }

    ins->setId(getVirtualRegister());
    return ins->id() < MAX_VIRTUAL_REGISTERS;
}

template <typename T> bool
LIRGeneratorShared::add(T *ins)
{
    JS_ASSERT(!ins->isPhi());
    current->add(ins);
    return annotate(ins);
}

} // namespace js
} // namespace ion

#endif // jsion_ion_lowering_inl_h__

