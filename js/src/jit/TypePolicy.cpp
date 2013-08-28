/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TypePolicy.h"
#include "MIR.h"
#include "MIRGraph.h"

using namespace js;
using namespace js::ion;

MDefinition *
BoxInputsPolicy::boxAt(MInstruction *at, MDefinition *operand)
{
    if (operand->isUnbox())
        return operand->toUnbox()->input();
    MBox *box = MBox::New(operand);
    at->block()->insertBefore(at, box);
    return box;
}

bool
BoxInputsPolicy::adjustInputs(MInstruction *ins)
{
    for (size_t i = 0; i < ins->numOperands(); i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;
        ins->replaceOperand(i, boxAt(ins, in));
    }
    return true;
}

bool
ArithPolicy::adjustInputs(MInstruction *ins)
{
    if (specialization_ == MIRType_None)
        return BoxInputsPolicy::adjustInputs(ins);

    JS_ASSERT(ins->type() == MIRType_Double || ins->type() == MIRType_Int32);

    for (size_t i = 0; i < ins->numOperands(); i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == ins->type())
            continue;

        MInstruction *replace;

        // If the input is a string or an object, the conversion is not
        // possible, at least, we can't specialize. So box the input.
        if (in->type() == MIRType_Object || in->type() == MIRType_String ||
            (in->type() == MIRType_Undefined && specialization_ == MIRType_Int32))
        {
            in = boxAt(ins, in);
        }

        if (ins->type() == MIRType_Double)
            replace = MToDouble::New(in);
        else
            replace = MToInt32::New(in);

        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);
    }

    return true;
}

bool
BinaryStringPolicy::adjustInputs(MInstruction *ins)
{
    for (size_t i = 0; i < 2; i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_String)
            continue;

        MInstruction *replace = NULL;
        if (in->type() == MIRType_Int32) {
            replace = MToString::New(in);
        } else {
            if (in->type() != MIRType_Value)
                in = boxAt(ins, in);
            replace = MUnbox::New(in, MIRType_String, MUnbox::Fallible);
        }

        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);
    }

    return true;
}

bool
ComparePolicy::adjustInputs(MInstruction *def)
{
    JS_ASSERT(def->isCompare());
    MCompare *compare = def->toCompare();

    // Box inputs to get value
    if (compare->compareType() == MCompare::Compare_Unknown ||
        compare->compareType() == MCompare::Compare_Value)
    {
        return BoxInputsPolicy::adjustInputs(def);
    }

    // Compare_Boolean specialization is done for "Anything === Bool"
    // If the LHS is boolean, we set the specialization to Compare_Int32.
    // This matches other comparisons of the form bool === bool and
    // generated code of Compare_Int32 is more efficient.
    if (compare->compareType() == MCompare::Compare_Boolean &&
        def->getOperand(0)->type() == MIRType_Boolean)
    {
       compare->setCompareType(MCompare::Compare_Int32);
    }

    // Compare_Boolean specialization is done for "Anything === Bool"
    // As of previous line Anything can't be Boolean
    if (compare->compareType() == MCompare::Compare_Boolean) {
        // Unbox rhs that is definitely Boolean
        MDefinition *rhs = def->getOperand(1);
        if (rhs->type() != MIRType_Boolean) {
            if (rhs->type() != MIRType_Value)
                rhs = boxAt(def, rhs);
            MInstruction *unbox = MUnbox::New(rhs, MIRType_Boolean, MUnbox::Infallible);
            def->block()->insertBefore(def, unbox);
            def->replaceOperand(1, unbox);
        }

        JS_ASSERT(def->getOperand(0)->type() != MIRType_Boolean);
        JS_ASSERT(def->getOperand(1)->type() == MIRType_Boolean);
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
            if (rhs->type() != MIRType_Value)
                rhs = boxAt(def, rhs);
            MInstruction *unbox = MUnbox::New(rhs, MIRType_String, MUnbox::Infallible);
            def->block()->insertBefore(def, unbox);
            def->replaceOperand(1, unbox);
        }

        JS_ASSERT(def->getOperand(0)->type() != MIRType_String);
        JS_ASSERT(def->getOperand(1)->type() == MIRType_String);
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
    JS_ASSERT(type == MIRType_Int32 || type == MIRType_Double ||
              type == MIRType_Object || type == MIRType_String);
    for (size_t i = 0; i < 2; i++) {
        MDefinition *in = def->getOperand(i);
        if (in->type() == type)
            continue;

        MInstruction *replace;

        // See BinaryArithPolicy::adjustInputs for an explanation of the following
        if (in->type() == MIRType_Object || in->type() == MIRType_String ||
            in->type() == MIRType_Undefined)
        {
            in = boxAt(def, in);
        }

        switch (type) {
          case MIRType_Double: {
            MToDouble::ConversionKind convert = MToDouble::NumbersOnly;
            if (compare->compareType() == MCompare::Compare_DoubleMaybeCoerceLHS && i == 0)
                convert = MToDouble::NonNullNonStringPrimitives;
            else if (compare->compareType() == MCompare::Compare_DoubleMaybeCoerceRHS && i == 1)
                convert = MToDouble::NonNullNonStringPrimitives;
            if (convert == MToDouble::NumbersOnly && in->type() == MIRType_Boolean)
                in = boxAt(def, in);
            replace = MToDouble::New(in, convert);
            break;
          }
          case MIRType_Int32:
            replace = MToInt32::New(in);
            break;
          case MIRType_Object:
            replace = MUnbox::New(in, MIRType_Object, MUnbox::Infallible);
            break;
          case MIRType_String:
            replace = MUnbox::New(in, MIRType_String, MUnbox::Infallible);
            break;
          default:
            JS_NOT_REACHED("Unknown compare specialization");
            return false;
        }

        def->block()->insertBefore(def, replace);
        def->replaceOperand(i, replace);
    }

    return true;
}

