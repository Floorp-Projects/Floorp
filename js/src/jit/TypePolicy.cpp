/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/TypePolicy.h"

#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

using namespace js;
using namespace js::jit;

using JS::DoubleNaNValue;

static void
EnsureOperandNotFloat32(TempAllocator &alloc, MInstruction *def, unsigned op)
{
    MDefinition *in = def->getOperand(op);
    if (in->type() == MIRType_Float32) {
        MToDouble *replace = MToDouble::New(alloc, in);
        def->block()->insertBefore(def, replace);
        def->replaceOperand(op, replace);
    }
}

MDefinition *
BoxInputsPolicy::boxAt(TempAllocator &alloc, MInstruction *at, MDefinition *operand)
{
    if (operand->isUnbox())
        return operand->toUnbox()->input();
    return alwaysBoxAt(alloc, at, operand);
}

MDefinition *
BoxInputsPolicy::alwaysBoxAt(TempAllocator &alloc, MInstruction *at, MDefinition *operand)
{
    MDefinition *boxedOperand = operand;
    // Replace Float32 by double
    if (operand->type() == MIRType_Float32) {
        MInstruction *replace = MToDouble::New(alloc, operand);
        at->block()->insertBefore(at, replace);
        boxedOperand = replace;
    }
    MBox *box = MBox::New(alloc, boxedOperand);
    at->block()->insertBefore(at, box);
    return box;
}

bool
BoxInputsPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    for (size_t i = 0, e = ins->numOperands(); i < e; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;
        ins->replaceOperand(i, boxAt(alloc, ins, in));
    }
    return true;
}

bool
ArithPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MIRType specialization = ins->typePolicySpecialization();
    if (specialization == MIRType_None)
        return BoxInputsPolicy::adjustInputs(alloc, ins);

    MOZ_ASSERT(ins->type() == MIRType_Double || ins->type() == MIRType_Int32 || ins->type() == MIRType_Float32);

    for (size_t i = 0, e = ins->numOperands(); i < e; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == ins->type())
            continue;

        MInstruction *replace;

        if (ins->type() == MIRType_Double)
            replace = MToDouble::New(alloc, in);
        else if (ins->type() == MIRType_Float32)
            replace = MToFloat32::New(alloc, in);
        else
            replace = MToInt32::New(alloc, in);

        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);

        if (!replace->typePolicy()->adjustInputs(alloc, replace))
            return false;
    }

    return true;
}

