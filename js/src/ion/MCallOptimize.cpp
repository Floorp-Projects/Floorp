/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jslibmath.h"
#include "jsmath.h"

#include "MIR.h"
#include "MIRGraph.h"
#include "IonBuilder.h"

#include "vm/StringObject-inl.h"

namespace js {
namespace ion {

IonBuilder::InliningStatus
IonBuilder::inlineNativeCall(JSNative native, uint32_t argc, bool constructing)
{
    // Array natives.
    if (native == js_Array)
        return inlineArray(argc, constructing);
    if (native == js::array_pop)
        return inlineArrayPopShift(MArrayPopShift::Pop, argc, constructing);
    if (native == js::array_shift)
        return inlineArrayPopShift(MArrayPopShift::Shift, argc, constructing);
    if (native == js::array_push)
        return inlineArrayPush(argc, constructing);
    if (native == js::array_concat)
        return inlineArrayConcat(argc, constructing);

    // Math natives.
    if (native == js_math_abs)
        return inlineMathAbs(argc, constructing);
    if (native == js_math_floor)
        return inlineMathFloor(argc, constructing);
    if (native == js_math_round)
        return inlineMathRound(argc, constructing);
    if (native == js_math_sqrt)
        return inlineMathSqrt(argc, constructing);
    if (native == js_math_max)
        return inlineMathMinMax(true /* max */, argc, constructing);
    if (native == js_math_min)
        return inlineMathMinMax(false /* max */, argc, constructing);
    if (native == js_math_pow)
        return inlineMathPow(argc, constructing);
    if (native == js_math_random)
        return inlineMathRandom(argc, constructing);
    if (native == js::math_sin)
        return inlineMathFunction(MMathFunction::Sin, argc, constructing);
    if (native == js::math_cos)
        return inlineMathFunction(MMathFunction::Cos, argc, constructing);
    if (native == js::math_tan)
        return inlineMathFunction(MMathFunction::Tan, argc, constructing);
    if (native == js::math_log)
        return inlineMathFunction(MMathFunction::Log, argc, constructing);

    // String natives.
    if (native == js_String)
        return inlineStringObject(argc, constructing);
    if (native == js_str_charCodeAt)
        return inlineStrCharCodeAt(argc, constructing);
    if (native == js::str_fromCharCode)
        return inlineStrFromCharCode(argc, constructing);
    if (native == js_str_charAt)
        return inlineStrCharAt(argc, constructing);

    // RegExp natives.
    if (native == regexp_exec && !CallResultEscapes(pc))
        return inlineRegExpTest(argc, constructing);
    if (native == regexp_test)
        return inlineRegExpTest(argc, constructing);

    return InliningStatus_NotInlined;
}

bool
IonBuilder::discardCallArgs(uint32_t argc, MDefinitionVector &argv, MBasicBlock *bb)
{
    if (!argv.resizeUninitialized(argc + 1))
        return false;

    for (int32_t i = argc; i >= 0; i--) {
        // Unwrap each MPassArg, replacing it with its contents.
        MPassArg *passArg = bb->pop()->toPassArg();
        MBasicBlock *block = passArg->block();
        MDefinition *wrapped = passArg->getArgument();
        passArg->replaceAllUsesWith(wrapped);
        block->discard(passArg);

        // Remember contents in vector.
        argv[i] = wrapped;
    }

    return true;
}

bool
IonBuilder::discardCall(uint32_t argc, MDefinitionVector &argv, MBasicBlock *bb)
{
    if (!discardCallArgs(argc, argv, bb))
        return false;

    // Function MDefinition implicitly consumed by inlining.
    bb->pop();
    return true;
}

types::StackTypeSet *
IonBuilder::getInlineReturnTypeSet()
{
    types::StackTypeSet *barrier;
    types::StackTypeSet *returnTypes = oracle->returnTypeSet(script(), pc, &barrier);

    JS_ASSERT(returnTypes);
    return returnTypes;
}

MIRType
IonBuilder::getInlineReturnType()
{
    types::StackTypeSet *returnTypes = getInlineReturnTypeSet();
    return MIRTypeFromValueType(returnTypes->getKnownTypeTag());
}

types::StackTypeSet *
IonBuilder::getInlineArgTypeSet(uint32_t argc, uint32_t arg)
{
    types::StackTypeSet *argTypes = oracle->getCallArg(script(), argc, arg, pc);
    JS_ASSERT(argTypes);
    return argTypes;
}

MIRType
IonBuilder::getInlineArgType(uint32_t argc, uint32_t arg)
{
    types::StackTypeSet *argTypes = getInlineArgTypeSet(argc, arg);
    return MIRTypeFromValueType(argTypes->getKnownTypeTag());
}

IonBuilder::InliningStatus
IonBuilder::inlineMathFunction(MMathFunction::Function function, uint32_t argc, bool constructing)
{
    if (constructing)
        return InliningStatus_NotInlined;

    if (argc != 1)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;
    if (!IsNumberType(getInlineArgType(argc, 1)))
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MathCache *cache = cx->runtime->getMathCache(cx);
    if (!cache)
        return InliningStatus_Error;

    MMathFunction *ins = MMathFunction::New(argv[1], function, cache);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArray(uint32_t argc, bool constructing)
{
    uint32_t initLength = 0;
    MNewArray::AllocatingBehaviour allocating = MNewArray::NewArray_Unallocating;

    // Multiple arguments imply array initialization, not just construction.
    if (argc >= 2) {
        initLength = argc;
        allocating = MNewArray::NewArray_Allocating;
    }

    // A single integer argument denotes initial length.
    if (argc == 1) {
        if (getInlineArgType(argc, 1) != MIRType_Int32)
            return InliningStatus_NotInlined;
        MDefinition *arg = current->peek(-1)->toPassArg()->getArgument();
        if (!arg->isConstant())
            return InliningStatus_NotInlined;

        // Negative lengths generate a RangeError, unhandled by the inline path.
        initLength = arg->toConstant()->value().toInt32();
        if (initLength >= JSObject::NELEMENTS_LIMIT)
            return InliningStatus_NotInlined;
    }

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    JSObject *templateObject = getNewArrayTemplateObject(initLength);
    if (!templateObject)
        return InliningStatus_Error;

    MNewArray *ins = new MNewArray(initLength, templateObject, allocating);
    current->add(ins);
    current->push(ins);

    if (argc >= 2) {
        // Get the elements vector.
        MElements *elements = MElements::New(ins);
        current->add(elements);

        // Store all values, no need to initialize the length after each as
        // jsop_initelem_array is doing because we do not expect to bailout
        // because the memory is supposed to be allocated by now.
        MConstant *id = NULL;
        for (uint32_t i = 0; i < initLength; i++) {
            id = MConstant::New(Int32Value(i));
            current->add(id);

            MStoreElement *store = MStoreElement::New(elements, id, argv[i + 1]);
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
IonBuilder::inlineArrayPopShift(MArrayPopShift::Mode mode, uint32_t argc, bool constructing)
{
    if (constructing)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    if (returnType == MIRType_Undefined || returnType == MIRType_Null)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_Object)
        return InliningStatus_NotInlined;

    // Pop and shift are only handled for dense arrays that have never been
    // used in an iterator: popping elements does not account for suppressing
    // deleted properties in active iterators.
    //
    // Inference's TypeConstraintCall generates the constraints that propagate
    // properties directly into the result type set.
    types::TypeObjectFlags unhandledFlags =
        types::OBJECT_FLAG_NON_DENSE_ARRAY | types::OBJECT_FLAG_ITERATED;

    types::StackTypeSet *thisTypes = getInlineArgTypeSet(argc, 0);
    if (thisTypes->hasObjectFlags(cx, unhandledFlags))
        return InliningStatus_NotInlined;
    RootedScript script(cx, script_);
    if (types::ArrayPrototypeHasIndexedProperty(cx, script))
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    types::StackTypeSet *returnTypes = getInlineReturnTypeSet();
    bool needsHoleCheck = thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
    bool maybeUndefined = returnTypes->hasType(types::Type::UndefinedType());

    MArrayPopShift *ins = MArrayPopShift::New(argv[0], mode, needsHoleCheck, maybeUndefined);
    current->add(ins);
    current->push(ins);
    ins->setResultType(returnType);

    if (!resumeAfter(ins))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArrayPush(uint32_t argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_Object)
        return InliningStatus_NotInlined;

    // Inference's TypeConstraintCall generates the constraints that propagate
    // properties directly into the result type set.
    types::StackTypeSet *thisTypes = getInlineArgTypeSet(argc, 0);
    if (thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY))
        return InliningStatus_NotInlined;
    RootedScript script(cx, script_);
    if (types::ArrayPrototypeHasIndexedProperty(cx, script))
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MArrayPush *ins = MArrayPush::New(argv[0], argv[1]);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineArrayConcat(uint32_t argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    // Ensure |this|, argument and result are objects.
    if (getInlineReturnType() != MIRType_Object)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_Object)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 1) != MIRType_Object)
        return InliningStatus_NotInlined;

    // |this| and the argument must be dense arrays.
    types::StackTypeSet *thisTypes = getInlineArgTypeSet(argc, 0);
    types::StackTypeSet *argTypes = getInlineArgTypeSet(argc, 1);

    if (thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY))
        return InliningStatus_NotInlined;

