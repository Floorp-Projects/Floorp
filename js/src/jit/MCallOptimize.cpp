/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsmath.h"

#include "builtin/TestingFunctions.h"
#include "builtin/TypedObject.h"
#include "jit/BaselineInspector.h"
#include "jit/IonBuilder.h"
#include "jit/Lowering.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#include "vm/ArgumentsObject.h"

#include "jsscriptinlines.h"

#include "vm/StringObject-inl.h"

namespace js {
namespace jit {

IonBuilder::InliningStatus
IonBuilder::inlineNativeCall(CallInfo &callInfo, JSFunction *target)
{
    JS_ASSERT(target->isNative());
    JSNative native = target->native();

    if (!optimizationInfo().inlineNative())
        return InliningStatus_NotInlined;

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
    if (native == js::array_splice)
        return inlineArraySplice(callInfo);

    // Math natives.
    if (native == js_math_abs)
        return inlineMathAbs(callInfo);
    if (native == js::math_floor)
        return inlineMathFloor(callInfo);
    if (native == js::math_ceil)
        return inlineMathCeil(callInfo);
    if (native == js::math_round)
        return inlineMathRound(callInfo);
    if (native == js_math_sqrt)
        return inlineMathSqrt(callInfo);
    if (native == math_atan2)
        return inlineMathAtan2(callInfo);
    if (native == js::math_hypot)
        return inlineMathHypot(callInfo);
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
    if (native == js::math_fround)
        return inlineMathFRound(callInfo);
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
    if (native == js::math_sinh)
        return inlineMathFunction(callInfo, MMathFunction::SinH);
    if (native == js::math_tanh)
        return inlineMathFunction(callInfo, MMathFunction::TanH);
    if (native == js::math_acosh)
        return inlineMathFunction(callInfo, MMathFunction::ACosH);
    if (native == js::math_asinh)
        return inlineMathFunction(callInfo, MMathFunction::ASinH);
    if (native == js::math_atanh)
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
    if (native == js::str_split)
        return inlineStringSplit(callInfo);
    if (native == js_str_charCodeAt)
        return inlineStrCharCodeAt(callInfo);
    if (native == js::str_fromCharCode)
        return inlineStrFromCharCode(callInfo);
    if (native == js_str_charAt)
        return inlineStrCharAt(callInfo);
    if (native == str_replace)
        return inlineStrReplace(callInfo);

    // RegExp natives.
    if (native == regexp_exec && CallResultEscapes(pc))
        return inlineRegExpExec(callInfo);
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
    if (native == intrinsic_ShouldForceSequential ||
        native == intrinsic_InParallelSection)
        return inlineForceSequentialOrInParallelSection(callInfo);
    if (native == intrinsic_ForkJoinGetSlice)
        return inlineForkJoinGetSlice(callInfo);

    // Utility intrinsics.
    if (native == intrinsic_IsCallable)
        return inlineIsCallable(callInfo);
    if (native == intrinsic_HaveSameClass)
        return inlineHaveSameClass(callInfo);
    if (native == intrinsic_ToObject)
        return inlineToObject(callInfo);
    if (native == intrinsic_ToInteger)
        return inlineToInteger(callInfo);
    if (native == intrinsic_ToString)
        return inlineToString(callInfo);

    // TypedObject intrinsics.
    if (native == intrinsic_ObjectIsTypedObject)
        return inlineHasClasses(callInfo,
                                &TransparentTypedObject::class_, &OpaqueTypedObject::class_);
    if (native == intrinsic_ObjectIsTransparentTypedObject)
        return inlineHasClass(callInfo, &TransparentTypedObject::class_);
    if (native == intrinsic_ObjectIsOpaqueTypedObject)
        return inlineHasClass(callInfo, &OpaqueTypedObject::class_);
    if (native == intrinsic_ObjectIsTypeDescr)
        return inlineObjectIsTypeDescr(callInfo);
    if (native == intrinsic_TypeDescrIsSimpleType)
        return inlineHasClasses(callInfo,
                                &ScalarTypeDescr::class_, &ReferenceTypeDescr::class_);
    if (native == intrinsic_TypeDescrIsArrayType)
        return inlineHasClasses(callInfo,
                                &SizedArrayTypeDescr::class_, &UnsizedArrayTypeDescr::class_);
    if (native == intrinsic_TypeDescrIsSizedArrayType)
        return inlineHasClass(callInfo, &SizedArrayTypeDescr::class_);
    if (native == intrinsic_TypeDescrIsUnsizedArrayType)
        return inlineHasClass(callInfo, &UnsizedArrayTypeDescr::class_);
    if (native == intrinsic_SetTypedObjectOffset)
        return inlineSetTypedObjectOffset(callInfo);

    // Testing Functions
    if (native == testingFunc_inParallelSection)
        return inlineForceSequentialOrInParallelSection(callInfo);
    if (native == testingFunc_bailout)
        return inlineBailout(callInfo);
    if (native == testingFunc_assertFloat32)
        return inlineAssertFloat32(callInfo);

    // Bound function
    if (native == js::CallOrConstructBoundFunction)
        return inlineBoundFunction(callInfo, target);

    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineNativeGetter(CallInfo &callInfo, JSFunction *target)
{
    JS_ASSERT(target->isNative());
    JSNative native = target->native();

    if (!optimizationInfo().inlineNative())
        return InliningStatus_NotInlined;

    types::TemporaryTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    JS_ASSERT(callInfo.argc() == 0);

    // Try to optimize typed array lengths. There is one getter for each
    // typed array prototype, and make sure we are accessing the right one
    // for the type of the instance object.
    if (thisTypes) {
        ScalarTypeDescr::Type type = (ScalarTypeDescr::Type) thisTypes->getTypedArrayType();
        if (type != ScalarTypeDescr::TYPE_MAX &&
            TypedArrayObject::isOriginalLengthGetter(type, native))
        {
            MInstruction *length = addTypedArrayLength(callInfo.thisArg());
            current->push(length);
            return InliningStatus_Inlined;
        }
    }

    return InliningStatus_NotInlined;
}

types::TemporaryTypeSet *
IonBuilder::getInlineReturnTypeSet()
{
    return bytecodeTypes(pc);
}

MIRType
IonBuilder::getInlineReturnType()
{
    types::TemporaryTypeSet *returnTypes = getInlineReturnTypeSet();
    return returnTypes->getKnownMIRType();
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

    const MathCache *cache = compartment->runtime()->maybeGetMathCache();

    callInfo.fun()->setImplicitlyUsedUnchecked();
    callInfo.thisArg()->setImplicitlyUsedUnchecked();

    MMathFunction *ins = MMathFunction::New(alloc(), callInfo.getArg(0), function, cache);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArray(CallInfo &callInfo)
{
    uint32_t initLength = 0;
    MNewArray::AllocatingBehaviour allocating = MNewArray::NewArray_Unallocating;

    JSObject *templateObject = inspector->getTemplateObjectForNative(pc, js_Array);
    if (!templateObject)
        return InliningStatus_NotInlined;
    JS_ASSERT(templateObject->is<ArrayObject>());

    // Multiple arguments imply array initialization, not just construction.
    if (callInfo.argc() >= 2) {
        initLength = callInfo.argc();
        allocating = MNewArray::NewArray_Allocating;

        types::TypeObjectKey *type = types::TypeObjectKey::get(templateObject);
        if (!type->unknownProperties()) {
            types::HeapTypeSetKey elemTypes = type->property(JSID_VOID);

            for (uint32_t i = 0; i < initLength; i++) {
                MDefinition *value = callInfo.getArg(i);
                if (!TypeSetIncludes(elemTypes.maybeTypes(), value->type(), value->resultTypeSet())) {
                    elemTypes.freeze(constraints());
                    return InliningStatus_NotInlined;
                }
            }
        }
    }

    // A single integer argument denotes initial length.
    if (callInfo.argc() == 1) {
        if (callInfo.getArg(0)->type() != MIRType_Int32)
            return InliningStatus_NotInlined;
        MDefinition *arg = callInfo.getArg(0);
        if (!arg->isConstant())
            return InliningStatus_NotInlined;

        // Negative lengths generate a RangeError, unhandled by the inline path.
        initLength = arg->toConstant()->value().toInt32();
        if (initLength >= JSObject::NELEMENTS_LIMIT)
            return InliningStatus_NotInlined;

        // Make sure initLength matches the template object's length. This is
        // not guaranteed to be the case, for instance if we're inlining the
        // MConstant may come from an outer script.
        if (initLength != templateObject->as<ArrayObject>().length())
            return InliningStatus_NotInlined;

        if (initLength <= ArrayObject::EagerAllocationMaxLength)
            allocating = MNewArray::NewArray_Allocating;
    }

    callInfo.setImplicitlyUsedUnchecked();

    types::TemporaryTypeSet::DoubleConversion conversion =
        getInlineReturnTypeSet()->convertDoubleElements(constraints());
    if (conversion == types::TemporaryTypeSet::AlwaysConvertToDoubles)
        templateObject->setShouldConvertDoubleElements();
    else
        templateObject->clearShouldConvertDoubleElements();

    MNewArray *ins = MNewArray::New(alloc(), constraints(), initLength, templateObject,
                                    templateObject->type()->initialHeap(constraints()),
                                    allocating);
    current->add(ins);
    current->push(ins);

    if (callInfo.argc() >= 2) {
        // Get the elements vector.
        MElements *elements = MElements::New(alloc(), ins);
        current->add(elements);

        // Store all values, no need to initialize the length after each as
        // jsop_initelem_array is doing because we do not expect to bailout
        // because the memory is supposed to be allocated by now.
        MConstant *id = nullptr;
        for (uint32_t i = 0; i < initLength; i++) {
            id = MConstant::New(alloc(), Int32Value(i));
            current->add(id);

            MDefinition *value = callInfo.getArg(i);
            if (conversion == types::TemporaryTypeSet::AlwaysConvertToDoubles) {
                MInstruction *valueDouble = MToDouble::New(alloc(), value);
                current->add(valueDouble);
                value = valueDouble;
            }

            // There is normally no need for a post barrier on these writes
            // because the new array will be in the nursery. However, this
            // assumption is volated if we specifically requested pre-tenuring.
            if (ins->initialHeap() == gc::TenuredHeap)
                current->add(MPostWriteBarrier::New(alloc(), ins, value));

            MStoreElement *store = MStoreElement::New(alloc(), elements, id, value,
                                                      /* needsHoleCheck = */ false);
            current->add(store);
        }

        // Update the length.
        MSetInitializedLength *length = MSetInitializedLength::New(alloc(), elements, id);
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

    types::TemporaryTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    if (!thisTypes || thisTypes->getKnownClass() != &ArrayObject::class_)
        return InliningStatus_NotInlined;
    if (thisTypes->hasObjectFlags(constraints(), unhandledFlags))
        return InliningStatus_NotInlined;

    if (types::ArrayPrototypeHasIndexedProperty(constraints(), script()))
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    types::TemporaryTypeSet *returnTypes = getInlineReturnTypeSet();
    bool needsHoleCheck = thisTypes->hasObjectFlags(constraints(), types::OBJECT_FLAG_NON_PACKED);
    bool maybeUndefined = returnTypes->hasType(types::Type::UndefinedType());

    BarrierKind barrier = PropertyReadNeedsTypeBarrier(analysisContext, constraints(),
                                                       callInfo.thisArg(), nullptr, returnTypes);
    if (barrier != BarrierKind::NoBarrier)
        returnType = MIRType_Value;

    MArrayPopShift *ins = MArrayPopShift::New(alloc(), callInfo.thisArg(), mode,
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
IonBuilder::inlineArraySplice(CallInfo &callInfo)
{
    if (callInfo.argc() != 2 || callInfo.constructing())
        return InliningStatus_NotInlined;

    // Ensure |this|, argument and result are objects.
    if (getInlineReturnType() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (callInfo.thisArg()->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Int32)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(1)->type() != MIRType_Int32)
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    // Specialize arr.splice(start, deleteCount) with unused return value and
    // avoid creating the result array in this case.
    if (!BytecodeIsPopped(pc))
        return InliningStatus_NotInlined;

    MArraySplice *ins = MArraySplice::New(alloc(),
                                          callInfo.thisArg(),
                                          callInfo.getArg(0),
                                          callInfo.getArg(1));

    current->add(ins);
    pushConstant(UndefinedValue());

    if (!resumeAfter(ins))
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
    if (PropertyWriteNeedsTypeBarrier(alloc(), constraints(), current,
                                      &obj, nullptr, &value, /* canModify = */ false))
    {
        return InliningStatus_NotInlined;
    }
    JS_ASSERT(obj == callInfo.thisArg() && value == callInfo.getArg(0));

    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;
    if (callInfo.thisArg()->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    types::TemporaryTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    if (!thisTypes || thisTypes->getKnownClass() != &ArrayObject::class_)
        return InliningStatus_NotInlined;
    if (thisTypes->hasObjectFlags(constraints(), types::OBJECT_FLAG_SPARSE_INDEXES |
                                  types::OBJECT_FLAG_LENGTH_OVERFLOW))
    {
        return InliningStatus_NotInlined;
    }

    if (types::ArrayPrototypeHasIndexedProperty(constraints(), script()))
        return InliningStatus_NotInlined;

    types::TemporaryTypeSet::DoubleConversion conversion =
        thisTypes->convertDoubleElements(constraints());
    if (conversion == types::TemporaryTypeSet::AmbiguousDoubleConversion)
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();
    value = callInfo.getArg(0);

    if (conversion == types::TemporaryTypeSet::AlwaysConvertToDoubles ||
        conversion == types::TemporaryTypeSet::MaybeConvertToDoubles)
    {
        MInstruction *valueDouble = MToDouble::New(alloc(), value);
        current->add(valueDouble);
        value = valueDouble;
    }

    if (NeedsPostBarrier(info(), value))
        current->add(MPostWriteBarrier::New(alloc(), callInfo.thisArg(), value));

    MArrayPush *ins = MArrayPush::New(alloc(), callInfo.thisArg(), value);
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
    types::TemporaryTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    types::TemporaryTypeSet *argTypes = callInfo.getArg(0)->resultTypeSet();
    if (!thisTypes || !argTypes)
        return InliningStatus_NotInlined;

    if (thisTypes->getKnownClass() != &ArrayObject::class_)
        return InliningStatus_NotInlined;
    if (thisTypes->hasObjectFlags(constraints(), types::OBJECT_FLAG_SPARSE_INDEXES |
                                  types::OBJECT_FLAG_LENGTH_OVERFLOW))
    {
        return InliningStatus_NotInlined;
    }

    if (argTypes->getKnownClass() != &ArrayObject::class_)
        return InliningStatus_NotInlined;
    if (argTypes->hasObjectFlags(constraints(), types::OBJECT_FLAG_SPARSE_INDEXES |
                                 types::OBJECT_FLAG_LENGTH_OVERFLOW))
    {
        return InliningStatus_NotInlined;
    }

    // Watch out for indexed properties on the prototype.
    if (types::ArrayPrototypeHasIndexedProperty(constraints(), script()))
        return InliningStatus_NotInlined;

    // Require the 'this' types to have a specific type matching the current
    // global, so we can create the result object inline.
    if (thisTypes->getObjectCount() != 1)
        return InliningStatus_NotInlined;

    types::TypeObject *baseThisType = thisTypes->getTypeObject(0);
    if (!baseThisType)
        return InliningStatus_NotInlined;
    types::TypeObjectKey *thisType = types::TypeObjectKey::get(baseThisType);
    if (thisType->unknownProperties())
        return InliningStatus_NotInlined;

    // Don't inline if 'this' is packed and the argument may not be packed
    // (the result array will reuse the 'this' type).
    if (!thisTypes->hasObjectFlags(constraints(), types::OBJECT_FLAG_NON_PACKED) &&
        argTypes->hasObjectFlags(constraints(), types::OBJECT_FLAG_NON_PACKED))
    {
        return InliningStatus_NotInlined;
    }

    // Constraints modeling this concat have not been generated by inference,
    // so check that type information already reflects possible side effects of
    // this call.
    types::HeapTypeSetKey thisElemTypes = thisType->property(JSID_VOID);

    types::TemporaryTypeSet *resTypes = getInlineReturnTypeSet();
    if (!resTypes->hasType(types::Type::ObjectType(thisType)))
        return InliningStatus_NotInlined;

    for (unsigned i = 0; i < argTypes->getObjectCount(); i++) {
        types::TypeObjectKey *argType = argTypes->getObject(i);
        if (!argType)
            continue;

        if (argType->unknownProperties())
            return InliningStatus_NotInlined;

        types::HeapTypeSetKey elemTypes = argType->property(JSID_VOID);
        if (!elemTypes.knownSubset(constraints(), thisElemTypes))
            return InliningStatus_NotInlined;
    }

    // Inline the call.
    JSObject *templateObj = inspector->getTemplateObjectForNative(pc, js::array_concat);
    if (!templateObj || templateObj->type() != baseThisType)
        return InliningStatus_NotInlined;
    JS_ASSERT(templateObj->is<ArrayObject>());

    callInfo.setImplicitlyUsedUnchecked();

    MArrayConcat *ins = MArrayConcat::New(alloc(), constraints(), callInfo.thisArg(), callInfo.getArg(0),
                                          templateObj, templateObj->type()->initialHeap(constraints()));
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
    if (!IsNumberType(argType))
        return InliningStatus_NotInlined;

    // Either argType == returnType, or
    //        argType == Double or Float32, returnType == Int, or
    //        argType == Float32, returnType == Double
    if (argType != returnType && !(IsFloatingPointType(argType) && returnType == MIRType_Int32)
        && !(argType == MIRType_Float32 && returnType == MIRType_Double))
    {
        return InliningStatus_NotInlined;
    }

    callInfo.setImplicitlyUsedUnchecked();

    // If the arg is a Float32, we specialize the op as double, it will be specialized
    // as float32 if necessary later.
    MIRType absType = (argType == MIRType_Float32) ? MIRType_Double : argType;
    MInstruction *ins = MAbs::New(alloc(), callInfo.getArg(0), absType);
    current->add(ins);

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
    MIRType returnType = getInlineReturnType();

    // Math.floor(int(x)) == int(x)
    if (argType == MIRType_Int32 && returnType == MIRType_Int32) {
        callInfo.setImplicitlyUsedUnchecked();
        // The int operand may be something which bails out if the actual value
        // is not in the range of the result type of the MIR. We need to tell
        // the optimizer to preserve this bailout even if the final result is
        // fully truncated.
        MLimitedTruncate *ins = MLimitedTruncate::New(alloc(), callInfo.getArg(0),
                                                      MDefinition::IndirectTruncate);
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    if (IsFloatingPointType(argType) && returnType == MIRType_Int32) {
        callInfo.setImplicitlyUsedUnchecked();
        MFloor *ins = MFloor::New(alloc(), callInfo.getArg(0));
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    if (IsFloatingPointType(argType) && returnType == MIRType_Double) {
        callInfo.setImplicitlyUsedUnchecked();
        MMathFunction *ins = MMathFunction::New(alloc(), callInfo.getArg(0), MMathFunction::Floor, nullptr);
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathCeil(CallInfo &callInfo)
{
    if (callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    MIRType argType = callInfo.getArg(0)->type();
    MIRType returnType = getInlineReturnType();

    // Math.ceil(int(x)) == int(x)
    if (argType == MIRType_Int32 && returnType == MIRType_Int32) {
        callInfo.setImplicitlyUsedUnchecked();
        // The int operand may be something which bails out if the actual value
        // is not in the range of the result type of the MIR. We need to tell
        // the optimizer to preserve this bailout even if the final result is
        // fully truncated.
        MLimitedTruncate *ins = MLimitedTruncate::New(alloc(), callInfo.getArg(0),
                                                      MDefinition::IndirectTruncate);
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    if (IsFloatingPointType(argType) && returnType == MIRType_Int32) {
        callInfo.setImplicitlyUsedUnchecked();
        MCeil *ins = MCeil::New(alloc(), callInfo.getArg(0));
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    if (IsFloatingPointType(argType) && returnType == MIRType_Double) {
        callInfo.setImplicitlyUsedUnchecked();
        MMathFunction *ins = MMathFunction::New(alloc(), callInfo.getArg(0), MMathFunction::Ceil, nullptr);
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
        callInfo.setImplicitlyUsedUnchecked();
        // The int operand may be something which bails out if the actual value
        // is not in the range of the result type of the MIR. We need to tell
        // the optimizer to preserve this bailout even if the final result is
        // fully truncated.
        MLimitedTruncate *ins = MLimitedTruncate::New(alloc(), callInfo.getArg(0),
                                                      MDefinition::IndirectTruncate);
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    if (IsFloatingPointType(argType) && returnType == MIRType_Int32) {
        callInfo.setImplicitlyUsedUnchecked();
        MRound *ins = MRound::New(alloc(), callInfo.getArg(0));
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    if (IsFloatingPointType(argType) && returnType == MIRType_Double) {
        callInfo.setImplicitlyUsedUnchecked();
        MMathFunction *ins = MMathFunction::New(alloc(), callInfo.getArg(0), MMathFunction::Round, nullptr);
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
    if (!IsNumberType(argType))
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MSqrt *sqrt = MSqrt::New(alloc(), callInfo.getArg(0));
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

    callInfo.setImplicitlyUsedUnchecked();

    MAtan2 *atan2 = MAtan2::New(alloc(), callInfo.getArg(0), callInfo.getArg(1));
    current->add(atan2);
    current->push(atan2);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathHypot(CallInfo &callInfo)
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

    callInfo.setImplicitlyUsedUnchecked();

    MHypot *hypot = MHypot::New(alloc(), callInfo.getArg(0), callInfo.getArg(1));
    current->add(hypot);
    current->push(hypot);
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
    if (!IsNumberType(baseType))
        return InliningStatus_NotInlined;
    if (!IsNumberType(powerType))
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MDefinition *base = callInfo.getArg(0);
    MDefinition *power = callInfo.getArg(1);
    MDefinition *output = nullptr;

    // Optimize some constant powers.
    if (callInfo.getArg(1)->isConstant() &&
        callInfo.getArg(1)->toConstant()->value().isNumber())
    {
        double pow = callInfo.getArg(1)->toConstant()->value().toNumber();

        // Math.pow(x, 0.5) is a sqrt with edge-case detection.
        if (pow == 0.5) {
            MPowHalf *half = MPowHalf::New(alloc(), base);
            current->add(half);
            output = half;
        }

        // Math.pow(x, -0.5) == 1 / Math.pow(x, 0.5), even for edge cases.
        if (pow == -0.5) {
            MPowHalf *half = MPowHalf::New(alloc(), base);
            current->add(half);
            MConstant *one = MConstant::New(alloc(), DoubleValue(1.0));
            current->add(one);
            MDiv *div = MDiv::New(alloc(), one, half, MIRType_Double);
            current->add(div);
            output = div;
        }

        // Math.pow(x, 1) == x.
        if (pow == 1.0)
            output = base;

        // Math.pow(x, 2) == x*x.
        if (pow == 2.0) {
            MMul *mul = MMul::New(alloc(), base, base, outputType);
            current->add(mul);
            output = mul;
        }

        // Math.pow(x, 3) == x*x*x.
        if (pow == 3.0) {
            MMul *mul1 = MMul::New(alloc(), base, base, outputType);
            current->add(mul1);
            MMul *mul2 = MMul::New(alloc(), base, mul1, outputType);
            current->add(mul2);
            output = mul2;
        }

        // Math.pow(x, 4) == y*y, where y = x*x.
        if (pow == 4.0) {
            MMul *y = MMul::New(alloc(), base, base, outputType);
            current->add(y);
            MMul *mul = MMul::New(alloc(), y, y, outputType);
            current->add(mul);
            output = mul;
        }
    }

    // Use MPow for other powers
    if (!output) {
        if (powerType == MIRType_Float32)
            powerType = MIRType_Double;
        MPow *pow = MPow::New(alloc(), base, power, powerType);
        current->add(pow);
        output = pow;
    }

    // Cast to the right type
    if (outputType == MIRType_Int32 && output->type() != MIRType_Int32) {
        MToInt32 *toInt = MToInt32::New(alloc(), output);
        current->add(toInt);
        output = toInt;
    }
    if (outputType == MIRType_Double && output->type() != MIRType_Double) {
        MToDouble *toDouble = MToDouble::New(alloc(), output);
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

    callInfo.setImplicitlyUsedUnchecked();

    MRandom *rand = MRandom::New(alloc());
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

    if (!IsNumberType(callInfo.getArg(0)->type()))
        return InliningStatus_NotInlined;
    if (!IsNumberType(callInfo.getArg(1)->type()))
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MInstruction *first = MTruncateToInt32::New(alloc(), callInfo.getArg(0));
    current->add(first);

    MInstruction *second = MTruncateToInt32::New(alloc(), callInfo.getArg(1));
    current->add(second);

    MMul *ins = MMul::New(alloc(), first, second, MIRType_Int32, MMul::Integer);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathFRound(CallInfo &callInfo)
{
    if (!LIRGenerator::allowFloat32Optimizations())
        return InliningStatus_NotInlined;

    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    // MIRType can't be Float32, as this point, as getInlineReturnType uses JSVal types
    // to infer the returned MIR type.
    types::TemporaryTypeSet *returned = getInlineReturnTypeSet();
    if (returned->empty()) {
        // As there's only one possible returned type, just add it to the observed
        // returned typeset
        returned->addType(types::Type::DoubleType(), alloc_->lifoAlloc());
    } else {
        MIRType returnType = getInlineReturnType();
        if (!IsNumberType(returnType))
            return InliningStatus_NotInlined;
    }

    MIRType arg = callInfo.getArg(0)->type();
    if (!IsNumberType(arg))
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MToFloat32 *ins = MToFloat32::New(alloc(), callInfo.getArg(0));
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathMinMax(CallInfo &callInfo, bool max)
{
    if (callInfo.argc() < 2 || callInfo.constructing())
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    if (!IsNumberType(returnType))
        return InliningStatus_NotInlined;

    for (unsigned i = 0; i < callInfo.argc(); i++) {
        MIRType argType = callInfo.getArg(i)->type();
        if (!IsNumberType(argType))
            return InliningStatus_NotInlined;

        // When one of the arguments is double, do a double MMinMax.
        if (returnType == MIRType_Int32 && IsFloatingPointType(argType))
            returnType = MIRType_Double;
    }

    callInfo.setImplicitlyUsedUnchecked();

    // Chain N-1 MMinMax instructions to compute the MinMax.
    MMinMax *last = MMinMax::New(alloc(), callInfo.getArg(0), callInfo.getArg(1), returnType, max);
    current->add(last);

    for (unsigned i = 2; i < callInfo.argc(); i++) {
        MMinMax *ins = MMinMax::New(alloc(), last, callInfo.getArg(i), returnType, max);
        current->add(ins);
        last = ins;
    }

    current->push(last);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStringObject(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || !callInfo.constructing())
        return InliningStatus_NotInlined;

    // ConvertToString doesn't support objects.
    if (callInfo.getArg(0)->mightBeType(MIRType_Object))
        return InliningStatus_NotInlined;

    JSObject *templateObj = inspector->getTemplateObjectForNative(pc, js_String);
    if (!templateObj)
        return InliningStatus_NotInlined;
    JS_ASSERT(templateObj->is<StringObject>());

    callInfo.setImplicitlyUsedUnchecked();

    MNewStringObject *ins = MNewStringObject::New(alloc(), callInfo.getArg(0), templateObj);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStringSplit(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;
    if (callInfo.thisArg()->type() != MIRType_String)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_String)
        return InliningStatus_NotInlined;

    JSObject *templateObject = inspector->getTemplateObjectForNative(pc, js::str_split);
    if (!templateObject)
        return InliningStatus_NotInlined;
    JS_ASSERT(templateObject->is<ArrayObject>());

    types::TypeObjectKey *retType = types::TypeObjectKey::get(templateObject);
    if (retType->unknownProperties())
        return InliningStatus_NotInlined;

    types::HeapTypeSetKey key = retType->property(JSID_VOID);
    if (!key.maybeTypes())
        return InliningStatus_NotInlined;

    if (!key.maybeTypes()->hasType(types::Type::StringType())) {
        key.freeze(constraints());
        return InliningStatus_NotInlined;
    }

    callInfo.setImplicitlyUsedUnchecked();

    MStringSplit *ins = MStringSplit::New(alloc(), constraints(), callInfo.thisArg(),
                                          callInfo.getArg(0), templateObject);
    current->add(ins);
    current->push(ins);

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

    callInfo.setImplicitlyUsedUnchecked();

    MInstruction *index = MToInt32::New(alloc(), callInfo.getArg(0));
    current->add(index);

    MStringLength *length = MStringLength::New(alloc(), callInfo.thisArg());
    current->add(length);

    index = addBoundsCheck(index, length);

    MCharCodeAt *charCode = MCharCodeAt::New(alloc(), callInfo.thisArg(), index);
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

    callInfo.setImplicitlyUsedUnchecked();

    MToInt32 *charCode = MToInt32::New(alloc(), callInfo.getArg(0));
    current->add(charCode);

    MFromCharCode *string = MFromCharCode::New(alloc(), charCode);
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

    callInfo.setImplicitlyUsedUnchecked();

    MInstruction *index = MToInt32::New(alloc(), callInfo.getArg(0));
    current->add(index);

    MStringLength *length = MStringLength::New(alloc(), callInfo.thisArg());
    current->add(length);

    index = addBoundsCheck(index, length);

    // String.charAt(x) = String.fromCharCode(String.charCodeAt(x))
    MCharCodeAt *charCode = MCharCodeAt::New(alloc(), callInfo.thisArg(), index);
    current->add(charCode);

    MFromCharCode *string = MFromCharCode::New(alloc(), charCode);
    current->add(string);
    current->push(string);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineRegExpExec(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    if (callInfo.thisArg()->type() != MIRType_Object)
        return InliningStatus_NotInlined;

    types::TemporaryTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    const Class *clasp = thisTypes ? thisTypes->getKnownClass() : nullptr;
    if (clasp != &RegExpObject::class_)
        return InliningStatus_NotInlined;

    if (callInfo.getArg(0)->mightBeType(MIRType_Object))
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MInstruction *exec = MRegExpExec::New(alloc(), callInfo.thisArg(), callInfo.getArg(0));
    current->add(exec);
    current->push(exec);

    if (!resumeAfter(exec))
        return InliningStatus_Error;

    if (!pushTypeBarrier(exec, getInlineReturnTypeSet(), BarrierKind::TypeSet))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineRegExpTest(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    // TI can infer a nullptr return type of regexp_test with eager compilation.
    if (CallResultEscapes(pc) && getInlineReturnType() != MIRType_Boolean)
        return InliningStatus_NotInlined;

    if (callInfo.thisArg()->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    types::TemporaryTypeSet *thisTypes = callInfo.thisArg()->resultTypeSet();
    const Class *clasp = thisTypes ? thisTypes->getKnownClass() : nullptr;
    if (clasp != &RegExpObject::class_)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->mightBeType(MIRType_Object))
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MInstruction *match = MRegExpTest::New(alloc(), callInfo.thisArg(), callInfo.getArg(0));
    current->add(match);
    current->push(match);
    if (!resumeAfter(match))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrReplace(CallInfo &callInfo)
{
    if (callInfo.argc() != 2 || callInfo.constructing())
        return InliningStatus_NotInlined;

    // Return: String.
    if (getInlineReturnType() != MIRType_String)
        return InliningStatus_NotInlined;

    // This: String.
    if (callInfo.thisArg()->type() != MIRType_String)
        return InliningStatus_NotInlined;

    // Arg 0: RegExp.
    types::TemporaryTypeSet *arg0Type = callInfo.getArg(0)->resultTypeSet();
    const Class *clasp = arg0Type ? arg0Type->getKnownClass() : nullptr;
    if (clasp != &RegExpObject::class_ && callInfo.getArg(0)->type() != MIRType_String)
        return InliningStatus_NotInlined;

    // Arg 1: String.
    if (callInfo.getArg(1)->type() != MIRType_String)
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MInstruction *cte;
    if (callInfo.getArg(0)->type() == MIRType_String) {
        cte = MStringReplace::New(alloc(), callInfo.thisArg(), callInfo.getArg(0),
                                  callInfo.getArg(1));
    } else {
        cte = MRegExpReplace::New(alloc(), callInfo.thisArg(), callInfo.getArg(0),
                                  callInfo.getArg(1));
    }
    current->add(cte);
    current->push(cte);
    if (cte->isEffectful() && !resumeAfter(cte))
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
            writeNeedsBarrier = PropertyWriteNeedsTypeBarrier(alloc(), constraints(), current,
                                                              &obj, nullptr, &elem,
                                                              /* canModify = */ false);
        }

        // We can only inline setelem on dense arrays that do not need type
        // barriers and on typed arrays and on typed object arrays.
        ScalarTypeDescr::Type arrayType;
        if ((!isDenseNative || writeNeedsBarrier) &&
            !ElementAccessIsTypedArray(obj, id, &arrayType) &&
            !elementAccessIsTypedObjectArrayOfScalarType(obj, id, &arrayType))
        {
            return InliningStatus_NotInlined;
        }
    }

    callInfo.setImplicitlyUsedUnchecked();

    // Push the result first so that the stack depth matches up for
    // the potential bailouts that will occur in the stores below.
    MConstant *udef = MConstant::New(alloc(), UndefinedValue());
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

        ScalarTypeDescr::Type arrayType;
        if (ElementAccessIsTypedArray(obj, id, &arrayType)) {
            if (!inlineUnsafeSetTypedArrayElement(callInfo, base, arrayType))
                return InliningStatus_Error;
            continue;
        }

        if (elementAccessIsTypedObjectArrayOfScalarType(obj, id, &arrayType)) {
            if (!inlineUnsafeSetTypedObjectArrayElement(callInfo, base, arrayType))
                return InliningStatus_Error;
            continue;
        }

        MOZ_ASSUME_UNREACHABLE("Element access not dense array nor typed array");
    }

    return InliningStatus_Inlined;
}

bool
IonBuilder::elementAccessIsTypedObjectArrayOfScalarType(MDefinition* obj, MDefinition* id,
                                                        ScalarTypeDescr::Type *arrayType)
{
    if (obj->type() != MIRType_Object) // lookupTypeDescrSet() tests for TypedObject
        return false;

    if (id->type() != MIRType_Int32 && id->type() != MIRType_Double)
        return false;

    TypeDescrSet objDescrs;
    if (!lookupTypeDescrSet(obj, &objDescrs))
        return false;

    if (!objDescrs.allOfArrayKind())
        return false;

    TypeDescrSet elemDescrs;
    if (!objDescrs.arrayElementType(*this, &elemDescrs))
        return false;

    if (elemDescrs.empty() || elemDescrs.kind() != TypeDescr::Scalar)
        return false;

    JS_ASSERT(TypeDescr::isSized(elemDescrs.kind()));

    return elemDescrs.scalarType(arrayType);
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

    types::TemporaryTypeSet::DoubleConversion conversion =
        obj->resultTypeSet()->convertDoubleElements(constraints());
    if (!jsop_setelem_dense(conversion, SetElem_Unsafe, obj, id, elem))
        return false;
    return true;
}

bool
IonBuilder::inlineUnsafeSetTypedArrayElement(CallInfo &callInfo,
                                             uint32_t base,
                                             ScalarTypeDescr::Type arrayType)
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

bool
IonBuilder::inlineUnsafeSetTypedObjectArrayElement(CallInfo &callInfo,
                                                   uint32_t base,
                                                   ScalarTypeDescr::Type arrayType)
{
    // Note: we do not check the conditions that are asserted as true
    // in intrinsic_UnsafePutElements():
    // - arr is a typed array
    // - idx < length

    MDefinition *obj = callInfo.getArg(base + 0);
    MDefinition *id = callInfo.getArg(base + 1);
    MDefinition *elem = callInfo.getArg(base + 2);

    if (!jsop_setelem_typed_object(arrayType, SetElem_Unsafe, true, obj, id, elem))
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
      case ParallelExecution: {
        // During Parallel Exec, we always force sequential, so
        // replace with true.  This permits UCE to eliminate the
        // entire path as dead, which is important.
        callInfo.setImplicitlyUsedUnchecked();
        MConstant *ins = MConstant::New(alloc(), BooleanValue(true));
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
      }

      default:
        // In sequential mode, leave as is, because we'd have to
        // access the "in warmup" flag of the runtime.
        return InliningStatus_NotInlined;
    }

    MOZ_ASSUME_UNREACHABLE("Invalid execution mode");
}

IonBuilder::InliningStatus
IonBuilder::inlineForkJoinGetSlice(CallInfo &callInfo)
{
    if (info().executionMode() != ParallelExecution)
        return InliningStatus_NotInlined;

    // Assert the way the function is used instead of testing, as it is a
    // self-hosted function which must be used in a particular fashion.
    MOZ_ASSERT(callInfo.argc() == 1 && !callInfo.constructing());
    MOZ_ASSERT(callInfo.getArg(0)->type() == MIRType_Int32);

    // Test this, as we might have not executed the native despite knowing the
    // target here.
    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    switch (info().executionMode()) {
      case ParallelExecution:
        if (LIRGenerator::allowInlineForkJoinGetSlice()) {
            MForkJoinGetSlice *getSlice = MForkJoinGetSlice::New(alloc(),
                                                                 graph().forkJoinContext());
            current->add(getSlice);
            current->push(getSlice);
            return InliningStatus_Inlined;
        }
        return InliningStatus_NotInlined;

      default:
        // ForkJoinGetSlice acts as identity for sequential execution.
        current->push(callInfo.getArg(0));
        return InliningStatus_Inlined;
    }

    MOZ_ASSUME_UNREACHABLE("Invalid execution mode");
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
      case ParallelExecution:
        return inlineNewDenseArrayForParallelExecution(callInfo);
      default:
        return inlineNewDenseArrayForSequentialExecution(callInfo);
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
    types::TemporaryTypeSet *returnTypes = getInlineReturnTypeSet();
    if (returnTypes->getKnownMIRType() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (returnTypes->unknownObject() || returnTypes->getObjectCount() != 1)
        return InliningStatus_NotInlined;
    if (callInfo.getArg(0)->type() != MIRType_Int32)
        return InliningStatus_NotInlined;
    types::TypeObject *typeObject = returnTypes->getTypeObject(0);

    JSObject *templateObject = inspector->getTemplateObjectForNative(pc, intrinsic_NewDenseArray);
    if (!templateObject || templateObject->type() != typeObject)
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MNewDenseArrayPar *newObject = MNewDenseArrayPar::New(alloc(),
                                                          graph().forkJoinContext(),
                                                          callInfo.getArg(0),
                                                          templateObject);
    current->add(newObject);
    current->push(newObject);

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineHasClasses(CallInfo &callInfo, const Class *clasp1, const Class *clasp2)
{
    // Thus far there has been no reason to complicate this beyond two classes,
    // though it generalizes pretty well.
    // clasp2 may be NULL.
    if (callInfo.constructing() || callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (getInlineReturnType() != MIRType_Boolean)
        return InliningStatus_NotInlined;

    types::TemporaryTypeSet *types = callInfo.getArg(0)->resultTypeSet();
    const Class *knownClass = types ? types->getKnownClass() : nullptr;
    if (knownClass) {
        pushConstant(BooleanValue(knownClass == clasp1 || knownClass == clasp2));
    } else {
        MHasClass *hasClass1 = MHasClass::New(alloc(), callInfo.getArg(0), clasp1);
        current->add(hasClass1);
        if (clasp2 == nullptr) {
            current->push(hasClass1);
        } else {
            // The following turns into branch-free, box-free code on x86, and should do so on ARM.
            MHasClass *hasClass2 = MHasClass::New(alloc(), callInfo.getArg(0), clasp2);
            current->add(hasClass2);
            MBitOr *either = MBitOr::New(alloc(), hasClass1, hasClass2);
            either->infer(inspector, pc);
            current->add(either);
            // Convert to bool with the '!!' idiom
            MNot *resultInverted = MNot::New(alloc(), either);
            resultInverted->cacheOperandMightEmulateUndefined();
            current->add(resultInverted);
            MNot *result = MNot::New(alloc(), resultInverted);
            result->cacheOperandMightEmulateUndefined();
            current->add(result);
            current->push(result);
        }
    }

    callInfo.setImplicitlyUsedUnchecked();
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineObjectIsTypeDescr(CallInfo &callInfo)
{
    if (callInfo.constructing() || callInfo.argc() != 1)
        return InliningStatus_NotInlined;

    if (callInfo.getArg(0)->type() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (getInlineReturnType() != MIRType_Boolean)
        return InliningStatus_NotInlined;

    // The test is elaborate: in-line only if there is exact
    // information.

    types::TemporaryTypeSet *types = callInfo.getArg(0)->resultTypeSet();
    if (!types)
        return InliningStatus_NotInlined;

    bool result = false;
    switch (types->forAllClasses(IsTypeDescrClass)) {
    case types::TemporaryTypeSet::ForAllResult::ALL_FALSE:
    case types::TemporaryTypeSet::ForAllResult::EMPTY:
        result = false;
        break;
    case types::TemporaryTypeSet::ForAllResult::ALL_TRUE:
        result = true;
        break;
    case types::TemporaryTypeSet::ForAllResult::MIXED:
        return InliningStatus_NotInlined;
    }

    pushConstant(BooleanValue(result));

    callInfo.setImplicitlyUsedUnchecked();
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineSetTypedObjectOffset(CallInfo &callInfo)
{
    if (callInfo.argc() != 2 || callInfo.constructing())
        return InliningStatus_NotInlined;

    MDefinition *typedObj = callInfo.getArg(0);
    MDefinition *offset = callInfo.getArg(1);

    // Return type should be undefined or something wacky is going on.
    if (getInlineReturnType() != MIRType_Undefined)
        return InliningStatus_NotInlined;

    // Check typedObj is a, well, typed object. Go ahead and use TI
    // data. If this check should fail, that is almost certainly a bug
    // in self-hosted code -- either because it's not being careful
    // with TI or because of something else -- but we'll just let it
    // fall through to the SetTypedObjectOffset intrinsic in such
    // cases.
    types::TemporaryTypeSet *types = typedObj->resultTypeSet();
    if (typedObj->type() != MIRType_Object || !types)
        return InliningStatus_NotInlined;
    switch (types->forAllClasses(IsTypedObjectClass)) {
      case types::TemporaryTypeSet::ForAllResult::ALL_FALSE:
      case types::TemporaryTypeSet::ForAllResult::EMPTY:
      case types::TemporaryTypeSet::ForAllResult::MIXED:
        return InliningStatus_NotInlined;
      case types::TemporaryTypeSet::ForAllResult::ALL_TRUE:
        break;
    }

    // Check type of offset argument is an integer.
    if (offset->type() != MIRType_Int32)
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();
    MInstruction *ins = MSetTypedObjectOffset::New(alloc(), typedObj, offset);
    current->add(ins);
    current->push(ins);
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
    MDefinition *arg = callInfo.getArg(1);
    if (!arg->isConstant())
        return InliningStatus_NotInlined;
    uint32_t slot = arg->toConstant()->value().toPrivateUint32();

    callInfo.setImplicitlyUsedUnchecked();

    MStoreFixedSlot *store = MStoreFixedSlot::New(alloc(), callInfo.getArg(0), slot, callInfo.getArg(2));
    current->add(store);
    current->push(store);

    if (NeedsPostBarrier(info(), callInfo.getArg(2)))
        current->add(MPostWriteBarrier::New(alloc(), callInfo.thisArg(), callInfo.getArg(2)));

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
    MDefinition *arg = callInfo.getArg(1);
    if (!arg->isConstant())
        return InliningStatus_NotInlined;
    uint32_t slot = arg->toConstant()->value().toPrivateUint32();

    callInfo.setImplicitlyUsedUnchecked();

    MLoadFixedSlot *load = MLoadFixedSlot::New(alloc(), callInfo.getArg(0), slot);
    current->add(load);
    current->push(load);

    // We don't track reserved slot types, so always emit a barrier.
    if (!pushTypeBarrier(load, getInlineReturnTypeSet(), BarrierKind::TypeSet))
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

    types::TemporaryTypeSet *arg1Types = callInfo.getArg(0)->resultTypeSet();
    types::TemporaryTypeSet *arg2Types = callInfo.getArg(1)->resultTypeSet();
    const Class *arg1Clasp = arg1Types ? arg1Types->getKnownClass() : nullptr;
    const Class *arg2Clasp = arg2Types ? arg2Types->getKnownClass() : nullptr;
    if (arg1Clasp && arg2Clasp) {
        MConstant *constant = MConstant::New(alloc(), BooleanValue(arg1Clasp == arg2Clasp));
        current->add(constant);
        current->push(constant);
        return InliningStatus_Inlined;
    }

    callInfo.setImplicitlyUsedUnchecked();

    MHaveSameClass *sameClass = MHaveSameClass::New(alloc(), callInfo.getArg(0), callInfo.getArg(1));
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
        types::TemporaryTypeSet *types = callInfo.getArg(0)->resultTypeSet();
        const Class *clasp = types ? types->getKnownClass() : nullptr;
        if (clasp) {
            isCallableKnown = true;
            isCallableConstant = clasp->isCallable();
        }
    }

    callInfo.setImplicitlyUsedUnchecked();

    if (isCallableKnown) {
        MConstant *constant = MConstant::New(alloc(), BooleanValue(isCallableConstant));
        current->add(constant);
        current->push(constant);
        return InliningStatus_Inlined;
    }

    MIsCallable *isCallable = MIsCallable::New(alloc(), callInfo.getArg(0));
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

    callInfo.setImplicitlyUsedUnchecked();
    MDefinition *object = callInfo.getArg(0);

    current->push(object);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineToInteger(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    // Only fast-path if we know the input is in integer in the int32 range.
    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();

    MToInt32 *toInt32 = MToInt32::New(alloc(), callInfo.getArg(0));
    current->add(toInt32);
    current->push(toInt32);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineToString(CallInfo &callInfo)
{
    if (callInfo.argc() != 1 || callInfo.constructing())
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_String)
        return InliningStatus_NotInlined;

    callInfo.setImplicitlyUsedUnchecked();
    MToString *toString = MToString::New(alloc(), callInfo.getArg(0));
    current->add(toString);
    current->push(toString);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineBailout(CallInfo &callInfo)
{
    callInfo.setImplicitlyUsedUnchecked();

    current->add(MBail::New(alloc()));

    MConstant *undefined = MConstant::New(alloc(), UndefinedValue());
    current->add(undefined);
    current->push(undefined);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineAssertFloat32(CallInfo &callInfo)
{
    callInfo.setImplicitlyUsedUnchecked();

    MDefinition *secondArg = callInfo.getArg(1);

    JS_ASSERT(secondArg->type() == MIRType_Boolean);
    JS_ASSERT(secondArg->isConstant());

    bool mustBeFloat32 = secondArg->toConstant()->value().toBoolean();
    current->add(MAssertFloat32::New(alloc(), callInfo.getArg(0), mustBeFloat32));

    MConstant *undefined = MConstant::New(alloc(), UndefinedValue());
    current->add(undefined);
    current->push(undefined);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineBoundFunction(CallInfo &nativeCallInfo, JSFunction *target)
{
     if (!target->getBoundFunctionTarget()->is<JSFunction>())
         return InliningStatus_NotInlined;

    JSFunction *scriptedTarget = &(target->getBoundFunctionTarget()->as<JSFunction>());

    // Don't optimize if we're constructing and the callee is not a
    // constructor, so that CallKnown does not have to handle this case
    // (it should always throw).
    if (nativeCallInfo.constructing() && !scriptedTarget->isInterpretedConstructor() &&
        !scriptedTarget->isNativeConstructor())
    {
        return InliningStatus_NotInlined;
    }

    if (gc::IsInsideNursery(scriptedTarget))
        return InliningStatus_NotInlined;

    for (size_t i = 0; i < target->getBoundFunctionArgumentCount(); i++) {
        const Value val = target->getBoundFunctionArgument(i);
        if (val.isObject() && gc::IsInsideNursery(&val.toObject()))
            return InliningStatus_NotInlined;
    }

    const Value thisVal = target->getBoundFunctionThis();
    if (thisVal.isObject() && gc::IsInsideNursery(&thisVal.toObject()))
        return InliningStatus_NotInlined;

    size_t argc = target->getBoundFunctionArgumentCount() + nativeCallInfo.argc();
    if (argc > ARGS_LENGTH_MAX)
        return InliningStatus_NotInlined;

    nativeCallInfo.thisArg()->setImplicitlyUsedUnchecked();

    CallInfo callInfo(alloc(), nativeCallInfo.constructing());
    callInfo.setFun(constant(ObjectValue(*scriptedTarget)));
    callInfo.setThis(constant(target->getBoundFunctionThis()));

    if (!callInfo.argv().reserve(argc))
        return InliningStatus_Error;

    for (size_t i = 0; i < target->getBoundFunctionArgumentCount(); i++)
        callInfo.argv().infallibleAppend(constant(target->getBoundFunctionArgument(i)));
    for (size_t i = 0; i < nativeCallInfo.argc(); i++)
        callInfo.argv().infallibleAppend(nativeCallInfo.getArg(i));

    if (!makeCall(scriptedTarget, callInfo, false))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

} // namespace jit
} // namespace js