bool
ComparePolicy::adjustInputs(TempAllocator &alloc, MInstruction *def)
{
    MOZ_ASSERT(def->isCompare());
    MCompare *compare = def->toCompare();

    // Convert Float32 operands to doubles
    for (size_t i = 0; i < 2; i++) {
        MDefinition *in = def->getOperand(i);
        if (in->type() == MIRType_Float32) {
            MInstruction *replace = MToDouble::New(alloc, in);
            def->block()->insertBefore(def, replace);
            def->replaceOperand(i, replace);
        }
    }

    // Box inputs to get value
    if (compare->compareType() == MCompare::Compare_Unknown ||
        compare->compareType() == MCompare::Compare_Value)
    {
        return BoxInputsPolicy::adjustInputs(alloc, def);
    }

    // Compare_Boolean specialization is done for "Anything === Bool"
    // If the LHS is boolean, we set the specialization to Compare_Int32.
    // This matches other comparisons of the form bool === bool and
    // generated code of Compare_Int32 is more efficient.
    if (compare->compareType() == MCompare::Compare_Boolean &&
        def->getOperand(0)->type() == MIRType_Boolean)
    {
       compare->setCompareType(MCompare::Compare_Int32MaybeCoerceBoth);
    }

    // Compare_Boolean specialization is done for "Anything === Bool"
    // As of previous line Anything can't be Boolean
    if (compare->compareType() == MCompare::Compare_Boolean) {
        // Unbox rhs that is definitely Boolean
        MDefinition *rhs = def->getOperand(1);
        if (rhs->type() != MIRType_Boolean) {
            MInstruction *unbox = MUnbox::New(alloc, rhs, MIRType_Boolean, MUnbox::Infallible);
            def->block()->insertBefore(def, unbox);
            def->replaceOperand(1, unbox);
            if (!unbox->typePolicy()->adjustInputs(alloc, unbox))
                return false;
        }

        MOZ_ASSERT(def->getOperand(0)->type() != MIRType_Boolean);
        MOZ_ASSERT(def->getOperand(1)->type() == MIRType_Boolean);
        return true;
    }

    // Compare_StrictString specialization is done for "Anything === String"
    // If the LHS is string, we set the specialization to Compare_String.
    if (compare->compareType() == MCompare::Compare_StrictString &&
        def->getOperand(0)->type() == MIRType_String)
    {
       compare->setCompareType(MCompare::Compare_String);
    }

    // Compare_StrictString specialization is done for "Anything === String"
    // As of previous line Anything can't be String
    if (compare->compareType() == MCompare::Compare_StrictString) {
        // Unbox rhs that is definitely String
        MDefinition *rhs = def->getOperand(1);
        if (rhs->type() != MIRType_String) {
            MInstruction *unbox = MUnbox::New(alloc, rhs, MIRType_String, MUnbox::Infallible);
            def->block()->insertBefore(def, unbox);
            def->replaceOperand(1, unbox);
            if (!unbox->typePolicy()->adjustInputs(alloc, unbox))
                return false;
        }

        MOZ_ASSERT(def->getOperand(0)->type() != MIRType_String);
        MOZ_ASSERT(def->getOperand(1)->type() == MIRType_String);
        return true;
    }

    if (compare->compareType() == MCompare::Compare_Undefined ||
        compare->compareType() == MCompare::Compare_Null)
    {
        // Nothing to do for undefined and null, lowering handles all types.
        return true;
    }

    // Convert all inputs to the right input type
    MIRType type = compare->inputType();
    MOZ_ASSERT(type == MIRType_Int32 || type == MIRType_Double ||
               type == MIRType_Object || type == MIRType_String || type == MIRType_Float32);
    for (size_t i = 0; i < 2; i++) {
        MDefinition *in = def->getOperand(i);
        if (in->type() == type)
            continue;

        MInstruction *replace;

        switch (type) {
          case MIRType_Double: {
            MToFPInstruction::ConversionKind convert = MToFPInstruction::NumbersOnly;
            if (compare->compareType() == MCompare::Compare_DoubleMaybeCoerceLHS && i == 0)
                convert = MToFPInstruction::NonNullNonStringPrimitives;
            else if (compare->compareType() == MCompare::Compare_DoubleMaybeCoerceRHS && i == 1)
                convert = MToFPInstruction::NonNullNonStringPrimitives;
            replace = MToDouble::New(alloc, in, convert);
            break;
          }
          case MIRType_Float32: {
            MToFPInstruction::ConversionKind convert = MToFPInstruction::NumbersOnly;
            if (compare->compareType() == MCompare::Compare_DoubleMaybeCoerceLHS && i == 0)
                convert = MToFPInstruction::NonNullNonStringPrimitives;
            else if (compare->compareType() == MCompare::Compare_DoubleMaybeCoerceRHS && i == 1)
                convert = MToFPInstruction::NonNullNonStringPrimitives;
            replace = MToFloat32::New(alloc, in, convert);
            break;
          }
          case MIRType_Int32: {
            MacroAssembler::IntConversionInputKind convert = MacroAssembler::IntConversion_NumbersOnly;
            if (compare->compareType() == MCompare::Compare_Int32MaybeCoerceBoth ||
                (compare->compareType() == MCompare::Compare_Int32MaybeCoerceLHS && i == 0) ||
                (compare->compareType() == MCompare::Compare_Int32MaybeCoerceRHS && i == 1))
            {
                convert = MacroAssembler::IntConversion_NumbersOrBoolsOnly;
            }
            replace = MToInt32::New(alloc, in, convert);
            break;
          }
          case MIRType_Object:
            replace = MUnbox::New(alloc, in, MIRType_Object, MUnbox::Infallible);
            break;
          case MIRType_String:
            replace = MUnbox::New(alloc, in, MIRType_String, MUnbox::Infallible);
            break;
          default:
            MOZ_CRASH("Unknown compare specialization");
        }

        def->block()->insertBefore(def, replace);
        def->replaceOperand(i, replace);

        if (!replace->typePolicy()->adjustInputs(alloc, replace))
            return false;
    }

    return true;
}

