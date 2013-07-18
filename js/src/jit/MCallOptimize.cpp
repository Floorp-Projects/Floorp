/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsmath.h"

#include "builtin/ParallelArray.h"
#include "builtin/TestingFunctions.h"
#include "jit/IonBuilder.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"

#include "jsscriptinlines.h"

#include "vm/StringObject-inl.h"

namespace js {
namespace jit {

IonBuilder::InliningStatus
IonBuilder::inlineNativeCall(CallInfo &callInfo, JSNative native)
{
    // Array natives.
    if (native == js_Array)
        return inlineArray(callInfo);
    if (native == js::array_pop)
        return inlineArrayPopShift(callInfo, MArrayPopShift::Pop);
    if (native == js::array_shift)
        return inlineArrayPopShift(callInfo, MArrayPopShift::Shift);
    if (native == js::array_push)
        return inlineArrayPush(callInfo);
    if (native == js::array_concat)
        return inlineArrayConcat(callInfo);

    // Math natives.
    if (native == js_math_abs)
        return inlineMathAbs(callInfo);
    if (native == js_math_floor)
        return inlineMathFloor(callInfo);
    if (native == js_math_round)
        return inlineMathRound(callInfo);
    if (native == js_math_sqrt)
        return inlineMathSqrt(callInfo);
    if (native == math_atan2)
        return inlineMathAtan2(callInfo);
    if (native == js_math_max)
        return inlineMathMinMax(callInfo, true /* max */);
    if (native == js_math_min)
        return inlineMathMinMax(callInfo, false /* max */);
    if (native == js_math_pow)
        return inlineMathPow(callInfo);
    if (native == js_math_random)
        return inlineMathRandom(callInfo);
    if (native == js::math_imul)
        return inlineMathImul(callInfo);
    if (native == js::math_sin)
        return inlineMathFunction(callInfo, MMathFunction::Sin);
    if (native == js::math_cos)
        return inlineMathFunction(callInfo, MMathFunction::Cos);
    if (native == js::math_exp)
        return inlineMathFunction(callInfo, MMathFunction::Exp);
    if (native == js::math_tan)
        return inlineMathFunction(callInfo, MMathFunction::Tan);
    if (native == js::math_log)
        return inlineMathFunction(callInfo, MMathFunction::Log);
    if (native == js::math_atan)
        return inlineMathFunction(callInfo, MMathFunction::ATan);
    if (native == js::math_asin)
        return inlineMathFunction(callInfo, MMathFunction::ASin);
    if (native == js::math_acos)
        return inlineMathFunction(callInfo, MMathFunction::ACos);
    if (native == js::math_log10)
        return inlineMathFunction(callInfo, MMathFunction::Log10);
    if (native == js::math_log2)
        return inlineMathFunction(callInfo, MMathFunction::Log2);
    if (native == js::math_log1p)
        return inlineMathFunction(callInfo, MMathFunction::Log1P);
    if (native == js::math_expm1)
        return inlineMathFunction(callInfo, MMathFunction::ExpM1);
    if (native == js::math_cosh)
        return inlineMathFunction(callInfo, MMathFunction::CosH);
    if (native == js::math_sin)
        return inlineMathFunction(callInfo, MMathFunction::SinH);
    if (native == js::math_tan)
        return inlineMathFunction(callInfo, MMathFunction::TanH);
    if (native == js::math_acosh)
        return inlineMathFunction(callInfo, MMathFunction::ACosH);
    if (native == js::math_asin)
        return inlineMathFunction(callInfo, MMathFunction::ASinH);
    if (native == js::math_atan)
        return inlineMathFunction(callInfo, MMathFunction::ATanH);
    if (native == js::math_sign)
        return inlineMathFunction(callInfo, MMathFunction::Sign);
    if (native == js::math_trunc)
        return inlineMathFunction(callInfo, MMathFunction::Trunc);
    if (native == js::math_cbrt)
        return inlineMathFunction(callInfo, MMathFunction::Cbrt);

    // String natives.
    if (native == js_String)
        return inlineStringObject(callInfo);
    if (native == js_str_charCodeAt)
        return inlineStrCharCodeAt(callInfo);
    if (native == js::str_fromCharCode)
        return inlineStrFromCharCode(callInfo);
    if (native == js_str_charAt)
        return inlineStrCharAt(callInfo);

    // RegExp natives.
    if (native == regexp_exec && !CallResultEscapes(pc))
        return inlineRegExpTest(callInfo);
    if (native == regexp_test)
        return inlineRegExpTest(callInfo);

    // Array intrinsics.
    if (native == intrinsic_UnsafePutElements)
        return inlineUnsafePutElements(callInfo);
    if (native == intrinsic_NewDenseArray)
        return inlineNewDenseArray(callInfo);

    // Slot intrinsics.
    if (native == intrinsic_UnsafeSetReservedSlot)
        return inlineUnsafeSetReservedSlot(callInfo);
    if (native == intrinsic_UnsafeGetReservedSlot)
        return inlineUnsafeGetReservedSlot(callInfo);

    // Parallel intrinsics.
    if (native == intrinsic_ShouldForceSequential)
        return inlineForceSequentialOrInParallelSection(callInfo);
    if (native == intrinsic_NewParallelArray)
        return inlineNewParallelArray(callInfo);
    if (native == ParallelArrayObject::construct)
        return inlineParallelArray(callInfo);

    // Utility intrinsics.
    if (native == intrinsic_IsCallable)
        return inlineIsCallable(callInfo);
    if (native == intrinsic_NewObjectWithClassPrototype)
        return inlineNewObjectWithClassPrototype(callInfo);
    if (native == intrinsic_HaveSameClass)
        return inlineHaveSameClass(callInfo);
    if (native == intrinsic_ToObject)
        return inlineToObject(callInfo);

    // Testing Functions
    if (native == testingFunc_inParallelSection)
        return inlineForceSequentialOrInParallelSection(callInfo);
    if (native == testingFunc_bailout)
        return inlineBailout(callInfo);

    return InliningStatus_NotInlined;
}

types::StackTypeSet *
IonBuilder::getInlineReturnTypeSet()
{
    return types::TypeScript::BytecodeTypes(script(), pc);
}

MIRType
IonBuilder::getInlineReturnType()
{
    types::StackTypeSet *returnTypes = getInlineReturnTypeSet();
    return MIRTypeFromValueType(returnTypes->getKnownTypeTag());
}

IonBuilder::InliningStatus
IonBuilder::inlineMathFunction(CallInfo &callInfo, MMathFunction::Function function)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;
    if (!IsNumberType(callInfo.getArg(0)->type()))
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MathCache *cache = cx->runtime()->getMathCache(cx);
    if (!cache)
        return InliningStatus_Error;

