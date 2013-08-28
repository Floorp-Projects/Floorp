/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_shared_CodeGenerator_shared_inl_h
#define jit_shared_CodeGenerator_shared_inl_h

#include "jit/shared/CodeGenerator-shared.h"

namespace js {
namespace jit {

static inline int32_t
ToInt32(const LAllocation *a)
{
    if (a->isConstantValue())
        return a->toConstant()->toInt32();
    if (a->isConstantIndex())
        return a->toConstantIndex()->index();
    MOZ_ASSUME_UNREACHABLE("this is not a constant!");
}
static inline double
ToDouble(const LAllocation *a)
{
    return a->toConstant()->toNumber();
}

static inline Register
ToRegister(const LAllocation &a)
{
    JS_ASSERT(a.isGeneralReg());
    return a.toGeneralReg()->reg();
}

static inline Register
ToRegister(const LAllocation *a)
{
    return ToRegister(*a);
}

static inline Register
ToRegister(const LDefinition *def)
{
    return ToRegister(*def->output());
}

static inline Register
ToTempUnboxRegister(const LDefinition *def)
{
    if (def->isBogusTemp())
        return InvalidReg;
    return ToRegister(def);
}

static inline Register
ToRegisterOrInvalid(const LAllocation *a)
{
    return a ? ToRegister(*a) : InvalidReg;
}

static inline FloatRegister
ToFloatRegister(const LAllocation &a)
{
    JS_ASSERT(a.isFloatReg());
    return a.toFloatReg()->reg();
}

static inline FloatRegister
ToFloatRegister(const LAllocation *a)
{
    return ToFloatRegister(*a);
}

static inline FloatRegister
ToFloatRegister(const LDefinition *def)
{
    return ToFloatRegister(*def->output());
}

static inline AnyRegister
ToAnyRegister(const LAllocation &a)
{
    JS_ASSERT(a.isGeneralReg() || a.isFloatReg());
    if (a.isGeneralReg())
        return AnyRegister(ToRegister(a));
    return AnyRegister(ToFloatRegister(a));
}

static inline AnyRegister
ToAnyRegister(const LAllocation *a)
{
    return ToAnyRegister(*a);
}

static inline AnyRegister
ToAnyRegister(const LDefinition *def)
{
    return ToAnyRegister(def->output());
}

static inline Int32Key
ToInt32Key(const LAllocation *a)
{
    if (a->isConstant())
        return Int32Key(ToInt32(a));
    return Int32Key(ToRegister(a));
}

static inline ValueOperand
GetValueOutput(LInstruction *ins)
{
#if defined(JS_NUNBOX32)
    return ValueOperand(ToRegister(ins->getDef(TYPE_INDEX)),
                        ToRegister(ins->getDef(PAYLOAD_INDEX)));
#elif defined(JS_PUNBOX64)
    return ValueOperand(ToRegister(ins->getDef(0)));
#else
#error "Unknown"
#endif
}

static inline ValueOperand
GetTempValue(const Register &type, const Register &payload)
{
#if defined(JS_NUNBOX32)
    return ValueOperand(type, payload);
#elif defined(JS_PUNBOX64)
    (void)type;
    return ValueOperand(payload);
#else
#error "Unknown"
#endif
}

void
CodeGeneratorShared::saveLive(LInstruction *ins)
{
    JS_ASSERT(!ins->isCall());
    LSafepoint *safepoint = ins->safepoint();
    masm.PushRegsInMask(safepoint->liveRegs());
}

void
CodeGeneratorShared::restoreLive(LInstruction *ins)
{
    JS_ASSERT(!ins->isCall());
    LSafepoint *safepoint = ins->safepoint();
    masm.PopRegsInMask(safepoint->liveRegs());
}

void
CodeGeneratorShared::restoreLiveIgnore(LInstruction *ins, RegisterSet ignore)
{
    JS_ASSERT(!ins->isCall());
    LSafepoint *safepoint = ins->safepoint();
    masm.PopRegsInMaskIgnore(safepoint->liveRegs(), ignore);
}

} // ion
} // js

#endif /* jit_shared_CodeGenerator_shared_inl_h */