bool
TypeBarrierPolicy::adjustInputs(TempAllocator &alloc, MInstruction *def)
{
    MTypeBarrier *ins = def->toTypeBarrier();
    MIRType inputType = ins->getOperand(0)->type();
    MIRType outputType = ins->type();

    // Input and output type are already in accordance.
    if (inputType == outputType)
        return true;

    // Output is a value, currently box the input.
    if (outputType == MIRType_Value) {
        // XXX: Possible optimization: decrease resultTypeSet to only include
        // the inputType. This will remove the need for boxing.
        MOZ_ASSERT(inputType != MIRType_Value);
        ins->replaceOperand(0, boxAt(alloc, ins, ins->getOperand(0)));
        return true;
    }

    // Input is a value. Unbox the input to the requested type.
    if (inputType == MIRType_Value) {
        MOZ_ASSERT(outputType != MIRType_Value);

        // We can't unbox a value to null/undefined/lazyargs. So keep output
        // also a value.
        if (IsNullOrUndefined(outputType) || outputType == MIRType_MagicOptimizedArguments) {
            MOZ_ASSERT(!ins->hasDefUses());
            ins->setResultType(MIRType_Value);
            return true;
        }

        MUnbox *unbox = MUnbox::New(alloc, ins->getOperand(0), outputType, MUnbox::TypeBarrier);
        ins->block()->insertBefore(ins, unbox);

        // The TypeBarrier is equivalent to removing branches with unexpected
        // types.  The unexpected types would have changed Range Analysis
        // predictions.  As such, we need to prevent destructive optimizations.
        ins->block()->flagOperandsOfPrunedBranches(unbox);

        ins->replaceOperand(0, unbox);
        return true;
    }

    // In the remaining cases we will alway bail. OutputType doesn't matter.
    // Take inputType so we can use redefine during lowering.
    MOZ_ASSERT(ins->alwaysBails());
    ins->setResultType(inputType);

    return true;
}

bool
TestPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MDefinition *op = ins->getOperand(0);
    switch (op->type()) {
      case MIRType_Value:
      case MIRType_Null:
      case MIRType_Undefined:
      case MIRType_Boolean:
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Float32:
      case MIRType_Symbol:
      case MIRType_Object:
        break;

      case MIRType_String:
      {
        MStringLength *length = MStringLength::New(alloc, op);
        ins->block()->insertBefore(ins, length);
        ins->replaceOperand(0, length);
        break;
      }

      default:
        ins->replaceOperand(0, boxAt(alloc, ins, op));
        break;
    }
    return true;
}

bool
BitwisePolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MIRType specialization = ins->typePolicySpecialization();
    if (specialization == MIRType_None)
        return BoxInputsPolicy::adjustInputs(alloc, ins);

    MOZ_ASSERT(ins->type() == specialization);
    MOZ_ASSERT(specialization == MIRType_Int32 || specialization == MIRType_Double);

    // This policy works for both unary and binary bitwise operations.
    for (size_t i = 0, e = ins->numOperands(); i < e; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Int32)
            continue;

        MInstruction *replace = MTruncateToInt32::New(alloc, in);
        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);

        if (!replace->typePolicy()->adjustInputs(alloc, replace))
            return false;
    }

    return true;
}

bool
PowPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MIRType specialization = ins->typePolicySpecialization();
    MOZ_ASSERT(specialization == MIRType_Int32 || specialization == MIRType_Double);

    // Input must be a double.
    if (!DoublePolicy<0>::staticAdjustInputs(alloc, ins))
        return false;

    // Power may be an int32 or a double. Integers receive a faster path.
    if (specialization == MIRType_Double)
        return DoublePolicy<1>::staticAdjustInputs(alloc, ins);
    return IntPolicy<1>::staticAdjustInputs(alloc, ins);
}