    MMathFunction *ins = MMathFunction::New(callInfo.getArg(0), function, cache);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArray(CallInfo &callInfo)
{
    uint32_t initLength = 0;
    MNewArray::AllocatingBehaviour allocating = MNewArray::NewArray_Unallocating;

    // Multiple arguments imply array initialization, not just construction.
    if (callInfo.argc() >= 2) {
        initLength = callInfo.argc();
        allocating = MNewArray::NewArray_Allocating;

        types::TypeObject *type = types::TypeScript::InitObject(cx, script(), pc, JSProto_Array);
        if (!type)
            return InliningStatus_Error;
        if (!type->unknownProperties()) {
            types::HeapTypeSet *elemTypes = type->getProperty(cx, JSID_VOID, false);
            if (!elemTypes)
                return InliningStatus_Error;

            for (uint32_t i = 0; i < initLength; i++) {
                MDefinition *value = callInfo.getArg(i);
                if (!TypeSetIncludes(elemTypes, value->type(), value->resultTypeSet())) {
                    elemTypes->addFreeze(cx);
                    return InliningStatus_NotInlined;
                }
            }
        }
    }

    // A single integer argument denotes initial length.
    if (callInfo.argc() == 1) {
        if (callInfo.getArg(0)->type() != MIRType_Int32)
            return InliningStatus_NotInlined;
        MDefinition *arg = callInfo.getArg(0)->toPassArg()->getArgument();
        if (!arg->isConstant())
            return InliningStatus_NotInlined;

        // Negative lengths generate a RangeError, unhandled by the inline path.
        initLength = arg->toConstant()->value().toInt32();
        if (initLength >= JSObject::NELEMENTS_LIMIT)
            return InliningStatus_NotInlined;
    }

    callInfo.unwrapArgs();

    JSObject *templateObject = getNewArrayTemplateObject(initLength);
    if (!templateObject)
        return InliningStatus_Error;

    types::StackTypeSet::DoubleConversion conversion =
        getInlineReturnTypeSet()->convertDoubleElements(cx);
    if (conversion == types::StackTypeSet::AlwaysConvertToDoubles)
        templateObject->setShouldConvertDoubleElements();

    MNewArray *ins = new MNewArray(initLength, templateObject, allocating);
    current->add(ins);
    current->push(ins);

    if (callInfo.argc() >= 2) {
        // Get the elements vector.
        MElements *elements = MElements::New(ins);
        current->add(elements);

        // Store all values, no need to initialize the length after each as
        // jsop_initelem_array is doing because we do not expect to bailout
        // because the memory is supposed to be allocated by now. There is no
        // need for a post barrier on these writes, as as the MNewAray will use
        // the nursery if possible, triggering a minor collection if it can't.
        MConstant *id = NULL;
        for (uint32_t i = 0; i < initLength; i++) {
            id = MConstant::New(Int32Value(i));
            current->add(id);

            MDefinition *value = callInfo.getArg(i);
            if (conversion == types::StackTypeSet::AlwaysConvertToDoubles) {
                MInstruction *valueDouble = MToDouble::New(value);
                current->add(valueDouble);
                value = valueDouble;
            }

            MStoreElement *store = MStoreElement::New(elements, id, value,
                                                      /* needsHoleCheck = */ false);
            current->add(store);
        }

        // Update the length.
        MSetInitializedLength *length = MSetInitializedLength::New(elements, id);
        current->add(length);

        if (!resumeAfter(length))
            return InliningStatus_Error;
    }

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArrayPopShift(CallInfo &callInfo, MArrayPopShift::Mode mode)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    if (returnType == MIRType_Undefined || returnType == MIRType_Null)
        return InliningStatus_NotInlined;
    if (callInfo.thisArg()->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    // Pop and shift are only handled for dense arrays that have never been
    // used in an iterator: popping elements does not account for suppressing
    // deleted properties in active iterators.
    types::TypeObjectFlags unhandledFlags =
        types::OBJECT_FLAG_SPARSE_INDEXES |
        types::OBJECT_FLAG_LENGTH_OVERFLOW |
        types::OBJECT_FLAG_ITERATED;

    types::StackTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    if (!thisTypes || thisTypes->getKnownClass() != &ArrayObject::class_)
        return InliningStatus_NotInlined;
    if (thisTypes->hasObjectFlags(cx, unhandledFlags))
        return InliningStatus_NotInlined;
    RootedScript script(cx, script_);
    if (types::ArrayPrototypeHasIndexedProperty(cx, script))
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    types::StackTypeSet *returnTypes = getInlineReturnTypeSet();
    bool needsHoleCheck = thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED);
    bool maybeUndefined = returnTypes->hasType(types::Type::UndefinedType());

    bool barrier;
    if (!PropertyReadNeedsTypeBarrier(cx, callInfo.thisArg(), NULL, returnTypes, &barrier))
        return InliningStatus_Error;

    if (barrier)
        returnType = MIRType_Value;

    MArrayPopShift *ins = MArrayPopShift::New(callInfo.thisArg(), mode,
                                              needsHoleCheck, maybeUndefined);
    current->add(ins);
    current->push(ins);
    ins->setResultType(returnType);

    if (!resumeAfter(ins))
        return InliningStatus_Error;

    if (!pushTypeBarrier(ins, returnTypes, barrier))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArrayPush(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    MDefinition *obj = callInfo.thisArg();
    MDefinition *value = callInfo.getArg(0);
    bool writeNeedsBarrier;
    if (!PropertyWriteNeedsTypeBarrier(cx, current, &obj, NULL, &value, /* canModify = */ false,
                                       &writeNeedsBarrier))
    {
        return InliningStatus_Error;
    }
    if (writeNeedsBarrier)
        return InliningStatus_NotInlined;
    JS_ASSERT(obj == callInfo.thisArg() && value == callInfo.getArg(0));

    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;
    if (callInfo.thisArg()->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    types::StackTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    if (!thisTypes || thisTypes->getKnownClass() != &ArrayObject::class_)
        return InliningStatus_NotInlined;
    if (thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPARSE_INDEXES |
                                  types::OBJECT_FLAG_LENGTH_OVERFLOW))
    {
        return InliningStatus_NotInlined;
    }
    RootedScript script(cx, script_);
    if (types::ArrayPrototypeHasIndexedProperty(cx, script))
        return InliningStatus_NotInlined;

    types::StackTypeSet::DoubleConversion conversion = thisTypes->convertDoubleElements(cx);
    if (conversion == types::StackTypeSet::AmbiguousDoubleConversion)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();
    value = callInfo.getArg(0);

    if (conversion == types::StackTypeSet::AlwaysConvertToDoubles ||
        conversion == types::StackTypeSet::MaybeConvertToDoubles)
    {
        MInstruction *valueDouble = MToDouble::New(value);
        current->add(valueDouble);
        value = valueDouble;
    }

    if (NeedsPostBarrier(info(), value))
        current->add(MPostWriteBarrier::New(callInfo.thisArg(), value));

    MArrayPush *ins = MArrayPush::New(callInfo.thisArg(), value);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArrayConcat(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    // Ensure |this|, argument and result are objects.
    if (getInlineReturnType() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (callInfo.thisArg()->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    // |this| and the argument must be dense arrays.
    types::StackTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    types::StackTypeSet *argTypes = callInfo.getArg(0)->resultTypeSet();
    if (!thisTypes || !argTypes)
        return InliningStatus_NotInlined;

    if (thisTypes->getKnownClass() != &ArrayObject::class_)
        return InliningStatus_NotInlined;
    if (thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPARSE_INDEXES |
                                  types::OBJECT_FLAG_LENGTH_OVERFLOW))
    {
        return InliningStatus_NotInlined;
    }

    if (argTypes->getKnownClass() != &ArrayObject::class_)
        return InliningStatus_NotInlined;
    if (argTypes->hasObjectFlags(cx, types::OBJECT_FLAG_SPARSE_INDEXES |
                                 types::OBJECT_FLAG_LENGTH_OVERFLOW))
    {
        return InliningStatus_NotInlined;
    }

    // Watch out for indexed properties on the prototype.
    RootedScript script(cx, script_);
    if (types::ArrayPrototypeHasIndexedProperty(cx, script))
        return InliningStatus_NotInlined;

    // Require the 'this' types to have a specific type matching the current
    // global, so we can create the result object inline.
    if (thisTypes->getObjectCount() != 1)
        return InliningStatus_NotInlined;

    types::TypeObject *thisType = thisTypes->getTypeObject(0);
    if (!thisType ||
        thisType->unknownProperties() ||
        &thisType->proto->global() != &script->global())
    {
        return InliningStatus_NotInlined;
    }

    // Don't inline if 'this' is packed and the argument may not be packed
    // (the result array will reuse the 'this' type).
    if (!thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED) &&
        argTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED))
    {
        return InliningStatus_NotInlined;
    }

    // Constraints modeling this concat have not been generated by inference,
    // so check that type information already reflects possible side effects of
    // this call.
    types::HeapTypeSet *thisElemTypes = thisType->getProperty(cx, JSID_VOID, false);
    if (!thisElemTypes)
        return InliningStatus_Error;

    types::StackTypeSet *resTypes = getInlineReturnTypeSet();
    if (!resTypes->hasType(types::Type::ObjectType(thisType)))
        return InliningStatus_NotInlined;

    for (unsigned i = 0; i < argTypes->getObjectCount(); i++) {
        if (argTypes->getSingleObject(i))
            return InliningStatus_NotInlined;

        types::TypeObject *argType = argTypes->getTypeObject(i);
        if (!argType)
            continue;

        if (argType->unknownProperties())
            return InliningStatus_NotInlined;

        types::HeapTypeSet *elemTypes = argType->getProperty(cx, JSID_VOID, false);
        if (!elemTypes)
            return InliningStatus_Error;

        if (!elemTypes->knownSubset(cx, thisElemTypes))
            return InliningStatus_NotInlined;
    }

    // Inline the call.
    RootedObject templateObj(cx, NewDenseEmptyArray(cx, thisType->proto, TenuredObject));
    if (!templateObj)
        return InliningStatus_Error;
    templateObj->setType(thisType);

    callInfo.unwrapArgs();

    MArrayConcat *ins = MArrayConcat::New(callInfo.thisArg(), callInfo.getArg(0), templateObj);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathAbs(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    MIRType argType = callInfo.getArg(0)->type();
    if (argType != MIRType_Int32 && argType != MIRType_Double)
        return InliningStatus_NotInlined;

    if (argType != returnType && returnType != MIRType_Int32)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MInstruction *ins = MAbs::New(callInfo.getArg(0), argType);
    current->add(ins);

    if (argType != returnType) {
        MToInt32 *toInt = MToInt32::New(ins);
        toInt->setCanBeNegativeZero(false);
        current->add(toInt);
        ins = toInt;
    }

    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathFloor(CallInfo &callInfo)
{

    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    MIRType argType = callInfo.getArg(0)->type();
    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;

    // Math.floor(int(x)) == int(x)
    if (argType == MIRType_Int32) {
        callInfo.unwrapArgs();
        current->push(callInfo.getArg(0));
        return InliningStatus_Inlined;
    }

    if (argType == MIRType_Double) {
        callInfo.unwrapArgs();
        MFloor *ins = new MFloor(callInfo.getArg(0));
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathRound(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    MIRType argType = callInfo.getArg(0)->type();

    // Math.round(int(x)) == int(x)
    if (argType == MIRType_Int32 && returnType == MIRType_Int32) {
        callInfo.unwrapArgs();
        current->push(callInfo.getArg(0));
        return InliningStatus_Inlined;
    }

    if (argType == MIRType_Double && returnType == MIRType_Int32) {
        callInfo.unwrapArgs();
        MRound *ins = new MRound(callInfo.getArg(0));
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathSqrt(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    MIRType argType = callInfo.getArg(0)->type();
    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;
    if (argType != MIRType_Double && argType != MIRType_Int32)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MSqrt *sqrt = MSqrt::New(callInfo.getArg(0));
    current->add(sqrt);
    current->push(sqrt);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathAtan2(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 2)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;

    MIRType argType0 = callInfo.getArg(0)->type();
    MIRType argType1 = callInfo.getArg(1)->type();

    if (!IsNumberType(argType0) || !IsNumberType(argType1))
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MAtan2 *atan2 = MAtan2::New(callInfo.getArg(0), callInfo.getArg(1));
    current->add(atan2);
    current->push(atan2);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathPow(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 2)
        return InliningStatus_NotInlined;

    // Typechecking.
    MIRType baseType = callInfo.getArg(0)->type();
    MIRType powerType = callInfo.getArg(1)->type();
    MIRType outputType = getInlineReturnType();

    if (outputType != MIRType_Int32 && outputType != MIRType_Double)
        return InliningStatus_NotInlined;
    if (baseType != MIRType_Int32 && baseType != MIRType_Double)
        return InliningStatus_NotInlined;
    if (powerType != MIRType_Int32 && powerType != MIRType_Double)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MDefinition *base = callInfo.getArg(0);
    MDefinition *power = callInfo.getArg(1);
    MDefinition *output = NULL;

    // Optimize some constant powers.
    if (callInfo.getArg(1)->isConstant()) {
        double pow;
        RootedValue v(GetIonContext()->cx, callInfo.getArg(1)->toConstant()->value());
        if (!ToNumber(GetIonContext()->cx, v, &pow))
            return InliningStatus_Error;

        // Math.pow(x, 0.5) is a sqrt with edge-case detection.
        if (pow == 0.5) {
            MPowHalf *half = MPowHalf::New(base);
            current->add(half);
            output = half;
        }

        // Math.pow(x, -0.5) == 1 / Math.pow(x, 0.5), even for edge cases.
        if (pow == -0.5) {
            MPowHalf *half = MPowHalf::New(base);
            current->add(half);
            MConstant *one = MConstant::New(DoubleValue(1.0));
            current->add(one);
            MDiv *div = MDiv::New(one, half, MIRType_Double);
            current->add(div);
            output = div;
        }

        // Math.pow(x, 1) == x.
        if (pow == 1.0)
            output = base;

        // Math.pow(x, 2) == x*x.
        if (pow == 2.0) {
            MMul *mul = MMul::New(base, base, outputType);
            current->add(mul);
            output = mul;
        }

        // Math.pow(x, 3) == x*x*x.
        if (pow == 3.0) {
            MMul *mul1 = MMul::New(base, base, outputType);
            current->add(mul1);
            MMul *mul2 = MMul::New(base, mul1, outputType);
            current->add(mul2);
            output = mul2;
        }

        // Math.pow(x, 4) == y*y, where y = x*x.
        if (pow == 4.0) {
            MMul *y = MMul::New(base, base, outputType);
            current->add(y);
            MMul *mul = MMul::New(y, y, outputType);
            current->add(mul);
            output = mul;
        }
    }

    // Use MPow for other powers
    if (!output) {
        MPow *pow = MPow::New(base, power, powerType);
        current->add(pow);
        output = pow;
    }

    // Cast to the right type
    if (outputType == MIRType_Int32 && output->type() != MIRType_Int32) {
        MToInt32 *toInt = MToInt32::New(output);
        current->add(toInt);
        output = toInt;
    }
    if (outputType == MIRType_Double && output->type() != MIRType_Double) {
        MToDouble *toDouble = MToDouble::New(output);
        current->add(toDouble);
        output = toDouble;
    }

    current->push(output);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathRandom(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MRandom *rand = MRandom::New();
    current->add(rand);
    current->push(rand);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathImul(CallInfo &callInfo)
{
    if (callInfo.argc() != 2 || callInfo.constructing())
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    if (returnType != MIRType_Int32)
        return InliningStatus_NotInlined;

    if (!IsNumberType(callInfo.getArg(0)->type()) || callInfo.getArg(0)->type() == MIRType_Float32)
        return InliningStatus_NotInlined;
    if (!IsNumberType(callInfo.getArg(1)->type()) || callInfo.getArg(1)->type() == MIRType_Float32)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MInstruction *first = MTruncateToInt32::New(callInfo.getArg(0));
    current->add(first);

    MInstruction *second = MTruncateToInt32::New(callInfo.getArg(1));
    current->add(second);

    MMul *ins = MMul::New(first, second, MIRType_Int32, MMul::Integer);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathMinMax(CallInfo &callInfo, bool max)
{
    if (callInfo.argc() != 2 || callInfo.constructing())
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    if (!IsNumberType(returnType))
        return InliningStatus_NotInlined;

    MIRType arg0Type = callInfo.getArg(0)->type();
    if (!IsNumberType(arg0Type))
        return InliningStatus_NotInlined;
    MIRType arg1Type = callInfo.getArg(1)->type();
    if (!IsNumberType(arg1Type))
        return InliningStatus_NotInlined;

    if (returnType == MIRType_Int32 &&
        (arg0Type == MIRType_Double || arg1Type == MIRType_Double))
    {
        // We would need to inform TI, if we happen to return a double.
        return InliningStatus_NotInlined;
    }

    callInfo.unwrapArgs();

    MMinMax *ins = MMinMax::New(callInfo.getArg(0), callInfo.getArg(1), returnType, max);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStringObject(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || !callInfo.constructing())
        return InliningStatus_NotInlined;

    // MToString only supports int32 or string values.
    MIRType type = callInfo.getArg(0)->type();
    if (type != MIRType_Int32 && type != MIRType_String)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    RootedString emptyString(cx, cx->runtime()->emptyString);
    RootedObject templateObj(cx, StringObject::create(cx, emptyString, TenuredObject));
    if (!templateObj)
        return InliningStatus_Error;

    MNewStringObject *ins = MNewStringObject::New(callInfo.getArg(0), templateObj);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrCharCodeAt(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;
    if (callInfo.thisArg()->type() != MIRType_String && callInfo.thisArg()->type() != MIRType_Value)
        return InliningStatus_NotInlined;
    MIRType argType = callInfo.getArg(0)->type();
    if (argType != MIRType_Int32 && argType != MIRType_Double)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MInstruction *index = MToInt32::New(callInfo.getArg(0));
    current->add(index);

    MStringLength *length = MStringLength::New(callInfo.thisArg());
    current->add(length);

    index = addBoundsCheck(index, length);

    MCharCodeAt *charCode = MCharCodeAt::New(callInfo.thisArg(), index);
    current->add(charCode);
    current->push(charCode);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrFromCharCode(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_String)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Int32)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MToInt32 *charCode = MToInt32::New(callInfo.getArg(0));
    current->add(charCode);

    MFromCharCode *string = MFromCharCode::New(charCode);
    current->add(string);
    current->push(string);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrCharAt(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_String)
        return InliningStatus_NotInlined;
    if (callInfo.thisArg()->type() != MIRType_String)
        return InliningStatus_NotInlined;
    MIRType argType = callInfo.getArg(0)->type();
    if (argType != MIRType_Int32 && argType != MIRType_Double)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MInstruction *index = MToInt32::New(callInfo.getArg(0));
    current->add(index);

    MStringLength *length = MStringLength::New(callInfo.thisArg());
    current->add(length);

    index = addBoundsCheck(index, length);

    // String.charAt(x) = String.fromCharCode(String.charCodeAt(x))
    MCharCodeAt *charCode = MCharCodeAt::New(callInfo.thisArg(), index);
    current->add(charCode);

    MFromCharCode *string = MFromCharCode::New(charCode);
    current->add(string);
    current->push(string);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineRegExpTest(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    // TI can infer a NULL return type of regexp_test with eager compilation.
    if (CallResultEscapes(pc) && getInlineReturnType() != MIRType_Boolean)
        return InliningStatus_NotInlined;

    if (callInfo.thisArg()->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    types::StackTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    Class *clasp = thisTypes ? thisTypes->getKnownClass() : NULL;
    if (clasp != &RegExpObject::class_)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_String)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();

    MInstruction *match = MRegExpTest::New(callInfo.thisArg(), callInfo.getArg(0));
    current->add(match);
    current->push(match);
    if (!resumeAfter(match))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineUnsafePutElements(CallInfo &callInfo)
{
    uint32_t argc = callInfo.argc();
    if (argc < 3 || (argc % 3) != 0 || callInfo.constructing())
        return InliningStatus_NotInlined;

    /* Important:
     *
     * Here we inline each of the stores resulting from a call to
     * UnsafePutElements().  It is essential that these stores occur
     * atomically and cannot be interrupted by a stack or recursion
     * check.  If this is not true, race conditions can occur.
     */

    for (uint32_t base = 0; base < argc; base += 3) {
        uint32_t arri = base + 0;
        uint32_t idxi = base + 1;
        uint32_t elemi = base + 2;

        MDefinition *obj = callInfo.getArg(arri);
        MDefinition *id = callInfo.getArg(idxi);
        MDefinition *elem = callInfo.getArg(elemi);

        bool isDenseNative = ElementAccessIsDenseNative(obj, id);

        bool writeNeedsBarrier = false;
        if (isDenseNative) {
            if (!PropertyWriteNeedsTypeBarrier(cx, current, &obj, NULL, &elem,
                                               /* canModify = */ false,
                                               &writeNeedsBarrier))
            {
                return InliningStatus_Error;
            }
        }

        // We can only inline setelem on dense arrays that do not need type
        // barriers and on typed arrays.
        ScalarTypeRepresentation::Type arrayType;
        if ((!isDenseNative || writeNeedsBarrier) &&
            !ElementAccessIsTypedArray(obj, id, &arrayType))
        {
            return InliningStatus_NotInlined;
        }
    }

    callInfo.unwrapArgs();

    // Push the result first so that the stack depth matches up for
    // the potential bailouts that will occur in the stores below.
    MConstant *udef = MConstant::New(UndefinedValue());
    current->add(udef);
    current->push(udef);

    for (uint32_t base = 0; base < argc; base += 3) {
        uint32_t arri = base + 0;
        uint32_t idxi = base + 1;

        MDefinition *obj = callInfo.getArg(arri);
        MDefinition *id = callInfo.getArg(idxi);

        if (ElementAccessIsDenseNative(obj, id)) {
            if (!inlineUnsafeSetDenseArrayElement(callInfo, base))
                return InliningStatus_Error;
            continue;
        }

        ScalarTypeRepresentation::Type arrayType;
        if (ElementAccessIsTypedArray(obj, id, &arrayType)) {
            if (!inlineUnsafeSetTypedArrayElement(callInfo, base, arrayType))
                return InliningStatus_Error;
            continue;
        }

        MOZ_ASSUME_UNREACHABLE("Element access not dense array nor typed array");
    }

    return InliningStatus_Inlined;
}

bool
IonBuilder::inlineUnsafeSetDenseArrayElement(CallInfo &callInfo, uint32_t base)
{
    // Note: we do not check the conditions that are asserted as true
    // in intrinsic_UnsafePutElements():
    // - arr is a dense array
    // - idx < initialized length
    // Furthermore, note that inlineUnsafePutElements ensures the type of the
    // value is reflected in the JSID_VOID property of the array.

    MDefinition *obj = callInfo.getArg(base + 0);
    MDefinition *id = callInfo.getArg(base + 1);
    MDefinition *elem = callInfo.getArg(base + 2);

    types::StackTypeSet::DoubleConversion conversion =
        obj->resultTypeSet()->convertDoubleElements(cx);
    if (!jsop_setelem_dense(conversion, SetElem_Unsafe, obj, id, elem))
        return false;
    return true;
}

bool
IonBuilder::inlineUnsafeSetTypedArrayElement(CallInfo &callInfo,
                                             uint32_t base,
                                             ScalarTypeRepresentation::Type arrayType)
{
    // Note: we do not check the conditions that are asserted as true
    // in intrinsic_UnsafePutElements():
    // - arr is a typed array
    // - idx < length

    MDefinition *obj = callInfo.getArg(base + 0);
    MDefinition *id = callInfo.getArg(base + 1);
    MDefinition *elem = callInfo.getArg(base + 2);

    if (!jsop_setelem_typed(arrayType, SetElem_Unsafe, obj, id, elem))
        return false;

    return true;
}

IonBuilder::InliningStatus
IonBuilder::inlineForceSequentialOrInParallelSection(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    ExecutionMode executionMode = info().executionMode();
    switch (executionMode) {
      case SequentialExecution:
        // In sequential mode, leave as is, because we'd have to
        // access the "in warmup" flag of the runtime.
        return InliningStatus_NotInlined;

      case ParallelExecution: {
        // During Parallel Exec, we always force sequential, so
        // replace with true.  This permits UCE to eliminate the
        // entire path as dead, which is important.
        callInfo.unwrapArgs();
        MConstant *ins = MConstant::New(BooleanValue(true));
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
      }

      default:;
    }

    MOZ_ASSUME_UNREACHABLE("Invalid execution mode");
}

IonBuilder::InliningStatus
IonBuilder::inlineNewParallelArray(CallInfo &callInfo)
{
    // Rewrites a call like
    //
    //    NewParallelArray(ParallelArrayView, arg0, ..., argN)
    //
    // to
    //
    //    x = MNewParallelArray()
    //    ParallelArrayView(x, arg0, ..., argN)

    uint32_t argc = callInfo.argc();
    if (argc < 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    types::StackTypeSet *ctorTypes = callInfo.getArg(0)->resultTypeSet();
    JSObject *targetObj = ctorTypes ? ctorTypes->getSingleton() : NULL;
    RootedFunction target(cx);
    if (targetObj && targetObj->is<JSFunction>())
        target = &targetObj->as<JSFunction>();
    if (target && target->isInterpreted() && target->nonLazyScript()->shouldCloneAtCallsite) {
        RootedScript scriptRoot(cx, script());
        target = CloneFunctionAtCallsite(cx, target, scriptRoot, pc);
        if (!target)
            return InliningStatus_Error;
    }
    MDefinition *ctor = makeCallsiteClone(
        target,
        callInfo.getArg(0)->toPassArg()->getArgument());

    // Discard the function.
    return inlineParallelArrayTail(callInfo, target, ctor,
                                   target ? NULL : ctorTypes, 1);
}

IonBuilder::InliningStatus
IonBuilder::inlineParallelArray(CallInfo &callInfo)
{
    if (!callInfo.constructing())
        return InliningStatus_NotInlined;

    uint32_t argc = callInfo.argc();
    RootedFunction target(cx, ParallelArrayObject::getConstructor(cx, argc));
    if (!target)
        return InliningStatus_Error;

    JS_ASSERT(target->nonLazyScript()->shouldCloneAtCallsite);
    RootedScript script(cx, script_);
    target = CloneFunctionAtCallsite(cx, target, script, pc);
    if (!target)
        return InliningStatus_Error;

    MConstant *ctor = MConstant::New(ObjectValue(*target));
    current->add(ctor);

    return inlineParallelArrayTail(callInfo, target, ctor, NULL, 0);
}

IonBuilder::InliningStatus
IonBuilder::inlineParallelArrayTail(CallInfo &callInfo,
                                    HandleFunction target,
                                    MDefinition *ctor,
                                    types::StackTypeSet *ctorTypes,
                                    uint32_t discards)
{
    // Rewrites either NewParallelArray(...) or new ParallelArray(...) from a
    // call to a native ctor into a call to the relevant function in the
    // self-hosted code.

    uint32_t argc = callInfo.argc() - discards;

    // Create the new parallel array object.  Parallel arrays have specially
    // constructed type objects, so we can only perform the inlining if we
    // already have one of these type objects.
    types::StackTypeSet *returnTypes = getInlineReturnTypeSet();
    if (returnTypes->getKnownTypeTag() != JSVAL_TYPE_OBJECT)
        return InliningStatus_NotInlined;
    if (returnTypes->unknownObject() || returnTypes->getObjectCount() != 1)
        return InliningStatus_NotInlined;
    types::TypeObject *typeObject = returnTypes->getTypeObject(0);
    if (!typeObject || typeObject->clasp != &ParallelArrayObject::class_)
        return InliningStatus_NotInlined;

    // Create the call and add in the non-this arguments.
    uint32_t targetArgs = argc;
    if (target && !target->isNative())
        targetArgs = Max<uint32_t>(target->nargs, argc);

    MCall *call = MCall::New(target, targetArgs + 1, argc, false);
    if (!call)
        return InliningStatus_Error;

    // Save the script for inspection by visitCallKnown().
    if (target && target->isInterpreted()) {
        if (!target->getOrCreateScript(cx))
            return InliningStatus_Error;
        call->rootTargetScript(target);
    }

    callInfo.unwrapArgs();

    // Explicitly pad any missing arguments with |undefined|.
    // This permits skipping the argumentsRectifier.
    for (uint32_t i = targetArgs; i > argc; i--) {
        JS_ASSERT_IF(target, !target->isNative());
        MConstant *undef = MConstant::New(UndefinedValue());
        current->add(undef);
        MPassArg *pass = MPassArg::New(undef);
        current->add(pass);
        call->addArg(i, pass);
    }

    MPassArg *oldThis = MPassArg::New(callInfo.thisArg());
    current->add(oldThis);

    // Add explicit arguments.
    // Skip addArg(0) because it is reserved for this
    for (uint32_t i = 0; i < argc; i++) {
        MDefinition *arg = callInfo.getArg(i + discards);
        MPassArg *passArg = MPassArg::New(arg);
        current->add(passArg);
        call->addArg(i + 1, passArg);
    }

    // Place an MPrepareCall before the first passed argument, before we
    // potentially perform rearrangement.
    MPrepareCall *start = new MPrepareCall;
    oldThis->block()->insertBefore(oldThis, start);
    call->initPrepareCall(start);

    // Create the MIR to allocate the new parallel array.  Take the type
    // object is taken from the prediction set.
    RootedObject templateObject(cx, ParallelArrayObject::newInstance(cx, TenuredObject));
    if (!templateObject)
        return InliningStatus_Error;
    templateObject->setType(typeObject);
    MNewParallelArray *newObject = MNewParallelArray::New(templateObject);
    current->add(newObject);
    MPassArg *newThis = MPassArg::New(newObject);
    current->add(newThis);
    call->addArg(0, newThis);

    // Set the new callee.
    call->initFunction(ctor);

    current->add(call);
    current->push(newObject);

    if (!resumeAfter(call))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineNewDenseArray(CallInfo &callInfo)
{
    if (callInfo.constructing() || callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    // For now, in seq. mode we just call the C function.  In
    // par. mode we use inlined MIR.
    ExecutionMode executionMode = info().executionMode();
    switch (executionMode) {
      case SequentialExecution:
        return inlineNewDenseArrayForSequentialExecution(callInfo);
      case ParallelExecution:
        return inlineNewDenseArrayForParallelExecution(callInfo);
      default:;
    }

    MOZ_ASSUME_UNREACHABLE("unknown ExecutionMode");
}

IonBuilder::InliningStatus
IonBuilder::inlineNewDenseArrayForSequentialExecution(CallInfo &callInfo)
{
    // not yet implemented; in seq. mode the C function is not so bad
    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineNewDenseArrayForParallelExecution(CallInfo &callInfo)
{
    // Create the new parallel array object.  Parallel arrays have specially
    // constructed type objects, so we can only perform the inlining if we
    // already have one of these type objects.
    types::StackTypeSet *returnTypes = getInlineReturnTypeSet();
    if (returnTypes->getKnownTypeTag() != JSVAL_TYPE_OBJECT)
        return InliningStatus_NotInlined;
    if (returnTypes->unknownObject() || returnTypes->getObjectCount() != 1)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Int32)
        return InliningStatus_NotInlined;
    types::TypeObject *typeObject = returnTypes->getTypeObject(0);

    RootedObject templateObject(cx, NewDenseAllocatedArray(cx, 0, NULL, TenuredObject));
    if (!templateObject)
        return InliningStatus_Error;
    templateObject->setType(typeObject);

    callInfo.unwrapArgs();

    MNewDenseArrayPar *newObject = new MNewDenseArrayPar(graph().forkJoinSlice(),
                                                         callInfo.getArg(0),
                                                         templateObject);
    current->add(newObject);
    current->push(newObject);

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineUnsafeSetReservedSlot(CallInfo &callInfo)
{
    if (callInfo.argc() != 3 || callInfo.constructing())
        return InliningStatus_NotInlined;
    if (getInlineReturnType() != MIRType_Undefined)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(1)->type() != MIRType_Int32)
        return InliningStatus_NotInlined;

    // Don't inline if we don't have a constant slot.
    MDefinition *arg = callInfo.getArg(1)->toPassArg()->getArgument();
    if (!arg->isConstant())
        return InliningStatus_NotInlined;
    uint32_t slot = arg->toConstant()->value().toPrivateUint32();

    callInfo.unwrapArgs();

    MStoreFixedSlot *store = MStoreFixedSlot::New(callInfo.getArg(0), slot, callInfo.getArg(2));
    current->add(store);
    current->push(store);

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineUnsafeGetReservedSlot(CallInfo &callInfo)
{
    if (callInfo.argc() != 2 || callInfo.constructing())
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(1)->type() != MIRType_Int32)
        return InliningStatus_NotInlined;

    // Don't inline if we don't have a constant slot.
    MDefinition *arg = callInfo.getArg(1)->toPassArg()->getArgument();
    if (!arg->isConstant())
        return InliningStatus_NotInlined;
    uint32_t slot = arg->toConstant()->value().toPrivateUint32();

    callInfo.unwrapArgs();

    MLoadFixedSlot *load = MLoadFixedSlot::New(callInfo.getArg(0), slot);
    current->add(load);
    current->push(load);

    // We don't track reserved slot types, so always emit a barrier.
    pushTypeBarrier(load, getInlineReturnTypeSet(), true);

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineNewObjectWithClassPrototype(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    MDefinition *arg = callInfo.getArg(0)->toPassArg()->getArgument();
    if (!arg->isConstant())
        return InliningStatus_NotInlined;
    JSObject *proto = &arg->toConstant()->value().toObject();

    JSObject *templateObject = NewObjectWithGivenProto(cx, proto->getClass(), proto, cx->global());
    if (!templateObject)
        return InliningStatus_Error;

    MNewObject *newObj = MNewObject::New(templateObject,
                                         /* templateObjectIsClassPrototype = */ true);
    current->add(newObj);
    current->push(newObj);

    if (!resumeAfter(newObj))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineHaveSameClass(CallInfo &callInfo)
{
    if (callInfo.argc() != 2 || callInfo.constructing())
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(1)->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    types::StackTypeSet *arg1Types = callInfo.getArg(0)->resultTypeSet();
    types::StackTypeSet *arg2Types = callInfo.getArg(1)->resultTypeSet();
    Class *arg1Clasp = arg1Types ? arg1Types->getKnownClass() : NULL;
    Class *arg2Clasp = arg2Types ? arg1Types->getKnownClass() : NULL;
    if (arg1Clasp && arg2Clasp) {
        MConstant *constant = MConstant::New(BooleanValue(arg1Clasp == arg2Clasp));
        current->add(constant);
        current->push(constant);
        return InliningStatus_Inlined;
    }

    callInfo.unwrapArgs();

    MHaveSameClass *sameClass = MHaveSameClass::New(callInfo.getArg(0), callInfo.getArg(1));
    current->add(sameClass);
    current->push(sameClass);

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineIsCallable(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Boolean)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    // Try inlining with constant true/false: only objects may be callable at
    // all, and if we know the class check if it is callable.
    bool isCallableKnown = false;
    bool isCallableConstant;
    if (callInfo.getArg(0)->type() != MIRType_Object) {
        isCallableKnown = true;
        isCallableConstant = false;
    } else {
        types::StackTypeSet *types = callInfo.getArg(0)->resultTypeSet();
        Class *clasp = types ? types->getKnownClass() : NULL;
        if (clasp) {
            isCallableKnown = true;
            isCallableConstant = clasp->isCallable();
        }
    }

    if (isCallableKnown) {
        MConstant *constant = MConstant::New(BooleanValue(isCallableConstant));
        current->add(constant);
        current->push(constant);
        return InliningStatus_Inlined;
    }

    callInfo.unwrapArgs();

    MIsCallable *isCallable = MIsCallable::New(callInfo.getArg(0));
    current->add(isCallable);
    current->push(isCallable);

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineToObject(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    // If we know the input type is an object, nop ToObject.
    if (getInlineReturnType() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    callInfo.unwrapArgs();
    MDefinition *object = callInfo.getArg(0);

    current->push(object);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineBailout(CallInfo &callInfo)
{
    callInfo.unwrapArgs();

    current->add(MBail::New());

    MConstant *undefined = MConstant::New(UndefinedValue());
    current->add(undefined);
    current->push(undefined);
    return InliningStatus_Inlined;
}

} // namespace jit
} // namespace js