    if (argTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY))
        return InliningStatus_NotInlined;

    // Watch out for indexed properties on the prototype.
    RootedScript script(cx, script_);
    if (types::ArrayPrototypeHasIndexedProperty(cx, script))
        return InliningStatus_NotInlined;

    // Require the 'this' types to have a specific type matching the current
    // global, so we can create the result object inline.
    if (thisTypes->getObjectCount() != 1)
        return InliningStatus_NotInlined;

    types::TypeObject *thisType = thisTypes->getTypeObject(0);
    if (!thisType || &thisType->proto->global() != &script->global())
        return InliningStatus_NotInlined;

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

        types::HeapTypeSet *elemTypes = argType->getProperty(cx, JSID_VOID, false);
        if (!elemTypes)
            return InliningStatus_Error;

        if (!elemTypes->knownSubset(cx, thisElemTypes))
            return InliningStatus_NotInlined;
    }

    // Inline the call.
    RootedObject templateObj(cx, NewDenseEmptyArray(cx, thisType->proto));
    if (!templateObj)
        return InliningStatus_Error;
    templateObj->setType(thisType);

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MArrayConcat *ins = MArrayConcat::New(argv[0], argv[1], templateObj);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathAbs(uint32_t argc, bool constructing)
{
    if (constructing)
        return InliningStatus_NotInlined;

    if (argc != 1)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    MIRType argType = getInlineArgType(argc, 1);
    if (argType != MIRType_Int32 && argType != MIRType_Double)
        return InliningStatus_NotInlined;
    if (argType != returnType)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MAbs *ins = MAbs::New(argv[1], returnType);
    current->add(ins);
    current->push(ins);

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathFloor(uint32_t argc, bool constructing)
{

    if (constructing)
        return InliningStatus_NotInlined;

    if (argc != 1)
        return InliningStatus_NotInlined;

    MIRType argType = getInlineArgType(argc, 1);
    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;

    // Math.floor(int(x)) == int(x)
    if (argType == MIRType_Int32) {
        MDefinitionVector argv;
        if (!discardCall(argc, argv, current))
            return InliningStatus_Error;
        current->push(argv[1]);
        return InliningStatus_Inlined;
    }

    if (argType == MIRType_Double) {
        MDefinitionVector argv;
        if (!discardCall(argc, argv, current))
            return InliningStatus_Error;
        MFloor *ins = new MFloor(argv[1]);
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathRound(uint32_t argc, bool constructing)
{
    if (constructing)
        return InliningStatus_NotInlined;

    if (argc != 1)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    MIRType argType = getInlineArgType(argc, 1);

    // Math.round(int(x)) == int(x)
    if (argType == MIRType_Int32 && returnType == MIRType_Int32) {
        MDefinitionVector argv;
        if (!discardCall(argc, argv, current))
            return InliningStatus_Error;
        current->push(argv[1]);
        return InliningStatus_Inlined;
    }

    if (argType == MIRType_Double && returnType == MIRType_Int32) {
        MDefinitionVector argv;
        if (!discardCall(argc, argv, current))
            return InliningStatus_Error;
        MRound *ins = new MRound(argv[1]);
        current->add(ins);
        current->push(ins);
        return InliningStatus_Inlined;
    }

    return InliningStatus_NotInlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathSqrt(uint32_t argc, bool constructing)
{
    if (constructing)
        return InliningStatus_NotInlined;

    if (argc != 1)
        return InliningStatus_NotInlined;

    MIRType argType = getInlineArgType(argc, 1);
    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;
    if (argType != MIRType_Double && argType != MIRType_Int32)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MSqrt *sqrt = MSqrt::New(argv[1]);
    current->add(sqrt);
    current->push(sqrt);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathPow(uint32_t argc, bool constructing)
{
    if (constructing)
        return InliningStatus_NotInlined;

    if (argc != 2)
        return InliningStatus_NotInlined;

    // Typechecking.
    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;

    MIRType arg1Type = getInlineArgType(argc, 1);
    MIRType arg2Type = getInlineArgType(argc, 2);

    if (arg1Type != MIRType_Int32 && arg1Type != MIRType_Double)
        return InliningStatus_NotInlined;
    if (arg2Type != MIRType_Int32 && arg2Type != MIRType_Double)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    // If the non-power input is integer, convert it to a Double.
    // Safe since the output must be a Double.
    if (arg1Type == MIRType_Int32) {
        MToDouble *conv = MToDouble::New(argv[1]);
        current->add(conv);
        argv[1] = conv;
    }

    // Optimize some constant powers.
    if (argv[2]->isConstant()) {
        double pow;
        if (!ToNumber(GetIonContext()->cx, argv[2]->toConstant()->value(), &pow))
            return InliningStatus_Error;

        // Math.pow(x, 0.5) is a sqrt with edge-case detection.
        if (pow == 0.5) {
            MPowHalf *half = MPowHalf::New(argv[1]);
            current->add(half);
            current->push(half);
            return InliningStatus_Inlined;
        }

        // Math.pow(x, -0.5) == 1 / Math.pow(x, 0.5), even for edge cases.
        if (pow == -0.5) {
            MPowHalf *half = MPowHalf::New(argv[1]);
            current->add(half);
            MConstant *one = MConstant::New(DoubleValue(1.0));
            current->add(one);
            MDiv *div = MDiv::New(one, half, MIRType_Double);
            current->add(div);
            current->push(div);
            return InliningStatus_Inlined;
        }

        // Math.pow(x, 1) == x.
        if (pow == 1.0) {
            current->push(argv[1]);
            return InliningStatus_Inlined;
        }

        // Math.pow(x, 2) == x*x.
        if (pow == 2.0) {
            MMul *mul = MMul::New(argv[1], argv[1], MIRType_Double);
            current->add(mul);
            current->push(mul);
            return InliningStatus_Inlined;
        }

        // Math.pow(x, 3) == x*x*x.
        if (pow == 3.0) {
            MMul *mul1 = MMul::New(argv[1], argv[1], MIRType_Double);
            current->add(mul1);
            MMul *mul2 = MMul::New(argv[1], mul1, MIRType_Double);
            current->add(mul2);
            current->push(mul2);
            return InliningStatus_Inlined;
        }

        // Math.pow(x, 4) == y*y, where y = x*x.
        if (pow == 4.0) {
            MMul *y = MMul::New(argv[1], argv[1], MIRType_Double);
            current->add(y);
            MMul *mul = MMul::New(y, y, MIRType_Double);
            current->add(mul);
            current->push(mul);
            return InliningStatus_Inlined;
        }
    }

    MPow *ins = MPow::New(argv[1], argv[2], arg2Type);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathRandom(uint32_t argc, bool constructing)
{
    if (constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Double)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MRandom *rand = MRandom::New();
    current->add(rand);
    current->push(rand);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathMinMax(bool max, uint32_t argc, bool constructing)
{
    if (argc != 2 || constructing)
        return InliningStatus_NotInlined;

    MIRType returnType = getInlineReturnType();
    if (returnType != MIRType_Double && returnType != MIRType_Int32)
        return InliningStatus_NotInlined;

    MIRType arg1Type = getInlineArgType(argc, 1);
    if (arg1Type != MIRType_Double && arg1Type != MIRType_Int32)
        return InliningStatus_NotInlined;
    MIRType arg2Type = getInlineArgType(argc, 2);
    if (arg2Type != MIRType_Double && arg2Type != MIRType_Int32)
        return InliningStatus_NotInlined;

    if (returnType == MIRType_Int32 &&
        (arg1Type == MIRType_Double || arg2Type == MIRType_Double))
    {
        // We would need to inform TI, if we happen to return a double.
        return InliningStatus_NotInlined;
    }

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MMinMax *ins = MMinMax::New(argv[1], argv[2], returnType, max);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStringObject(uint32_t argc, bool constructing)
{
    if (argc != 1 || !constructing)
        return InliningStatus_NotInlined;

    // MToString only supports int32 or string values.
    MIRType type = getInlineArgType(argc, 1);
    if (type != MIRType_Int32 && type != MIRType_String)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    RootedString emptyString(cx, cx->runtime->emptyString);
    RootedObject templateObj(cx, StringObject::create(cx, emptyString));
    if (!templateObj)
        return InliningStatus_Error;

    MNewStringObject *ins = MNewStringObject::New(argv[1], templateObj);
    current->add(ins);
    current->push(ins);

    if (!resumeAfter(ins))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrCharCodeAt(uint32_t argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_Int32)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_String)
        return InliningStatus_NotInlined;
    MIRType argType = getInlineArgType(argc, 1);
    if (argType != MIRType_Int32 && argType != MIRType_Double)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MInstruction *index = MToInt32::New(argv[1]);
    current->add(index);

    MStringLength *length = MStringLength::New(argv[0]);
    current->add(length);

    index = addBoundsCheck(index, length);

    MCharCodeAt *charCode = MCharCodeAt::New(argv[0], index);
    current->add(charCode);
    current->push(charCode);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrFromCharCode(uint32_t argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_String)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 1) != MIRType_Int32)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MToInt32 *charCode = MToInt32::New(argv[1]);
    current->add(charCode);

    MFromCharCode *string = MFromCharCode::New(charCode);
    current->add(string);
    current->push(string);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineStrCharAt(uint32_t argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    if (getInlineReturnType() != MIRType_String)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 0) != MIRType_String)
        return InliningStatus_NotInlined;
    MIRType argType = getInlineArgType(argc, 1);
    if (argType != MIRType_Int32 && argType != MIRType_Double)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MInstruction *index = MToInt32::New(argv[1]);
    current->add(index);

    MStringLength *length = MStringLength::New(argv[0]);
    current->add(length);

    index = addBoundsCheck(index, length);

    // String.charAt(x) = String.fromCharCode(String.charCodeAt(x))
    MCharCodeAt *charCode = MCharCodeAt::New(argv[0], index);
    current->add(charCode);

    MFromCharCode *string = MFromCharCode::New(charCode);
    current->add(string);
    current->push(string);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineRegExpTest(uint32_t argc, bool constructing)
{
    if (argc != 1 || constructing)
        return InliningStatus_NotInlined;

    // TI can infer a NULL return type of regexp_test with eager compilation.
    if (CallResultEscapes(pc) && getInlineReturnType() != MIRType_Boolean)
        return InliningStatus_NotInlined;

    if (getInlineArgType(argc, 0) != MIRType_Object)
        return InliningStatus_NotInlined;
    if (getInlineArgType(argc, 1) != MIRType_String)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(argc, argv, current))
        return InliningStatus_Error;

    MInstruction *match = MRegExpTest::New(argv[0], argv[1]);
    current->add(match);
    current->push(match);
    if (!resumeAfter(match))
        return InliningStatus_Error;

    return InliningStatus_Inlined;
}

} // namespace ion
} // namespace js