template <unsigned Op>
bool
StringPolicy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == MIRType_String)
        return true;

    MUnbox *replace = MUnbox::New(alloc, in, MIRType_String, MUnbox::Fallible);
    ins->block()->insertBefore(ins, replace);
    ins->replaceOperand(Op, replace);

    return replace->typePolicy()->adjustInputs(alloc, replace);
}

template bool StringPolicy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
template bool StringPolicy<1>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
template bool StringPolicy<2>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);

template <unsigned Op>
bool
ConvertToStringPolicy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == MIRType_String)
        return true;

    MToString *replace = MToString::New(alloc, in);
    ins->block()->insertBefore(ins, replace);
    ins->replaceOperand(Op, replace);

    if (!ToStringPolicy::staticAdjustInputs(alloc, replace))
        return false;

    return true;
}

template bool ConvertToStringPolicy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
template bool ConvertToStringPolicy<1>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);

template <unsigned Op>
bool
IntPolicy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Int32)
        return true;

    MUnbox *replace = MUnbox::New(alloc, in, MIRType_Int32, MUnbox::Fallible);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);

    return replace->typePolicy()->adjustInputs(alloc, replace);
}

template bool IntPolicy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
template bool IntPolicy<1>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
template bool IntPolicy<2>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);

template <unsigned Op>
bool
ConvertToInt32Policy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Int32)
        return true;

    MToInt32 *replace = MToInt32::New(alloc, in);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);

    return replace->typePolicy()->adjustInputs(alloc, replace);
}

template bool ConvertToInt32Policy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);

template <unsigned Op>
bool
DoublePolicy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Double)
        return true;

    MToDouble *replace = MToDouble::New(alloc, in);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);

    return replace->typePolicy()->adjustInputs(alloc, replace);
}

template bool DoublePolicy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
template bool DoublePolicy<1>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);

template <unsigned Op>
bool
Float32Policy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Float32)
        return true;

    MToFloat32 *replace = MToFloat32::New(alloc, in);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);

    return replace->typePolicy()->adjustInputs(alloc, replace);
}

template bool Float32Policy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
template bool Float32Policy<1>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
template bool Float32Policy<2>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);

template <unsigned Op>
bool
FloatingPointPolicy<Op>::adjustInputs(TempAllocator &alloc, MInstruction *def)
{
    MIRType policyType = def->typePolicySpecialization();
    if (policyType == MIRType_Double)
        return DoublePolicy<Op>::staticAdjustInputs(alloc, def);
    return Float32Policy<Op>::staticAdjustInputs(alloc, def);
}

template bool FloatingPointPolicy<0>::adjustInputs(TempAllocator &alloc, MInstruction *def);

template <unsigned Op>
bool
NoFloatPolicy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def)
{
    EnsureOperandNotFloat32(alloc, def, Op);
    return true;
}

template bool NoFloatPolicy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
template bool NoFloatPolicy<1>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
template bool NoFloatPolicy<2>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);
template bool NoFloatPolicy<3>::staticAdjustInputs(TempAllocator &alloc, MInstruction *def);

template <unsigned Op>
bool
BoxPolicy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == MIRType_Value)
        return true;

    ins->replaceOperand(Op, boxAt(alloc, ins, in));
    return true;
}

template bool BoxPolicy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
template bool BoxPolicy<1>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
template bool BoxPolicy<2>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);

template <unsigned Op, MIRType Type>
bool
BoxExceptPolicy<Op, Type>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == Type)
        return true;
    return BoxPolicy<Op>::staticAdjustInputs(alloc, ins);
}

template bool BoxExceptPolicy<0, MIRType_String>::staticAdjustInputs(TempAllocator &alloc,
                                                                     MInstruction *ins);
template bool BoxExceptPolicy<1, MIRType_String>::staticAdjustInputs(TempAllocator &alloc,
                                                                     MInstruction *ins);
template bool BoxExceptPolicy<2, MIRType_String>::staticAdjustInputs(TempAllocator &alloc,
                                                                     MInstruction *ins);