bool
TestPolicy::adjustInputs(MInstruction *ins)
{
    MDefinition *op = ins->getOperand(0);
    switch (op->type()) {
      case MIRType_Value:
      case MIRType_Null:
      case MIRType_Undefined:
      case MIRType_Boolean:
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Object:
        break;

      case MIRType_String:
      {
        MStringLength *length = MStringLength::New(op);
        ins->block()->insertBefore(ins, length);
        ins->replaceOperand(0, length);
        break;
      }

      default:
        ins->replaceOperand(0, boxAt(ins, op));
        break;
    }
    return true;
}

bool
BitwisePolicy::adjustInputs(MInstruction *ins)
{
    if (specialization_ == MIRType_None)
        return BoxInputsPolicy::adjustInputs(ins);

    JS_ASSERT(ins->type() == specialization_);
    JS_ASSERT(specialization_ == MIRType_Int32 || specialization_ == MIRType_Double);

    // This policy works for both unary and binary bitwise operations.
    for (size_t i = 0; i < ins->numOperands(); i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Int32)
            continue;

        // See BinaryArithPolicy::adjustInputs for an explanation of the following
        if (in->type() == MIRType_Object || in->type() == MIRType_String)
            in = boxAt(ins, in);

        MInstruction *replace = MTruncateToInt32::New(in);
        ins->block()->insertBefore(ins, replace);
        ins->replaceOperand(i, replace);
    }

    return true;
}

bool
PowPolicy::adjustInputs(MInstruction *ins)
{
    JS_ASSERT(specialization_ == MIRType_Int32 || specialization_ == MIRType_Double);

    // Input must be a double.
    if (!DoublePolicy<0>::staticAdjustInputs(ins))
        return false;

    // Power may be an int32 or a double. Integers receive a faster path.
    if (specialization_ == MIRType_Double)
        return DoublePolicy<1>::staticAdjustInputs(ins);
    return IntPolicy<1>::staticAdjustInputs(ins);
}

template <unsigned Op>
bool
StringPolicy<Op>::staticAdjustInputs(MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_String)
        return true;

    MInstruction *replace;
    if (in->type() == MIRType_Int32) {
        replace = MToString::New(in);
    } else {
        if (in->type() != MIRType_Value)
            in = boxAt(def, in);
        replace = MUnbox::New(in, MIRType_String, MUnbox::Fallible);
    }

    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);
    return true;
}

template bool StringPolicy<0>::staticAdjustInputs(MInstruction *ins);
template bool StringPolicy<1>::staticAdjustInputs(MInstruction *ins);

