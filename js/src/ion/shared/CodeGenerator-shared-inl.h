/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsion_codegen_inl_h__
#define jsion_codegen_inl_h__

namespace js {
namespace ion {

static inline int32
ToInt32(const LAllocation *a)
{
    if (a->isConstantValue())
        return a->toConstant()->toInt32();
    if (a->isConstantIndex())
        return a->toConstantIndex()->index();
    JS_NOT_REACHED("this is not a constant!");
    return -1;
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

static inline Assembler::Condition
JSOpToCondition(JSOp op)
{
    switch (op) {
      case JSOP_EQ:
      case JSOP_STRICTEQ:
        return Assembler::Equal;
      case JSOP_NE:
      case JSOP_STRICTNE:
        return Assembler::NotEqual;
      case JSOP_LT:
        return Assembler::LessThan;
      case JSOP_LE:
        return Assembler::LessThanOrEqual;
      case JSOP_GT:
        return Assembler::GreaterThan;
      case JSOP_GE:
        return Assembler::GreaterThanOrEqual;
      default:
        JS_NOT_REACHED("Unrecognized comparison operation");
        return Assembler::Equal;
    }
}

static inline Assembler::DoubleCondition
JSOpToDoubleCondition(JSOp op)
{
    switch (op) {
      case JSOP_EQ:
      case JSOP_STRICTEQ:
        return Assembler::DoubleEqual;
      case JSOP_NE:
      case JSOP_STRICTNE:
        return Assembler::DoubleNotEqualOrUnordered;
      case JSOP_LT:
        return Assembler::DoubleLessThan;
      case JSOP_LE:
        return Assembler::DoubleLessThanOrEqual;
      case JSOP_GT:
        return Assembler::DoubleGreaterThan;
      case JSOP_GE:
        return Assembler::DoubleGreaterThanOrEqual;
      default:
        JS_NOT_REACHED("Unexpected comparison operation");
        return Assembler::DoubleEqual;
    }
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

} // ion
} // js

#endif // jsion_codegen_inl_h__