bool
ToDoublePolicy::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MOZ_ASSERT(ins->isToDouble() || ins->isToFloat32());

    MDefinition *in = ins->getOperand(0);
    MToFPInstruction::ConversionKind conversion;
    if (ins->isToDouble())
        conversion = ins->toToDouble()->conversion();
    else
        conversion = ins->toToFloat32()->conversion();

    switch (in->type()) {
      case MIRType_Int32:
      case MIRType_Float32:
      case MIRType_Double:
      case MIRType_Value:
        // No need for boxing for these types.
        return true;
      case MIRType_Null:
        // No need for boxing, when we will convert.
        if (conversion == MToFPInstruction::NonStringPrimitives)
            return true;
        break;
      case MIRType_Undefined:
      case MIRType_Boolean:
        // No need for boxing, when we will convert.
        if (conversion == MToFPInstruction::NonStringPrimitives)
            return true;
        if (conversion == MToFPInstruction::NonNullNonStringPrimitives)
            return true;
        break;
      case MIRType_Object:
      case MIRType_String:
      case MIRType_Symbol:
        // Objects might be effectful. Symbols give TypeError.
        break;
      default:
        break;
    }

    in = boxAt(alloc, ins, in);
    ins->replaceOperand(0, in);
    return true;
}

bool
ToInt32Policy::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MOZ_ASSERT(ins->isToInt32() || ins->isTruncateToInt32());

    MacroAssembler::IntConversionInputKind conversion = MacroAssembler::IntConversion_Any;
    if (ins->isToInt32())
        conversion = ins->toToInt32()->conversion();

    MDefinition *in = ins->getOperand(0);
    switch (in->type()) {
      case MIRType_Int32:
      case MIRType_Float32:
      case MIRType_Double:
      case MIRType_Value:
        // No need for boxing for these types.
        return true;
      case MIRType_Undefined:
        // No need for boxing when truncating.
        if (ins->isTruncateToInt32())
            return true;
        break;
      case MIRType_Null:
        // No need for boxing, when we will convert.
        if (conversion == MacroAssembler::IntConversion_Any)
            return true;
        break;
      case MIRType_Boolean:
        // No need for boxing, when we will convert.
        if (conversion == MacroAssembler::IntConversion_Any)
            return true;
        if (conversion == MacroAssembler::IntConversion_NumbersOrBoolsOnly)
            return true;
        break;
      case MIRType_Object:
      case MIRType_String:
      case MIRType_Symbol:
        // Objects might be effectful. Symbols give TypeError.
        break;
      default:
        break;
    }

    in = boxAt(alloc, ins, in);
    ins->replaceOperand(0, in);
    return true;
}

bool
ToStringPolicy::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MOZ_ASSERT(ins->isToString());

    MIRType type = ins->getOperand(0)->type();
    if (type == MIRType_Object || type == MIRType_Symbol) {
        ins->replaceOperand(0, boxAt(alloc, ins, ins->getOperand(0)));
        return true;
    }

    // TODO remove the following line once 966957 has landed
    EnsureOperandNotFloat32(alloc, ins, 0);

    return true;
}

template <unsigned Op>
bool
ObjectPolicy<Op>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == MIRType_Object || in->type() == MIRType_Slots ||
        in->type() == MIRType_Elements)
    {
        return true;
    }

    MUnbox *replace = MUnbox::New(alloc, in, MIRType_Object, MUnbox::Fallible);
    ins->block()->insertBefore(ins, replace);
    ins->replaceOperand(Op, replace);

    return replace->typePolicy()->adjustInputs(alloc, replace);
}

template bool ObjectPolicy<0>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
template bool ObjectPolicy<1>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
template bool ObjectPolicy<2>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);
template bool ObjectPolicy<3>::staticAdjustInputs(TempAllocator &alloc, MInstruction *ins);

bool
CallPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MCall *call = ins->toCall();

    MDefinition *func = call->getFunction();
    if (func->type() != MIRType_Object) {
        MInstruction *unbox = MUnbox::New(alloc, func, MIRType_Object, MUnbox::Fallible);
        call->block()->insertBefore(call, unbox);
        call->replaceFunction(unbox);

        if (!unbox->typePolicy()->adjustInputs(alloc, unbox))
            return false;
    }

    for (uint32_t i = 0; i < call->numStackArgs(); i++)
        EnsureOperandNotFloat32(alloc, call, MCall::IndexOfStackArg(i));

    return true;
}