template <unsigned Op>
bool
IntPolicy<Op>::staticAdjustInputs(MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Int32)
        return true;

    MUnbox *replace = MUnbox::New(in, MIRType_Int32, MUnbox::Fallible);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);
    return true;
}

template bool IntPolicy<0>::staticAdjustInputs(MInstruction *def);
template bool IntPolicy<1>::staticAdjustInputs(MInstruction *def);

template <unsigned Op>
bool
DoublePolicy<Op>::staticAdjustInputs(MInstruction *def)
{
    MDefinition *in = def->getOperand(Op);
    if (in->type() == MIRType_Double)
        return true;

    // Force a bailout. Objects may be effectful; strings are currently unhandled.
    if (in->type() == MIRType_Object || in->type() == MIRType_String) {
        MBox *box = MBox::New(in);
        def->block()->insertBefore(def, box);

        MUnbox *unbox = MUnbox::New(box, MIRType_Double, MUnbox::Fallible);
        def->block()->insertBefore(def, unbox);
        def->replaceOperand(Op, unbox);
        return true;
    }

    MToDouble *replace = MToDouble::New(in);
    def->block()->insertBefore(def, replace);
    def->replaceOperand(Op, replace);
    return true;
}

template bool DoublePolicy<0>::staticAdjustInputs(MInstruction *def);
template bool DoublePolicy<1>::staticAdjustInputs(MInstruction *def);

template <unsigned Op>
bool
BoxPolicy<Op>::staticAdjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == MIRType_Value)
        return true;

    ins->replaceOperand(Op, boxAt(ins, in));
    return true;
}

template bool BoxPolicy<0>::staticAdjustInputs(MInstruction *ins);
template bool BoxPolicy<1>::staticAdjustInputs(MInstruction *ins);
template bool BoxPolicy<2>::staticAdjustInputs(MInstruction *ins);

bool
ToDoublePolicy::staticAdjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->getOperand(0);
    if (in->type() != MIRType_Object && in->type() != MIRType_String)
        return true;

    in = boxAt(ins, in);
    ins->replaceOperand(0, in);
    return true;
}

template <unsigned Op>
bool
ObjectPolicy<Op>::staticAdjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->getOperand(Op);
    if (in->type() == MIRType_Object || in->type() == MIRType_Slots ||
        in->type() == MIRType_Elements)
    {
        return true;
    }

    if (in->type() != MIRType_Value)
        in = boxAt(ins, in);

    MUnbox *replace = MUnbox::New(in, MIRType_Object, MUnbox::Fallible);
    ins->block()->insertBefore(ins, replace);
    ins->replaceOperand(Op, replace);
    return true;
}

template bool ObjectPolicy<0>::staticAdjustInputs(MInstruction *ins);
template bool ObjectPolicy<1>::staticAdjustInputs(MInstruction *ins);
template bool ObjectPolicy<2>::staticAdjustInputs(MInstruction *ins);
template bool ObjectPolicy<3>::staticAdjustInputs(MInstruction *ins);

bool
CallPolicy::adjustInputs(MInstruction *ins)
{
    MCall *call = ins->toCall();

    MDefinition *func = call->getFunction();
    if (func->type() == MIRType_Object)
        return true;

    // If the function is impossible to call,
    // bail out by causing a subsequent unbox to fail.
    if (func->type() != MIRType_Value)
        func = boxAt(call, func);

    MInstruction *unbox = MUnbox::New(func, MIRType_Object, MUnbox::Fallible);
    call->block()->insertBefore(call, unbox);
    call->replaceFunction(unbox);

    return true;
}

bool
CallSetElementPolicy::adjustInputs(MInstruction *ins)
{
    // The first operand should be an object.
    SingleObjectPolicy::adjustInputs(ins);

    // Box the index and value operands.
    for (size_t i = 1; i < ins->numOperands(); i++) {
        MDefinition *in = ins->getOperand(i);
        if (in->type() == MIRType_Value)
            continue;
        ins->replaceOperand(i, boxAt(ins, in));
    }
    return true;
}