bool
CallSetElementPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    // The first operand should be an object.
    SingleObjectPolicy::adjustInputs(alloc, ins);

    // Box the index and value operands.
    for (size_t i = 1, e = ins->numOperands(); i < e; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;
        ins->replaceOperand(i, boxAt(alloc, ins, in));
    }
    return true;
}

bool
InstanceOfPolicy::adjustInputs(TempAllocator &alloc, MInstruction *def)
{
    // Box first operand if it isn't object
    if (def->getOperand(0)->type() != MIRType_Object)
        BoxPolicy<0>::staticAdjustInputs(alloc, def);

    return true;
}

bool
StoreTypedArrayPolicy::adjustValueInput(TempAllocator &alloc, MInstruction *ins, int arrayType,
                                        MDefinition *value, int valueOperand)
{
    MDefinition *curValue = value;
    // First, ensure the value is int32, boolean, double or Value.
    // The conversion is based on TypedArrayObjectTemplate::setElementTail.
    switch (value->type()) {
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Float32:
      case MIRType_Boolean:
      case MIRType_Value:
        break;
      case MIRType_Null:
        value->setImplicitlyUsedUnchecked();
        value = MConstant::New(alloc, Int32Value(0));
        ins->block()->insertBefore(ins, value->toInstruction());
        break;
      case MIRType_Undefined:
        value->setImplicitlyUsedUnchecked();
        value = MConstant::New(alloc, DoubleNaNValue());
        ins->block()->insertBefore(ins, value->toInstruction());
        break;
      case MIRType_Object:
      case MIRType_String:
      case MIRType_Symbol:
        value = boxAt(alloc, ins, value);
        break;
      default:
        MOZ_CRASH("Unexpected type");
    }

    if (value != curValue) {
        ins->replaceOperand(valueOperand, value);
        curValue = value;
    }

    MOZ_ASSERT(value->type() == MIRType_Int32 ||
               value->type() == MIRType_Boolean ||
               value->type() == MIRType_Double ||
               value->type() == MIRType_Float32 ||
               value->type() == MIRType_Value);

    switch (arrayType) {
      case Scalar::Int8:
      case Scalar::Uint8:
      case Scalar::Int16:
      case Scalar::Uint16:
      case Scalar::Int32:
      case Scalar::Uint32:
        if (value->type() != MIRType_Int32) {
            value = MTruncateToInt32::New(alloc, value);
            ins->block()->insertBefore(ins, value->toInstruction());
        }
        break;
      case Scalar::Uint8Clamped:
        // IonBuilder should have inserted ClampToUint8.
        MOZ_ASSERT(value->type() == MIRType_Int32);
        break;
      case Scalar::Float32:
        if (value->type() != MIRType_Float32) {
            value = MToFloat32::New(alloc, value);
            ins->block()->insertBefore(ins, value->toInstruction());
        }
        break;
      case Scalar::Float64:
        if (value->type() != MIRType_Double) {
            value = MToDouble::New(alloc, value);
            ins->block()->insertBefore(ins, value->toInstruction());
        }
        break;
      default:
        MOZ_CRASH("Invalid array type");
    }

    if (value != curValue)
        ins->replaceOperand(valueOperand, value);

    return true;
}

bool
StoreTypedArrayPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MStoreTypedArrayElement *store = ins->toStoreTypedArrayElement();
    MOZ_ASSERT(store->elements()->type() == MIRType_Elements);
    MOZ_ASSERT(store->index()->type() == MIRType_Int32);

    return adjustValueInput(alloc, ins, store->arrayType(), store->value(), 2);
}

bool
StoreTypedArrayHolePolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MStoreTypedArrayElementHole *store = ins->toStoreTypedArrayElementHole();
    MOZ_ASSERT(store->elements()->type() == MIRType_Elements);
    MOZ_ASSERT(store->index()->type() == MIRType_Int32);
    MOZ_ASSERT(store->length()->type() == MIRType_Int32);

    return adjustValueInput(alloc, ins, store->arrayType(), store->value(), 3);
}

bool
StoreTypedArrayElementStaticPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MStoreTypedArrayElementStatic *store = ins->toStoreTypedArrayElementStatic();

    return ConvertToInt32Policy<0>::staticAdjustInputs(alloc, ins) &&
        adjustValueInput(alloc, ins, store->viewType(), store->value(), 1);
}

bool
ClampPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MDefinition *in = ins->toClampToUint8()->input();

    switch (in->type()) {
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Value:
        break;
      default:
          ins->replaceOperand(0, boxAt(alloc, ins, in));
        break;
    }

    return true;
}

bool
FilterTypeSetPolicy::adjustInputs(TempAllocator &alloc, MInstruction *ins)
{
    MOZ_ASSERT(ins->numOperands() == 1);

    // Do nothing if already same type.
    if (ins->type() == ins->getOperand(0)->type())
        return true;

    // Box input if ouput type is MIRType_Value
    if (ins->type() == MIRType_Value) {
        ins->replaceOperand(0, boxAt(alloc, ins, ins->getOperand(0)));
        return true;
    }

    // For simplicity just mark output type as MIRType_Value if input type
    // is MIRType_Value. It should be possible to unbox, but we need to
    // add extra code for Undefined/Null.
    if (ins->getOperand(0)->type() == MIRType_Value) {
        ins->setResultType(MIRType_Value);
        return true;
    }

    // In all other cases we will definitely bail, since types don't
    // correspond. Just box and mark output as MIRType_Value.
    ins->replaceOperand(0, boxAt(alloc, ins, ins->getOperand(0)));
    ins->setResultType(MIRType_Value);

    return true;
}

// Lists of all TypePolicy specializations which are used by MIR Instructions.
#define TYPE_POLICY_LIST(_)                     \
    _(ArithPolicy)                              \
    _(BitwisePolicy)                            \
    _(BoxInputsPolicy)                          \
    _(CallPolicy)                               \
    _(CallSetElementPolicy)                     \
    _(ClampPolicy)                              \
    _(ComparePolicy)                            \
    _(FilterTypeSetPolicy)                      \
    _(InstanceOfPolicy)                         \
    _(PowPolicy)                                \
    _(StoreTypedArrayElementStaticPolicy)       \
    _(StoreTypedArrayHolePolicy)                \
    _(StoreTypedArrayPolicy)                    \
    _(TestPolicy)                               \
    _(ToDoublePolicy)                           \
    _(ToInt32Policy)                            \
    _(ToStringPolicy)                           \
    _(TypeBarrierPolicy)

#define TEMPLATE_TYPE_POLICY_LIST(_)                                    \
    _(BoxExceptPolicy<0, MIRType_String>)                               \
    _(BoxPolicy<0>)                                                     \
    _(ConvertToInt32Policy<0>)                                          \
    _(ConvertToStringPolicy<0>)                                         \
    _(DoublePolicy<0>)                                                  \
    _(FloatingPointPolicy<0>)                                           \
    _(IntPolicy<0>)                                                     \
    _(IntPolicy<1>)                                                     \
    _(Mix3Policy<ObjectPolicy<0>, BoxExceptPolicy<1, MIRType_String>, BoxPolicy<2> >) \
    _(Mix3Policy<ObjectPolicy<0>, BoxPolicy<1>, BoxPolicy<2> >)         \
    _(Mix3Policy<ObjectPolicy<0>, BoxPolicy<1>, ObjectPolicy<2> >)      \
    _(Mix3Policy<ObjectPolicy<0>, IntPolicy<1>, BoxPolicy<2> >)         \
    _(Mix3Policy<ObjectPolicy<0>, IntPolicy<1>, IntPolicy<2> >)         \
    _(Mix3Policy<ObjectPolicy<0>, ObjectPolicy<1>, IntPolicy<2> >)      \
    _(Mix3Policy<StringPolicy<0>, ObjectPolicy<1>, StringPolicy<2> >)   \
    _(Mix3Policy<StringPolicy<0>, StringPolicy<1>, StringPolicy<2> >)   \
    _(MixPolicy<BoxPolicy<0>, ObjectPolicy<1> >)                        \
    _(MixPolicy<ConvertToStringPolicy<0>, ConvertToStringPolicy<1> >)   \
    _(MixPolicy<ConvertToStringPolicy<0>, ObjectPolicy<1> >)            \
    _(MixPolicy<DoublePolicy<0>, DoublePolicy<1> >)                     \
    _(MixPolicy<ObjectPolicy<0>, BoxPolicy<1> >)                        \
    _(MixPolicy<ObjectPolicy<0>, ConvertToStringPolicy<1> >)            \
    _(MixPolicy<ObjectPolicy<0>, IntPolicy<1> >)                        \
    _(MixPolicy<ObjectPolicy<0>, NoFloatPolicy<1> >)                    \
    _(MixPolicy<ObjectPolicy<0>, NoFloatPolicy<2> >)                    \
    _(MixPolicy<ObjectPolicy<0>, NoFloatPolicy<3> >)                    \
    _(MixPolicy<ObjectPolicy<0>, ObjectPolicy<1> >)                     \
    _(MixPolicy<ObjectPolicy<0>, StringPolicy<1> >)                     \
    _(MixPolicy<ObjectPolicy<1>, ConvertToStringPolicy<0> >)            \
    _(MixPolicy<StringPolicy<0>, IntPolicy<1> >)                        \
    _(MixPolicy<StringPolicy<0>, StringPolicy<1> >)                     \
    _(NoFloatPolicy<0>)                                                 \
    _(ObjectPolicy<0>)                                                  \
    _(ObjectPolicy<1>)                                                  \
    _(ObjectPolicy<3>)                                                  \
    _(StringPolicy<0>)