bool
InstanceOfPolicy::adjustInputs(MInstruction *def)
{
    // Box first operand if it isn't object
    if (def->getOperand(0)->type() != MIRType_Object) {
       BoxPolicy<0>::staticAdjustInputs(def);
    }

    return true;
}

bool
StoreTypedArrayPolicy::adjustValueInput(MInstruction *ins, int arrayType,
                                        MDefinition *value, int valueOperand)
{
    MDefinition *curValue = value;
    // First, ensure the value is int32, boolean, double or Value.
    // The conversion is based on TypedArrayTemplate::setElementTail.
    switch (value->type()) {
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Boolean:
      case MIRType_Value:
        break;
      case MIRType_Null:
        value->setFoldedUnchecked();
        value = MConstant::New(Int32Value(0));
        ins->block()->insertBefore(ins, value->toInstruction());
        break;
      case MIRType_Object:
      case MIRType_Undefined:
        value->setFoldedUnchecked();
        value = MConstant::New(DoubleValue(js_NaN));
        ins->block()->insertBefore(ins, value->toInstruction());
        break;
      case MIRType_String:
        value = boxAt(ins, value);
        break;
      default:
        JS_NOT_REACHED("Unexpected type");
        break;
    }

    if (value != curValue) {
        ins->replaceOperand(valueOperand, value);
        curValue = value;
    }

    JS_ASSERT(value->type() == MIRType_Int32 ||
              value->type() == MIRType_Boolean ||
              value->type() == MIRType_Double ||
              value->type() == MIRType_Value);

    switch (arrayType) {
      case TypedArray::TYPE_INT8:
      case TypedArray::TYPE_UINT8:
      case TypedArray::TYPE_INT16:
      case TypedArray::TYPE_UINT16:
      case TypedArray::TYPE_INT32:
      case TypedArray::TYPE_UINT32:
        if (value->type() != MIRType_Int32) {
            value = MTruncateToInt32::New(value);
            ins->block()->insertBefore(ins, value->toInstruction());
        }
        break;
      case TypedArray::TYPE_UINT8_CLAMPED:
        // IonBuilder should have inserted ClampToUint8.
        JS_ASSERT(value->type() == MIRType_Int32);
        break;
      case TypedArray::TYPE_FLOAT32:
      case TypedArray::TYPE_FLOAT64:
        if (value->type() != MIRType_Double) {
            value = MToDouble::New(value);
            ins->block()->insertBefore(ins, value->toInstruction());
        }
        break;
      default:
        JS_NOT_REACHED("Invalid array type");
        break;
    }

    if (value != curValue) {
        ins->replaceOperand(valueOperand, value);
        curValue = value;
    }
    return true;
}

bool
StoreTypedArrayPolicy::adjustInputs(MInstruction *ins)
{
    MStoreTypedArrayElement *store = ins->toStoreTypedArrayElement();
    JS_ASSERT(store->elements()->type() == MIRType_Elements);
    JS_ASSERT(store->index()->type() == MIRType_Int32);

    return adjustValueInput(ins, store->arrayType(), store->value(), 2);
}

bool
StoreTypedArrayHolePolicy::adjustInputs(MInstruction *ins)
{
    MStoreTypedArrayElementHole *store = ins->toStoreTypedArrayElementHole();
    JS_ASSERT(store->elements()->type() == MIRType_Elements);
    JS_ASSERT(store->index()->type() == MIRType_Int32);
    JS_ASSERT(store->length()->type() == MIRType_Int32);

    return adjustValueInput(ins, store->arrayType(), store->value(), 3);
}

bool
StoreTypedArrayElementStaticPolicy::adjustInputs(MInstruction *ins)
{
    MStoreTypedArrayElementStatic *store = ins->toStoreTypedArrayElementStatic();
    JS_ASSERT(store->ptr()->type() == MIRType_Int32);

    return adjustValueInput(ins, store->viewType(), store->value(), 1);
}

bool
ClampPolicy::adjustInputs(MInstruction *ins)
{
    MDefinition *in = ins->toClampToUint8()->input();

    switch (in->type()) {
      case MIRType_Int32:
      case MIRType_Double:
      case MIRType_Value:
        break;
      default:
        ins->replaceOperand(0, boxAt(ins, in));
        break;
    }

    return true;
}