namespace js {
namespace jit {

// Define for all used TypePolicy specialization, the definition for
// |TypePolicy::Data::thisTypePolicy|.  This function returns one constant
// instance of the TypePolicy which is shared among all MIR Instructions of the
// same type.
//
// This Macro use __VA_ARGS__ to account for commas of template parameters.
#define DEFINE_TYPE_POLICY_SINGLETON_INSTANCES_(...)    \
    TypePolicy *                                        \
    __VA_ARGS__::Data::thisTypePolicy()                 \
    {                                                   \
        static __VA_ARGS__ singletonType;               \
        return &singletonType;                          \
    }

    TYPE_POLICY_LIST(DEFINE_TYPE_POLICY_SINGLETON_INSTANCES_)
    TEMPLATE_TYPE_POLICY_LIST(template<> DEFINE_TYPE_POLICY_SINGLETON_INSTANCES_)
#undef DEFINE_TYPE_POLICY_SINGLETON_INSTANCES_

}
}

namespace {

// Default function visited by the C++ lookup rules, if the MIR Instruction does
// not inherit from a TypePolicy::Data type.
static TypePolicy *
thisTypePolicy() {
    return nullptr;
}

static MIRType
thisTypeSpecialization() {
    MOZ_CRASH("TypeSpecialization lacks definition of thisTypeSpecialization.");
}

}

TypePolicy *
MGetElementCache::thisTypePolicy()
{
    if (type() == MIRType_Value)
        return PolicyV.thisTypePolicy();
    return PolicyT.thisTypePolicy();
}

// For each MIR Instruction, this macro define the |typePolicy| method which is
// using the |thisTypePolicy| function.  We use the C++ lookup rules to select
// the right |thisTypePolicy| member.  The |thisTypePolicy| function can either
// be a member of the MIR Instruction, such as done for MGetElementCache, or a
// member inherited from the TypePolicy::Data structure, or at last the global
// with the same name if the instruction has no TypePolicy.
#define DEFINE_MIR_TYPEPOLICY_MEMBERS_(op)      \
    TypePolicy *                                \
    js::jit::M##op::typePolicy()                \
    {                                           \
        return thisTypePolicy();                \
    }                                           \
                                                \
    MIRType                                     \
    js::jit::M##op::typePolicySpecialization()  \
    {                                           \
        return thisTypeSpecialization();        \
    }

    MIR_OPCODE_LIST(DEFINE_MIR_TYPEPOLICY_MEMBERS_)
#undef DEFINE_MIR_TYPEPOLICY_MEMBERS_
